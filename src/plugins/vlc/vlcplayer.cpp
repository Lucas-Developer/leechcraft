/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Vladislav Tyulbashev
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

#include <QVBoxLayout>
#include <QPushButton>
#include <QTime>
#include <QMouseEvent>
#include <QWidget>
#include <QTime>
#include <QTimer>
#include <QSizePolicy>
#include <QEventLoop>
#include <QTimeLine>
#include <QDebug>
#include "vlcplayer.h"

namespace 
{
	QTime convertTime (libvlc_time_t t) 
	{
		return QTime (t / 1000 / 60 / 60, t / 1000 / 60 % 60, t / 1000 % 60, t % 1000);
	}
	
	void sleep (int ms)
	{
			QEventLoop loop;
			QTimer::singleShot (ms, &loop, SLOT (quit ()));
			loop.exec ();
	}
}

namespace LeechCraft
{
namespace vlc
{
	VlcPlayer::VlcPlayer (QWidget *parent)
	: QObject (parent)
	, M_ (nullptr)
	, Parent_ (parent)
	, DVD_ (false)
	{
		const char * const vlc_args[] = 
		{
			"--ffmpeg-hw"
		};

		VlcInstance_ = std::shared_ptr<libvlc_instance_t> (libvlc_new (1, vlc_args), libvlc_release);
		Mp_ = std::shared_ptr<libvlc_media_player_t> (libvlc_media_player_new (VlcInstance_.get ()), libvlc_media_player_release);
		libvlc_media_player_set_xwindow (Mp_.get (), parent->winId ());
	}
	
	void VlcPlayer::setUrl (const QUrl &url) 
	{
		Subtitles_.clear ();
		libvlc_media_player_stop (Mp_.get ());
		
		DVD_ = url.scheme () == "dvd";
		M_.reset (libvlc_media_new_location (VlcInstance_.get (), url.toEncoded ()), libvlc_media_release);
		
		libvlc_media_player_set_media (Mp_.get (), M_.get ());
		libvlc_media_player_play (Mp_.get ());
		
		LastMedia_ = url;
	}
	
	void VlcPlayer::addUrl(const QUrl &url)
	{
		Freeze ();
		M_.reset (libvlc_media_new_location (VlcInstance_.get (), LastMedia_.toEncoded ()), libvlc_media_release);
		libvlc_media_add_option (M_.get (), ":input-slave=" + url.toEncoded ());
		libvlc_media_player_set_media (Mp_.get (), M_.get ());
		UnFreeze ();		
	}
	
	void VlcPlayer::ClearAll () 
	{	
	}
	
	bool VlcPlayer::NowPlaying () const
	{
		return libvlc_media_player_is_playing (Mp_.get ());
	}
	
	double VlcPlayer::GetPosition () const
	{
		return libvlc_media_player_get_position (Mp_.get ());
	}
	
	void VlcPlayer::togglePlay () 
	{
		if (NowPlaying ())
			libvlc_media_player_pause (Mp_.get ());
		else
			libvlc_media_player_play (Mp_.get ());
	}
	
	void VlcPlayer::stop () 
	{
		libvlc_media_player_stop (Mp_.get ());
	}
	
	void VlcPlayer::changePosition (double pos)
	{
		if (libvlc_media_player_get_media (Mp_.get ()))
			libvlc_media_player_set_position (Mp_.get (), pos);
	}	
	
	QTime VlcPlayer::GetFullTime () const 
	{
		if (libvlc_media_player_get_media (Mp_.get ()))
			return convertTime (libvlc_media_player_get_length (Mp_.get ()));
		else
			return convertTime (0);
	}
	
	QTime VlcPlayer::GetCurrentTime () const
	{
		if (libvlc_media_player_get_media (Mp_.get ()))
			return convertTime (libvlc_media_player_get_time (Mp_.get ()));
		else
			return convertTime (0);
	}
	
	void VlcPlayer::Freeze ()
	{
		FreezePlayingMedia_ = libvlc_media_player_get_media (Mp_.get ());
		if (FreezePlayingMedia_) 
		{
			FreezeCur_ = libvlc_media_player_get_time (Mp_.get ());
			FreezeAudio_ = GetCurrentAudioTrack ();
			FreezeSubtitle_ = GetCurrentSubtitle ();
		}
		
		FreezeIsPlaying_ = libvlc_media_player_is_playing (Mp_.get ()); 
		FreezeDVD_ = DVD_ && libvlc_media_player_get_length (Mp_.get ()) > 60 * 10 * 1000;
		
		libvlc_media_player_stop (Mp_.get ());
	}
	
	void VlcPlayer::switchWidget (QWidget *widget) 
	{
		Freeze ();
		libvlc_media_player_set_xwindow (Mp_.get (), widget->winId ());
		UnFreeze ();
	}
	
	void VlcPlayer::UnFreeze()
	{
		libvlc_media_player_play (Mp_.get ());
		
		WaitForPlaying ();
		if (FreezeDVD_)
		{
			libvlc_media_player_navigate (Mp_.get (), libvlc_navigate_activate);
			sleep (150);
		}
		
		if (FreezePlayingMedia_ && (!DVD_ || FreezeDVD_))
		{
			libvlc_media_player_set_time (Mp_.get (), FreezeCur_);
			setAudioTrack (FreezeAudio_);
			setSubtitle (FreezeSubtitle_);
		}
		
		if (!FreezeIsPlaying_)
			libvlc_media_player_pause (Mp_.get ());
		
		for (int i = 0; i < Subtitles_.size (); i++)
			libvlc_video_set_subtitle_file (Mp_.get (), Subtitles_[i].toUtf8 ());
	}
	
	QWidget* VlcPlayer::GetParent () const
	{
		return Parent_;
	}
	
	std::shared_ptr<libvlc_media_player_t> VlcPlayer::GetPlayer () const
	{
		return Mp_;
	}
	
	int VlcPlayer::GetAudioTracksNumber () const
	{
		return libvlc_audio_get_track_count (Mp_.get ());
	}
	
	int VlcPlayer::GetCurrentAudioTrack () const
	{
		return libvlc_audio_get_track (Mp_.get ());
	}
	
	void VlcPlayer::setAudioTrack (int track)
	{
		libvlc_audio_set_track (Mp_.get (), track);
	}
	
	QString VlcPlayer::GetAudioTrackDescription (int track) const
	{
		libvlc_track_description_t *t = GetTrack (libvlc_audio_get_track_description (Mp_.get ()), track);
		return QString (t->psz_name);
	}
	
	int VlcPlayer::GetAudioTrackId(int track) const
	{
		libvlc_track_description_t *t = GetTrack (libvlc_audio_get_track_description (Mp_.get ()), track);
		return t->i_id;
	}
	
	int VlcPlayer::GetSubtitlesNumber () const
	{
		return libvlc_video_get_spu_count (Mp_.get ());
	}
	
	void VlcPlayer::AddSubtitles (QString file)
	{
		libvlc_video_set_subtitle_file (Mp_.get (), file.toUtf8 ());
		Subtitles_ << file;
	}

	int VlcPlayer::GetCurrentSubtitle () const
	{
		return libvlc_video_get_spu (Mp_.get ());
	}
	
	QString VlcPlayer::GetSubtitleDescription (int track) const
	{
		libvlc_track_description_t *t = GetTrack (libvlc_video_get_spu_description (Mp_.get ()), track);	
		return QString (t->psz_name);
	}
	
	int VlcPlayer::GetSubtitleId(int track) const
	{
		libvlc_track_description_t *t = GetTrack (libvlc_video_get_spu_description (Mp_.get ()), track);
		return t->i_id;
	}

	void VlcPlayer::setSubtitle (int track)
	{
		libvlc_video_set_spu (Mp_.get (), track);
	}
	
	libvlc_track_description_t* VlcPlayer::GetTrack(libvlc_track_description_t *t, int track) const
	{
		for (int i = 0; i < track; i++)
			t = t->p_next;
		
		return t;
	}

	void VlcPlayer::DVDNavigate (unsigned nav)
	{
		libvlc_media_player_navigate (Mp_.get (), nav);
	}

	void VlcPlayer::dvdNavigateDown ()
	{
		libvlc_media_player_navigate (Mp_.get (), libvlc_navigate_down);
	}

	void VlcPlayer::dvdNavigateUp ()
	{
		libvlc_media_player_navigate (Mp_.get (), libvlc_navigate_up);
	}
	
	void VlcPlayer::dvdNavigateRight ()
	{
		libvlc_media_player_navigate (Mp_.get (), libvlc_navigate_right);
	}
	
	void VlcPlayer::dvdNavigateLeft ()
	{
		libvlc_media_player_navigate (Mp_.get (), libvlc_navigate_left);
	}
	
	void VlcPlayer::dvdNavigateEnter ()
	{
		libvlc_media_player_navigate (Mp_.get (), libvlc_navigate_activate);
	}

	void VlcPlayer::WaitForPlaying () const
	{
		QTimeLine line;
		line.start ();
		while (!NowPlaying ()) 
		{
			sleep (5);
			if (line.currentTime () > 1000)
			{
				qWarning () << Q_FUNC_INFO << "timeout";
				break;
			}
		}
	}
}
}
