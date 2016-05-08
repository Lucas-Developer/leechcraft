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

#include "servermessagessyncer.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <util/sll/either.h>
#include <util/sll/slotclosure.h>
#include <util/sll/urloperator.h>
#include <util/sll/parsejson.h>
#include <util/sll/prelude.h>
#include "vkconnection.h"
#include "vkaccount.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Murm
{
	namespace
	{
		const auto RequestSize = 200;
	}

	ServerMessagesSyncer::ServerMessagesSyncer (const QDateTime& since, VkAccount *acc, QObject *parent)
	: QObject { parent }
	, Since_ { since }
	, Acc_ { acc }
	{
		Iface_.reportStarted ();

		Request (IMessage::Direction::Out);
		Request (IMessage::Direction::In);
	}

	QFuture<IHaveServerHistory::DatedFetchResult_t> ServerMessagesSyncer::GetFuture ()
	{
		return Iface_.future ();
	}

	void ServerMessagesSyncer::Request (IMessage::Direction dir)
	{
		auto getter = [=] (const QString& key, const VkConnection::UrlParams_t& params)
		{
			const auto gracePeriod = 60 * 10;
			const auto secsDiff = Since_.secsTo (QDateTime::currentDateTime ()) + gracePeriod;

			QUrl url { "https://api.vk.com/method/messages.get" };
			Util::UrlOperator { url }
					("access_token", key)
					("out", dir == IMessage::Direction::Out ? "1" : "0")
					("count", RequestSize)
					("offset", Offset_)
					("time_offset", secsDiff);
			VkConnection::AddParams (url, params);

			const auto reply = Acc_->GetCoreProxy ()->
					GetNetworkAccessManager ()->get (QNetworkRequest { url });

			new Util::SlotClosure<Util::DeleteLaterPolicy>
			{
				[=] { HandleFinished (reply, dir); },
				reply,
				SIGNAL (finished ()),
				this
			};

			return reply;
		};

		Acc_->GetConnection ()->QueueRequest (getter);
	}

	void ServerMessagesSyncer::HandleFinished (QNetworkReply *reply, IMessage::Direction dir)
	{
		const auto& json = Util::ParseJson (reply, Q_FUNC_INFO);
		reply->deleteLater ();

		const auto itemsVar = json.toMap () ["response"].toMap () ["items"];
		if (itemsVar.type () != QVariant::List)
		{
			ReportError ("Unable to parse reply.");
			return;
		}

		const auto& itemsList = itemsVar.toList ();

		const auto& accId = Acc_->GetAccountID ();
		for (const auto& mapVar : itemsList)
		{
			const auto& map = mapVar.toMap ();

			const HistoryItem item
			{
				QDateTime::fromTime_t (map ["date"].toULongLong ()),
				dir,
				map ["body"].toString (),
				{},
				IMessage::Type::ChatMessage,
				{},
				IMessage::EscapePolicy::Escape
			};

			const auto& id = accId + QString::number (map ["user_id"].toLongLong ());
			Messages_ [id].VisibleName_ = id;
			Messages_ [id].Messages_ << item;
		}

		if (itemsList.size () == RequestSize)
		{
			Offset_ += RequestSize;
			Request (dir);
		}
		else
		{
			Dones_.insert (dir);
			CheckDone ();
		}
	}

	void ServerMessagesSyncer::CheckDone ()
	{
		if (Dones_.size () != 2)
			return;

		qDebug () << Q_FUNC_INFO
				<< Messages_.size ();

		for (auto& list : Messages_)
			std::sort (list.Messages_.begin (), list.Messages_.end (),
					Util::ComparingBy (&HistoryItem::Date_));

		const auto res = IHaveServerHistory::DatedFetchResult_t::Right (Messages_);
		Iface_.reportFinished (&res);

		deleteLater ();
	}

	void ServerMessagesSyncer::ReportError (const QString& err)
	{
		const auto res = IHaveServerHistory::DatedFetchResult_t::Left (err);
		Iface_.reportFinished (&res);

		deleteLater ();
	}
}
}
}