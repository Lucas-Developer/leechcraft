/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2011 Minh Ngo
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

#include <interfaces/iinfo.h>
#include <interfaces/ihavesettings.h>
#include <interfaces/media/iaudioscrobbler.h>
#include <interfaces/media/ialbumartprovider.h>
#include <interfaces/media/isimilarartists.h>
#include <interfaces/media/irecommendedartists.h>
#include <interfaces/media/iradiostationprovider.h>
#include <interfaces/media/irecentreleases.h>
#include <interfaces/media/iartistbiofetcher.h>
#include <interfaces/media/ieventsprovider.h>
#include <interfaces/media/ihypesprovider.h>

class QStandardItem;
class QStandardItemModel;

namespace LeechCraft
{
namespace Lastfmscrobble
{
	class Authenticator;
	class LastFMSubmitter;

	class Plugin : public QObject
				, public IInfo
				, public IHaveSettings
				, public Media::IAudioScrobbler
				, public Media::IAlbumArtProvider
				, public Media::ISimilarArtists
				, public Media::IRecommendedArtists
				, public Media::IRadioStationProvider
				, public Media::IRecentReleases
				, public Media::IArtistBioFetcher
				, public Media::IEventsProvider
				, public Media::IHypesProvider
	{
		Q_OBJECT
		Q_INTERFACES (IInfo
				IHaveSettings
				Media::IAudioScrobbler
				Media::IAlbumArtProvider
				Media::ISimilarArtists
				Media::IRecommendedArtists
				Media::IRadioStationProvider
				Media::IRecentReleases
				Media::IArtistBioFetcher
				Media::IEventsProvider
				Media::IHypesProvider)

		LC_PLUGIN_METADATA ("org.LeechCraft.LastFMScrobble")

		Util::XmlSettingsDialog_ptr XmlSettingsDialog_;

		Authenticator *Auth_;
		LastFMSubmitter *LFSubmitter_;

		ICoreProxy_ptr Proxy_;

		QStandardItemModel *RadioModel_;
		QStandardItem *RadioRoot_;
	public:
		void Init (ICoreProxy_ptr proxy);
		void SecondInit ();
		QByteArray GetUniqueID () const;
		QString GetName () const;
		QString GetInfo () const;
		void Release ();
		QIcon GetIcon () const;

		Util::XmlSettingsDialog_ptr GetSettingsDialog () const;

		bool SupportsFeature (Feature) const;
		QString GetServiceName () const;
		void NowPlaying (const Media::AudioInfo&);
		void SendBackdated (const BackdatedTracks_t&);
		void PlaybackStopped ();
		void LoveCurrentTrack ();
		void BanCurrentTrack ();

		QString GetAlbumArtProviderName () const;
		QFuture<AlbumArtResult_t> RequestAlbumArt (const Media::AlbumInfo& album) const;

		Media::IPendingSimilarArtists* GetSimilarArtists (const QString&, int);

		Media::IPendingSimilarArtists* RequestRecommended (int);

		Media::IRadioStation_ptr GetRadioStation (const QModelIndex&, const QString&);
		QList<QAbstractItemModel*> GetRadioListItems () const;
		void RefreshItems (const QList<QModelIndex>&);

		void RequestRecentReleases (int, bool);

		Media::IPendingArtistBio* RequestArtistBio (const QString&, bool);

		void UpdateRecommendedEvents ();
		void AttendEvent (qint64, Media::EventAttendType);

		bool SupportsHype (HypeType);
		void RequestHype (HypeType);
	private slots:
		void reloadRecommendedEvents ();
	signals:
		void gotRecentReleases (const QList<Media::AlbumRelease>&);

		void gotRecommendedEvents (const Media::EventInfos_t&);

		void gotHypedArtists (const QList<Media::HypedArtistInfo>&, Media::IHypesProvider::HypeType);
		void gotHypedTracks (const QList<Media::HypedTrackInfo>&, Media::IHypesProvider::HypeType);
	};
}
}
