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

#pragma once

#include <memory>
#include <QObject>
#include <util/sll/visitor.h>
#include "accountthread.h"

namespace LeechCraft
{
namespace Snails
{
	class Account;
	class Storage;

	using AccountThread_ptr = std::shared_ptr<AccountThread>;

	enum class TaskPriority;

	class ThreadPool : public QObject
	{
		Account * const Acc_;
		Storage * const Storage_;
		const CertList_t CertList_;

		QList<AccountThread_ptr> ExistingThreads_;

		bool HitLimit_ = false;
		bool CheckingNext_ = false;

		QList<std::function<void (AccountThread*)>> Scheduled_;

		QList<std::function<void (AccountThread*)>> ThreadInitializers_;
	public:
		ThreadPool (const CertList_t&, Account*, Storage*);

		QFuture<EitherInvokeError_t<Util::Void>> TestConnectivity ();

		AccountThread* GetThread ();

		template<typename F, typename... Args>
		QFuture<WrapFunctionType_t<F, Args...>> Schedule (TaskPriority prio, const F& func, const Args&... args)
		{
			QFutureInterface<WrapFunctionType_t<F, Args...>> iface;

			auto runner = [=] (AccountThread *thread) mutable
					{
						iface.reportStarted ();
						PerformScheduledFunc (thread, iface, prio, func, args...);
					};

			switch (prio)
			{
			case TaskPriority::High:
				Scheduled_.prepend (runner);
				break;
			case TaskPriority::Low:
				Scheduled_.append (runner);
				break;
			}

			RunThreads ();

			return iface.future ();
		}

		template<typename F, typename... Args>
		void AddThreadInitializer (const F& func, const Args&... args)
		{
			auto runner = [=] (AccountThread *thread) { Util::Invoke (func, thread, args...); };
			ThreadInitializers_ << runner;

			for (const auto& thread : ExistingThreads_)
				runner (thread.get ());
		}
	private:
		template<typename FutureInterface, typename F, typename... Args>
		void PerformScheduledFunc (AccountThread *thread, FutureInterface iface, TaskPriority prio, const F& func, const Args&... args)
		{
			Util::Sequence (nullptr, thread->Schedule (prio, func, args...)) >>
					[=] (auto result) mutable
					{
						if (result.IsRight ())
						{
							iface.reportFinished (&result);
							return;
						}

						Util::Visit (result.GetLeft (),
								[=, &iface] (const vmime::exceptions::authentication_error& e)
								{
									const auto& respStr = QString::fromStdString (e.response ());
									if (respStr.contains ("simultaneous"))
									{
										qWarning () << Q_FUNC_INFO
												<< "seems like a thread has died, rescheduling...";
										HandleThreadOverflow (thread);
										PerformScheduledFunc (GetNextThread (), iface, prio, func, args...);
									}
									else
										iface.reportFinished (&result);
								},
								[=, &iface] (auto)
								{
									iface.reportFinished (&result);
								});
					};
		}

		void RunThreads ();

		AccountThread_ptr CreateThread ();

		void RunScheduled (AccountThread*);
		AccountThread* GetNextThread ();

		void HandleThreadOverflow (AccountThread*);
		void HandleThreadOverflow (const AccountThread_ptr&);
	};
}
}
