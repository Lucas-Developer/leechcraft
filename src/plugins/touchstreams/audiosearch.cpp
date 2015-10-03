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

#include "audiosearch.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <util/sll/queuemanager.h>
#include <util/sll/urloperator.h>
#include <util/svcauth/vkauthmanager.h>

namespace LeechCraft
{
namespace TouchStreams
{
	AudioSearch::AudioSearch (ICoreProxy_ptr proxy, const Media::AudioSearchRequest& query,
			Util::SvcAuth::VkAuthManager *mgr, Util::QueueManager *queue, QObject *parent)
	: QObject (parent)
	, Proxy_ (proxy)
	, Queue_ (queue)
	, AuthMgr_ (mgr)
	, Query_ (query)
	{
		connect (AuthMgr_,
				SIGNAL (gotAuthKey (QString)),
				this,
				SLOT (handleGotAuthKey (QString)));
		AuthMgr_->GetAuthKey ();
	}

	QObject* AudioSearch::GetQObject ()
	{
		return this;
	}

	QList<Media::IPendingAudioSearch::Result> AudioSearch::GetResults () const
	{
		return Result_;
	}

	void AudioSearch::handleGotAuthKey (const QString& key)
	{
		QUrl url ("https://api.vk.com/method/audio.search");
		Util::UrlOperator { url }
				("access_token", key)
				("q", Query_.FreeForm_);

		Queue_->Schedule ([this, url]
			{
				auto reply = Proxy_->GetNetworkAccessManager ()->get (QNetworkRequest (url));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleGotReply ()));
				connect (reply,
						SIGNAL (error (QNetworkReply::NetworkError)),
						this,
						SLOT (handleError ()));
			},
			this,
			Util::QueuePriority::High);
	}

	void AudioSearch::handleGotReply ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		const auto& data = reply->readAll ();
		std::istringstream istr (data.constData ());
		boost::property_tree::ptree pt;
		boost::property_tree::read_json (istr, pt);

		for (const auto& v : pt.get_child ("response"))
		{
			const auto& sub = v.second;
			if (sub.empty ())
				continue;

			Media::IPendingAudioSearch::Result result;
			try
			{
				result.Info_.Length_ = sub.get<qint32> ("duration");

				if (Query_.TrackLength_ > 0 && result.Info_.Length_ != Query_.TrackLength_)
				{
					qDebug () << Q_FUNC_INFO
							<< "skipping track due to track length mismatch"
							<< result.Info_.Length_
							<< Query_.TrackLength_;
					continue;
				}

				result.Info_.Artist_ = QString::fromUtf8 (sub.get<std::string> ("artist").c_str ());
				result.Info_.Title_ = QString::fromUtf8 (sub.get<std::string> ("title").c_str ());
				result.Source_ = QUrl::fromEncoded (sub.get<std::string> ("url").c_str ());
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to get props"
						<< e.what ();
				continue;
			}
			Result_ << result;
		}

		emit ready ();
		emit deleteLater ();
	}

	void AudioSearch::handleError ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		qWarning () << Q_FUNC_INFO
				<< reply->errorString ();
		emit error ();
		deleteLater ();
	}
}
}
