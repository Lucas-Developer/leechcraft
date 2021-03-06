/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
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

#include "pmutils.h"
#include <QProcess>
#include <QtDebug>
#include <util/sll/slotclosure.h>
#include <util/sll/unreachable.h>
#include <util/threads/futures.h>

namespace LeechCraft
{
namespace Liznoo
{
namespace PowerActions
{
	namespace
	{
		QString State2Str (Platform::State state)
		{
			switch (state)
			{
			case Platform::State::Suspend:
				return "suspend";
			case Platform::State::Hibernate:
				return "hibernate";
			}

			Util::Unreachable ();
		}

		QString MakeErrMsg (QProcess *process)
		{
			const auto& origMsg = process->errorString ();
			switch (process->error ())
			{
			case QProcess::ProcessError::FailedToStart:
				return PMUtils::tr ("%1 failed to start. "
						"Probably %2 is not installed? Original message: %3.")
						.arg ("pm-is-supported")
						.arg ("pm-utils")
						.arg (origMsg);
			default:
				return origMsg;
			}
		}
	}

	QFuture<bool> PMUtils::IsAvailable ()
	{
		QFutureInterface<bool> iface;
		iface.reportStarted ();

		auto process = new QProcess { this };
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[process, iface] () mutable
			{
				Util::ReportFutureResult (iface, false);
				process->deleteLater ();
			},
			process,
			SIGNAL (error (QProcess::ProcessError)),
			process
		};
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[process, iface] () mutable
			{
				Util::ReportFutureResult (iface,
						process->exitStatus () == QProcess::NormalExit);
				process->deleteLater ();
			},
			process,
			SIGNAL (finished (int, QProcess::ExitStatus)),
			process
		};
		process->start ("pm-is-supported");

		return iface.future ();
	}

	QFuture<Platform::QueryChangeStateResult> PMUtils::CanChangeState (State state)
	{
		QFutureInterface<QueryChangeStateResult> iface;
		iface.reportStarted ();

		auto process = new QProcess { this };
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[process, iface] () mutable
			{
				const QueryChangeStateResult result { false, MakeErrMsg (process) };
				iface.reportFinished (&result);
				process->deleteLater ();
			},
			process,
			SIGNAL (error (QProcess::ProcessError)),
			process
		};
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[process, iface] () mutable
			{
				const QueryChangeStateResult result { process->exitCode () == 0, {} };
				iface.reportFinished (&result);
				process->deleteLater ();
			},
			process,
			SIGNAL (finished (int, QProcess::ExitStatus)),
			process
		};
		process->start ("pm-is-supported", { "--" + State2Str (state) });

		return iface.future ();
	}

	void PMUtils::ChangeState (State state)
	{
		const auto& app = "pm-" + State2Str (state);

		QProcess::startDetached ("/usr/sbin/" + app);
	}
}
}
}

