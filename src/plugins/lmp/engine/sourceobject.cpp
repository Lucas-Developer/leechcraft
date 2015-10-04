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

#include "sourceobject.h"
#include <memory>
#include <atomic>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <QtDebug>
#include <QTimer>
#include <QThread>
#include "util/lmp/gstutil.h"
#include "audiosource.h"
#include "path.h"
#include "../gstfix.h"
#include "../core.h"
#include "../xmlsettingsmanager.h"

Q_DECLARE_METATYPE (GstMessage*);
Q_DECLARE_METATYPE (GstMessage_ptr);

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		gboolean CbAboutToFinish (GstElement*, gpointer data)
		{
			static_cast<SourceObject*> (data)->HandleAboutToFinish ();
			return true;
		}

		gboolean CbSourceChanged (GstElement*, GParamSpec*, gpointer data)
		{
			static_cast<SourceObject*> (data)->SetupSource ();
			return true;
		}
	}

	class MsgPopThread : public QThread
	{
		GstBus * const Bus_;
		SourceObject * const SourceObj_;
		std::atomic_bool ShouldStop_;
		const double Multiplier_;

		QMutex& BusDrainMutex_;
		QWaitCondition& BusDrainWC_;
	public:
		MsgPopThread (GstBus*, SourceObject*, double, QMutex&, QWaitCondition&);
		~MsgPopThread ();

		void Stop ();
	protected:
		void run ();
	};

	MsgPopThread::MsgPopThread (GstBus *bus, SourceObject *obj, double multiplier, QMutex& bdMutex, QWaitCondition& bdWC)
	: QThread (obj)
	, Bus_ (bus)
	, SourceObj_ (obj)
	, ShouldStop_ (false)
	, Multiplier_ (multiplier)
	, BusDrainMutex_ (bdMutex)
	, BusDrainWC_ (bdWC)
	{
	}

	MsgPopThread::~MsgPopThread ()
	{
		gst_object_unref (Bus_);
	}

	void MsgPopThread::Stop ()
	{
		ShouldStop_.store (true, std::memory_order_relaxed);
	}

	void MsgPopThread::run ()
	{
		while (!ShouldStop_.load (std::memory_order_relaxed))
		{
			msleep (3);
			const auto msg = gst_bus_timed_pop (Bus_, Multiplier_ * GST_SECOND);
			if (!msg)
				continue;

			QMetaObject::invokeMethod (SourceObj_,
					"handleMessage",
					Qt::QueuedConnection,
					Q_ARG (GstMessage_ptr, std::shared_ptr<GstMessage> (msg, gst_message_unref)));

			if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR)
			{
				BusDrainMutex_.lock ();
				BusDrainWC_.wait (&BusDrainMutex_);
				BusDrainMutex_.unlock ();

				qDebug () << "bus drained, continuing";
			}
		}
	}

	SourceObject::SourceObject (Category cat, QObject *parent)
	: QObject (parent)
#if GST_VERSION_MAJOR < 1
	, Dec_ (gst_element_factory_make ("playbin2", "play"))
#else
	, Dec_ (gst_element_factory_make ("playbin", "play"))
#endif
	, Path_ (nullptr)
	, IsSeeking_ (false)
	, LastCurrentTime_ (-1)
	, PrevSoupRank_ (0)
	, PopThread_ (new MsgPopThread (gst_pipeline_get_bus (GST_PIPELINE (Dec_)),
				this,
				cat == Category::Notification ? 0.05 : 1,
				BusDrainMutex_,
				BusDrainWC_))
	, OldState_ (SourceState::Stopped)
	{
		g_signal_connect (Dec_, "about-to-finish", G_CALLBACK (CbAboutToFinish), this);
		g_signal_connect (Dec_, "notify::source", G_CALLBACK (CbSourceChanged), this);

		qRegisterMetaType<GstMessage*> ("GstMessage*");
		qRegisterMetaType<GstMessage_ptr> ("GstMessage_ptr");

		qRegisterMetaType<AudioSource> ("AudioSource");

		auto timer = new QTimer (this);
		connect (timer,
				SIGNAL (timeout ()),
				this,
				SLOT (handleTick ()));
		timer->start (1000);

		gst_bus_set_sync_handler (gst_pipeline_get_bus (GST_PIPELINE (Dec_)),
				[] (GstBus *bus, GstMessage *msg, gpointer udata)
				{
					return static_cast<GstBusSyncReply> (static_cast<SourceObject*> (udata)->
								HandleSyncMessage (bus, msg));
				},
#if GST_VERSION_MAJOR < 1
				this);
#else
				this,
				nullptr);
#endif

		PopThread_->start (QThread::LowestPriority);
	}

	SourceObject::~SourceObject ()
	{
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_NULL);

		PopThread_->Stop ();
		PopThread_->wait (1100);
		if (PopThread_->isRunning ())
			PopThread_->terminate ();

		gst_object_unref (Dec_);
	}

	QObject* SourceObject::GetQObject ()
	{
		return this;
	}

	bool SourceObject::IsSeekable () const
	{
		std::shared_ptr<GstQuery> query (gst_query_new_seeking (GST_FORMAT_TIME), gst_query_unref);

		if (!gst_element_query (GST_ELEMENT (Dec_), query.get ()))
			return false;

		gboolean seekable = false;
		GstFormat format;
		gint64 start = 0, stop = 0;
		gst_query_parse_seeking (query.get (), &format, &seekable, &start, &stop);
		return seekable;
	}

	SourceState SourceObject::GetState () const
	{
		return OldState_;
	}

	void SourceObject::SetState (SourceState state)
	{
		if (state == OldState_)
			return;

		switch (state)
		{
		case SourceState::Stopped:
			Stop ();
			break;
		case SourceState::Paused:
			Pause ();
			break;
		case SourceState::Buffering:
			qWarning () << Q_FUNC_INFO
					<< "`buffering` is quite a bad state to be in, falling through to Playing";
		case SourceState::Playing:
			Play ();
			break;
		default:
			qWarning () << Q_FUNC_INFO
					<< "erroneous state"
					<< static_cast<int> (state);
			break;
		}
	}

	QString SourceObject::GetErrorString () const
	{
		return {};
	}

	QString SourceObject::GetMetadata (Metadata field) const
	{
		switch (field)
		{
		case Metadata::Artist:
			return Metadata_ ["artist"];
		case Metadata::Album:
			return Metadata_ ["album"];
		case Metadata::Title:
			return Metadata_ ["title"];
		case Metadata::Genre:
			return Metadata_ ["genre"];
		case Metadata::Tracknumber:
			return Metadata_ ["tracknumber"];
		case Metadata::NominalBitrate:
			return Metadata_ ["bitrate"];
		case Metadata::MinBitrate:
			return Metadata_ ["minimum-bitrate"];
		case Metadata::MaxBitrate:
			return Metadata_ ["maximum-bitrate"];
		}

		qWarning () << Q_FUNC_INFO
				<< "unknown field"
				<< static_cast<int> (field);

		return {};
	}

	qint64 SourceObject::GetCurrentTime ()
	{
		if (GetState () != SourceState::Paused)
		{
			auto format = GST_FORMAT_TIME;
			gint64 position = 0;
#if GST_VERSION_MAJOR >= 1
			gst_element_query_position (GST_ELEMENT (Dec_), format, &position);
#else
			gst_element_query_position (GST_ELEMENT (Dec_), &format, &position);
#endif
			LastCurrentTime_ = position;
		}
		return LastCurrentTime_ / GST_MSECOND;
	}

	qint64 SourceObject::GetRemainingTime () const
	{
		auto format = GST_FORMAT_TIME;
		gint64 duration = 0;
#if GST_VERSION_MAJOR >= 1
		if (!gst_element_query_duration (GST_ELEMENT (Dec_), format, &duration))
#else
		if (!gst_element_query_duration (GST_ELEMENT (Dec_), &format, &duration))
#endif
			return -1;

		return (duration - LastCurrentTime_) / GST_MSECOND;
	}

	qint64 SourceObject::GetTotalTime () const
	{
		auto format = GST_FORMAT_TIME;
		gint64 duration = 0;
#if GST_VERSION_MAJOR >= 1
		if (gst_element_query_duration (GST_ELEMENT (Dec_), format, &duration))
#else
		if (gst_element_query_duration (GST_ELEMENT (Dec_), &format, &duration))
#endif
			return duration / GST_MSECOND;
		return -1;
	}

	void SourceObject::Seek (qint64 pos)
	{
		if (!IsSeekable ())
			return;

		if (OldState_ == SourceState::Playing)
			IsSeeking_ = true;

		gst_element_seek (GST_ELEMENT (Dec_), 1.0, GST_FORMAT_TIME,
				GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, pos * GST_MSECOND,
				GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

		LastCurrentTime_ = pos * GST_MSECOND;
	}

	AudioSource SourceObject::GetActualSource () const
	{
		return ActualSource_;
	}

	AudioSource SourceObject::GetCurrentSource () const
	{
		return CurrentSource_;
	}

	namespace
	{
		uint SetSoupRank (uint rank)
		{
			const auto factory = gst_element_factory_find ("souphttpsrc");
			if (!factory)
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot find soup factory";
				return 0;
			}

			const auto oldRank = gst_plugin_feature_get_rank (GST_PLUGIN_FEATURE (factory));
			gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), rank);
#if GST_VERSION_MAJOR >= 1
			gst_registry_add_feature (gst_registry_get (), GST_PLUGIN_FEATURE (factory));
#else
			gst_registry_add_feature (gst_registry_get_default (), GST_PLUGIN_FEATURE (factory));
#endif


			return oldRank;
		}

		uint GetRank (const char *name)
		{
			const auto factory = gst_element_factory_find (name);
			if (!factory)
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot find factory"
						<< name;
				return 0;
			}

			return gst_plugin_feature_get_rank (GST_PLUGIN_FEATURE (factory));
		}
	}

	void SourceObject::SetCurrentSource (const AudioSource& source)
	{
		IsSeeking_ = false;

		CurrentSource_ = source;

		Metadata_.clear ();

		if (source.ToUrl ().scheme ().startsWith ("http"))
			PrevSoupRank_ = SetSoupRank (G_MAXINT / 2);

		auto path = source.ToUrl ().toEncoded ();
		g_object_set (G_OBJECT (Dec_), "uri", path.constData (), nullptr);

		NextSource_.Clear ();
	}

	void SourceObject::PrepareNextSource (const AudioSource& source)
	{
		NextSrcMutex_.lock ();

		qDebug () << Q_FUNC_INFO << source.ToUrl ();
		NextSource_ = source;

		NextSrcWC_.wakeAll ();
		NextSrcMutex_.unlock ();

		Metadata_.clear ();
	}

	void SourceObject::Play ()
	{
		if (CurrentSource_.IsEmpty ())
		{
			qDebug () << Q_FUNC_INFO
					<< "current source is invalid, setting next one";
			if (NextSource_.IsEmpty ())
				return;

			SetCurrentSource (NextSource_);
		}

		if (CurrentSource_.ToUrl ().scheme ().startsWith ("http"))
			PrevSoupRank_ = SetSoupRank (G_MAXINT / 2);

		gst_element_set_state (Path_->GetPipeline (), GST_STATE_PLAYING);
	}

	void SourceObject::Pause ()
	{
		if (!IsSeekable ())
			Stop ();
		else
			gst_element_set_state (Path_->GetPipeline (), GST_STATE_PAUSED);
	}

	void SourceObject::Stop ()
	{
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_READY);
		Seek (0);
	}

	void SourceObject::Clear ()
	{
		ClearQueue ();
		CurrentSource_.Clear ();
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_READY);
	}

	void SourceObject::ClearQueue ()
	{
		NextSource_.Clear ();
	}

	void SourceObject::HandleAboutToFinish ()
	{
		qDebug () << Q_FUNC_INFO;
		auto timeoutIndicator = std::make_shared<std::atomic_bool> (false);

		NextSrcMutex_.lock ();
		if (NextSource_.IsEmpty ())
		{
			emit aboutToFinish (timeoutIndicator);
			NextSrcWC_.wait (&NextSrcMutex_, 500);
		}
		qDebug () << "wait finished; next source:" << NextSource_.ToUrl ()
				<< "; current source:" << CurrentSource_.ToUrl ();

		std::shared_ptr<void> mutexGuard (nullptr,
				[this] (void*) { NextSrcMutex_.unlock (); });

		if (NextSource_.IsEmpty ())
		{
			*timeoutIndicator = true;
			qDebug () << Q_FUNC_INFO
					<< "no next source set, will stop playing";
			return;
		}

		SetCurrentSource (NextSource_);
	}

	void SourceObject::SetupSource ()
	{
		GstElement *src;
		g_object_get (Dec_, "source", &src, nullptr);

		if (!CurrentSource_.ToUrl ().scheme ().startsWith ("http"))
			return;

		std::shared_ptr<void> soupRankGuard (nullptr,
				[&] (void*) -> void
				{
					if (PrevSoupRank_)
					{
						SetSoupRank (PrevSoupRank_);
						PrevSoupRank_ = 0;
					}
				});

		if (!g_object_class_find_property (G_OBJECT_GET_CLASS (src), "user-agent"))
		{
			qDebug () << Q_FUNC_INFO
					<< "user-agent property not found for"
					<< CurrentSource_.ToUrl ()
					<< (QString ("|") + G_OBJECT_TYPE_NAME (src) + "|")
					<< "soup rank:"
					<< GetRank ("souphttpsrc")
					<< "webkit rank:"
					<< GetRank ("webkitwebsrc");
			return;
		}

		const auto& str = QString ("LeechCraft LMP/%1 (%2)")
				.arg (Core::Instance ().GetProxy ()->GetVersion ())
				.arg (gst_version_string ());
		qDebug () << Q_FUNC_INFO
				<< "setting user-agent to"
				<< str;
		g_object_set (src, "user-agent", str.toUtf8 ().constData (), nullptr);
	}

	void SourceObject::AddToPath (Path *path)
	{
		path->SetPipeline (Dec_);
		Path_ = path;
	}

	void SourceObject::SetSink (GstElement *bin)
	{
		g_object_set (GST_OBJECT (Dec_), "audio-sink", bin, nullptr);
	}

	void SourceObject::AddSyncHandler (const SyncHandler_f& handler, QObject *dependent)
	{
		SyncHandlers_.AddHandler (handler, dependent);
	}

	void SourceObject::AddAsyncHandler (const AsyncHandler_f& handler, QObject *dependent)
	{
		AsyncHandlers_.AddHandler (handler, dependent);
	}

	void SourceObject::HandleErrorMsg (GstMessage *msg)
	{
		GError *gerror = nullptr;
		gchar *debug = nullptr;
		gst_message_parse_error (msg, &gerror, &debug);

		const auto& msgStr = QString::fromUtf8 (gerror->message);
		const auto& debugStr = QString::fromUtf8 (debug);

		const auto code = gerror->code;
		const auto domain = gerror->domain;

		g_error_free (gerror);
		g_free (debug);

		// GStreamer is utter crap
		if (domain == GST_RESOURCE_ERROR &&
				code == GST_RESOURCE_ERROR_NOT_FOUND &&
				msgStr == "Cancelled")
			return;

		qWarning () << Q_FUNC_INFO
				<< GetCurrentSource ().ToUrl ()
				<< domain
				<< code
				<< msgStr
				<< debugStr;

		const std::map<decltype (domain), std::map<decltype (code), SourceError>> errMap
		{
			{
				GST_CORE_ERROR,
				{
					{
						GST_CORE_ERROR_MISSING_PLUGIN,
						SourceError::MissingPlugin
					}
				}
			},
			{
				GST_RESOURCE_ERROR,
				{
					{
						GST_RESOURCE_ERROR_NOT_FOUND,
						SourceError::SourceNotFound
					},
					{
						GST_RESOURCE_ERROR_OPEN_READ,
						SourceError::CannotOpenSource
					}
				}
			},
			{
				GST_STREAM_ERROR,
				{
					{
						GST_STREAM_ERROR_TYPE_NOT_FOUND,
						SourceError::InvalidSource
					},
					{
						GST_STREAM_ERROR_DECODE,
						SourceError::InvalidSource
					}
				}
			}
		};

		const auto errCode = [&]
			{
				try
				{
					return errMap.at (domain).at (code);
				}
				catch (const std::out_of_range&)
				{
					return SourceError::Other;
				}
			} ();

		if (!IsDrainingMsgs_)
		{
			qDebug () << Q_FUNC_INFO << "draining bus";
			IsDrainingMsgs_ = true;

			while (const auto newMsg = gst_bus_pop (gst_pipeline_get_bus (GST_PIPELINE (Dec_))))
				handleMessage (std::shared_ptr<GstMessage> (newMsg, gst_message_unref));

			IsDrainingMsgs_ = false;
			BusDrainWC_.wakeAll ();
		}

		if (!IsDrainingMsgs_)
			emit error (msgStr, errCode);
	}

	void SourceObject::HandleTagMsg (GstMessage *msg)
	{
		const auto oldMetadata = Metadata_;
		const auto& region = XmlSettingsManager::Instance ()
				.property ("TagsRecodingRegion").toString ();
		const bool isEnabled = XmlSettingsManager::Instance ()
				.property ("EnableTagsRecoding").toBool ();
		if (!GstUtil::ParseTagMessage (msg, Metadata_, isEnabled ? region : QString ()))
			return;

		auto merge = [this] (const QString& oldName, const QString& stdName, bool emptyOnly)
		{
			if (Metadata_.contains (oldName) &&
					(!emptyOnly || Metadata_.value (stdName).isEmpty ()))
				Metadata_ [stdName] = Metadata_.value (oldName);
		};

		const auto& title = Metadata_.value ("title");
		const auto& split = title.split (" - ", QString::SkipEmptyParts);
		if (split.size () == 2 &&
				(!Metadata_.contains ("artist") ||
					Metadata_.value ("title") != split.value (1)))
		{
			Metadata_ ["artist"] = split.value (0);
			Metadata_ ["title"] = split.value (1);
		}

		merge ("organization", "album", true);
		merge ("genre", "title", true);

		if (oldMetadata != Metadata_)
			emit metaDataChanged ();
	}

	void SourceObject::HandleBufferingMsg (GstMessage *msg)
	{
		gint percentage = 0;
		gst_message_parse_buffering (msg, &percentage);

		emit bufferStatus (percentage);
	}

	namespace
	{
		SourceState GstToState (GstState state)
		{
			switch (state)
			{
			case GST_STATE_PAUSED:
				return SourceState::Paused;
			case GST_STATE_READY:
				return SourceState::Stopped;
			case GST_STATE_PLAYING:
				return SourceState::Playing;
			default:
				return SourceState::Error;
			}
		}
	}

	void SourceObject::HandleStateChangeMsg (GstMessage *msg)
	{
		if (msg->src != GST_OBJECT (Path_->GetPipeline ()))
			return;

		GstState oldState, newState, pending;
		gst_message_parse_state_changed (msg, &oldState, &newState, &pending);

		if (oldState == newState)
			return;

		if (IsSeeking_)
		{
			if (oldState == GST_STATE_PLAYING && newState == GST_STATE_PAUSED)
				return;

			if (oldState == GST_STATE_PAUSED && newState == GST_STATE_PLAYING)
			{
				IsSeeking_ = false;
				return;
			}
		}

		const auto newNativeState = GstToState (newState);
		if (newNativeState == OldState_ ||
				pending != GST_STATE_VOID_PENDING)
			return;

		auto prevState = OldState_;
		OldState_ = newNativeState;
		emit stateChanged (newNativeState, prevState);

		if (newNativeState == SourceState::Stopped)
			emit finished ();
	}

	void SourceObject::HandleElementMsg (GstMessage *msg)
	{
#if GST_VERSION_MAJOR < 1
		const auto msgStruct = gst_message_get_structure (msg);

		if (gst_structure_has_name (msgStruct, "playbin2-stream-changed"))
		{
			gchar *uri = nullptr;
			g_object_get (Dec_, "uri", &uri, nullptr);
			qDebug () << Q_FUNC_INFO << uri;
			g_free (uri);

			setActualSource (CurrentSource_);
			emit currentSourceChanged (CurrentSource_);
		}
#else
		Q_UNUSED (msg)
#endif
	}

	void SourceObject::HandleEosMsg (GstMessage*)
	{
		qDebug () << Q_FUNC_INFO;
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_READY);
	}

	void SourceObject::HandleStreamStatusMsg (GstMessage*)
	{
	}

	void SourceObject::HandleWarningMsg (GstMessage *msg)
	{
		GError *gerror = nullptr;
		gchar *debug = nullptr;
		gst_message_parse_warning (msg, &gerror, &debug);

		const auto& msgStr = QString::fromUtf8 (gerror->message);
		const auto& debugStr = QString::fromUtf8 (debug);

		const auto code = gerror->code;
		const auto domain = gerror->domain;

		g_error_free (gerror);
		g_free (debug);

		qDebug () << Q_FUNC_INFO << code << domain << msgStr << debugStr;
	}

	int SourceObject::HandleSyncMessage (GstBus *bus, GstMessage *msg)
	{
		return SyncHandlers_ ([] (int a, int b) { return std::min (a, b); },
				static_cast<int> (GST_BUS_PASS),
				bus, msg);
	}

	void SourceObject::handleMessage (GstMessage_ptr msgPtr)
	{
		const auto message = msgPtr.get ();

		AsyncHandlers_ (message);

		switch (GST_MESSAGE_TYPE (message))
		{
		case GST_MESSAGE_ERROR:
			HandleErrorMsg (message);
			break;
		case GST_MESSAGE_TAG:
			HandleTagMsg (message);
			break;
		case GST_MESSAGE_NEW_CLOCK:
		case GST_MESSAGE_ASYNC_DONE:
			break;
		case GST_MESSAGE_BUFFERING:
			HandleBufferingMsg (message);
			break;
		case GST_MESSAGE_STATE_CHANGED:
			HandleStateChangeMsg (message);
			break;
		case GST_MESSAGE_DURATION:
			QTimer::singleShot (0,
					this,
					SLOT (updateTotalTime ()));
			break;
		case GST_MESSAGE_ELEMENT:
			HandleElementMsg (message);
			break;
		case GST_MESSAGE_EOS:
			HandleEosMsg (message);
			break;
		case GST_MESSAGE_STREAM_STATUS:
			HandleStreamStatusMsg (message);
			break;
		case GST_MESSAGE_WARNING:
			HandleWarningMsg (message);
			break;
		case GST_MESSAGE_LATENCY:
			gst_bin_recalculate_latency (GST_BIN (Dec_));
			break;
		case GST_MESSAGE_QOS:
			break;
#if GST_VERSION_MAJOR >= 1
		case GST_MESSAGE_STREAM_START:
			setActualSource (CurrentSource_);
			emit currentSourceChanged (CurrentSource_);
			break;
		case GST_MESSAGE_RESET_TIME:
			break;
#endif
		default:
			qDebug () << Q_FUNC_INFO << GST_MESSAGE_TYPE (message);
			break;
		}
	}

	void SourceObject::updateTotalTime ()
	{
		emit totalTimeChanged (GetTotalTime ());
	}

	void SourceObject::handleTick ()
	{
		emit tick (GetCurrentTime ());
	}

	void SourceObject::setActualSource (const AudioSource& source)
	{
		ActualSource_ = source;
	}
}
}
