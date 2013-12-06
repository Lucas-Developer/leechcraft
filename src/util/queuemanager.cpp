/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "queuemanager.h"
#include <QTimer>

namespace LeechCraft
{
namespace Util
{
	QueueManager::QueueManager (int timeout, QObject *parent)
	: QObject (parent)
	, Timeout_ (timeout)
	, ReqTimer_ (new QTimer (this))
	, Paused_ (false)
	{
		ReqTimer_->setSingleShot (true);
		connect (ReqTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (exec ()));
	}

	void QueueManager::Schedule (std::function<void ()> f, QObject *dep, QueuePriority prio)
	{
		const auto& now = QDateTime::currentDateTime ();

		if (prio == QueuePriority::High)
			Queue_.prepend ({ f, dep ? OptionalTracker_t { dep } : OptionalTracker_t () });
		else
			Queue_.append ({ f, dep ? OptionalTracker_t { dep } : OptionalTracker_t () });

		const auto diff = LastRequest_.msecsTo (now);
		if (diff >= Timeout_)
			exec ();
		else if (Queue_.size () == 1)
			ReqTimer_->start (Timeout_ - diff);
	}

	void QueueManager::Clear ()
	{
		Queue_.clear ();
	}

	void QueueManager::Pause ()
	{
		Paused_ = true;
		ReqTimer_->stop ();
	}

	void QueueManager::Resume ()
	{
		Paused_ = false;
		ReqTimer_->start (Timeout_);
	}

	void QueueManager::exec ()
	{
		if (Queue_.isEmpty ())
			return;

		if (Paused_)
			return;

		const auto& pair = Queue_.takeFirst ();
		if (pair.second && !*pair.second)
		{
			exec ();
			return;
		}

		pair.first ();
		LastRequest_ = QDateTime::currentDateTime ();

		if (!Queue_.isEmpty ())
			ReqTimer_->start (Timeout_);
	}
}
}
