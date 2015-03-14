/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2012  Georg Rudoy
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

#include "pendingdisco.h"
#include <memory>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QDomDocument>
#include <QtDebug>
#include <QPointer>
#include <util/sll/queuemanager.h>
#include <util/util.h>
#include "artistlookup.h"

namespace LeechCraft
{
namespace MusicZombie
{
	PendingDisco::PendingDisco (Util::QueueManager *queue, const QString& artist,
			const QString& release, QNetworkAccessManager *nam, QObject *parent)
	: QObject (parent)
	, ReleaseName_ (release.toLower ())
	, Queue_ (queue)
	, NAM_ (nam)
	{
		Queue_->Schedule ([this, artist, nam] () -> void
			{
				auto idLookup = new ArtistLookup (artist, nam, this);
				connect (idLookup,
						SIGNAL(gotID (QString)),
						this,
						SLOT (handleGotID (QString)));
				connect (idLookup,
						SIGNAL (replyError ()),
						this,
						SLOT (handleIDError ()));
				connect (idLookup,
						SIGNAL (networkError ()),
						this,
						SLOT (handleIDError ()));
			}, this);
	}

	QObject* PendingDisco::GetQObject ()
	{
		return this;
	}

	QList<Media::ReleaseInfo> PendingDisco::GetReleases () const
	{
		return Releases_;
	}

	void PendingDisco::handleGotID (const QString& id)
	{
		const auto urlStr = "http://musicbrainz.org/ws/2/release?limit=100&inc=recordings+release-groups&status=official&artist=" + id;

		Queue_->Schedule ([this, urlStr] () -> void
			{
				auto reply = NAM_->get (QNetworkRequest (QUrl (urlStr)));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleLookupFinished ()));
				connect (reply,
						SIGNAL (error (QNetworkReply::NetworkError)),
						this,
						SLOT (handleLookupError ()));
			}, this);
	}

	void PendingDisco::handleIDError ()
	{
		qWarning () << Q_FUNC_INFO
				<< "error getting MBID";
		emit error (tr ("Error getting artist MBID."));
		deleteLater ();
	}

	namespace
	{
		void ParseMediumList (Media::ReleaseInfo& release, QDomElement mediumElem)
		{
			while (!mediumElem.isNull ())
			{
				auto trackElem = mediumElem.firstChildElement ("track-list").firstChildElement ("track");

				QList<Media::ReleaseTrackInfo> tracks;
				while (!trackElem.isNull ())
				{
					const int num = trackElem.firstChildElement ("number").text ().toInt ();

					const auto& recElem = trackElem.firstChildElement ("recording");
					const auto& title = recElem.firstChildElement ("title").text ();
					const int length = recElem.firstChildElement ("length").text ().toInt () / 1000;

					tracks.push_back ({ num, title, length });
					trackElem = trackElem.nextSiblingElement ("track");
				}

				release.TrackInfos_ << tracks;

				mediumElem = mediumElem.nextSiblingElement ("medium");
			}
		}

		Media::ReleaseInfo::Type GetReleaseType (const QDomElement& releaseElem)
		{
			const auto& elem = releaseElem.firstChildElement ("release-group");
			if (elem.isNull ())
			{
				qWarning () << Q_FUNC_INFO
						<< "null element";
				return Media::ReleaseInfo::Type::Other;
			}

			const auto& type = elem.attribute ("type");

			static const auto& map = Util::MakeMap<QString, Media::ReleaseInfo::Type> ({
						{ "Album", Media::ReleaseInfo::Type::Standard },
						{ "EP", Media::ReleaseInfo::Type::EP },
						{ "Single", Media::ReleaseInfo::Type::Single },
						{ "Compilation", Media::ReleaseInfo::Type::Compilation },
						{ "Live", Media::ReleaseInfo::Type::Live },
						{ "Soundtrack", Media::ReleaseInfo::Type::Soundtrack },
						{ "Other", Media::ReleaseInfo::Type::Other }
					});

			if (map.contains (type))
				return map.value (type);

			qWarning () << Q_FUNC_INFO
					<< "unknown type:"
					<< type;
			return Media::ReleaseInfo::Type::Other;
		}
	}

	void PendingDisco::handleLookupFinished ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		const auto& data = reply->readAll ();
		QDomDocument doc;
		if (!doc.setContent (data))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to parse"
					<< data;
			emit error (tr ("Unable to parse MusicBrainz reply."));
			deleteLater ();
		}

		QMap<QString, QMap<QString, Media::ReleaseInfo>> infos;

		auto releaseElem = doc
				.documentElement ()
				.firstChildElement ("release-list")
				.firstChildElement ("release");
		while (!releaseElem.isNull ())
		{
			std::shared_ptr<void> guard (nullptr,
					[&releaseElem] (void*)
						{ releaseElem = releaseElem.nextSiblingElement ("release"); });

			auto elemText = [&releaseElem] (const QString& sub)
			{
				return releaseElem.firstChildElement (sub).text ();
			};

			if (elemText ("status") != "Official")
				continue;

			const auto& dateStr = elemText ("date");
			const int dashPos = dateStr.indexOf ('-');
			const int date = (dashPos > 0 ? dateStr.left (dashPos) : dateStr).toInt ();
			if (date < 1000)
				continue;

			const auto& title = elemText ("title");
			if (!ReleaseName_.isEmpty () && title.toLower () != ReleaseName_)
				continue;

			Media::ReleaseInfo info
			{
				releaseElem.attribute ("id"),
				title,
				date,
				GetReleaseType (releaseElem),
				{}
			};
			const auto& mediumListElem = releaseElem.firstChildElement ("medium-list");
			ParseMediumList (info, mediumListElem.firstChildElement ("medium"));

			infos [title] [elemText ("country")] = info;
		}

		for (const auto& countries : infos)
		{
			const auto& release = countries.contains ("US") ?
					countries ["US"] :
					countries.values ().first ();
			Releases_ << release;
		}

		std::sort (Releases_.begin (), Releases_.end (),
				[] (decltype (Releases_.at (0)) left, decltype (Releases_.at (0)) right)
					{ return left.Year_ < right.Year_; });

		emit ready ();
		deleteLater ();
	}

	void PendingDisco::handleLookupError ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		qWarning () << Q_FUNC_INFO
				<< "error looking stuff up"
				<< reply->errorString ();
		emit error (tr ("Error performing artist lookup: %1.")
					.arg (reply->errorString ()));
	}
}
}
