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

#include "httpstreamfilter.h"
#include <QUuid>
#include <QtDebug>
#include <gst/gst.h>
#include "interfaces/lmp/ifilterconfigurator.h"
#include "util/lmp/gstutil.h"
#include "httpserver.h"
#include "filterconfigurator.h"

namespace LeechCraft
{
namespace LMP
{
namespace HttStream
{
	HttpStreamFilter::HttpStreamFilter (const QByteArray& filterId, const QByteArray& instanceId)
	: FilterId_ { filterId }
	, InstanceId_ { instanceId.isEmpty () ? QUuid::createUuid ().toByteArray () : instanceId }
	, Configurator_ { new FilterConfigurator { instanceId, this } }
	, Elem_ { gst_bin_new ("httpstreambin") }
	, Tee_ { gst_element_factory_make ("tee", nullptr) }
	, TeeTemplate_ { gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (Tee_), "src%d") }
	, AudioQueue_ { gst_element_factory_make ("queue", nullptr) }
	, StreamQueue_ { gst_element_factory_make ("queue", nullptr) }
	, Encoder_ { gst_element_factory_make ("vorbisenc", nullptr) }
	, Muxer_ { gst_element_factory_make ("oggmux", nullptr) }
	, MSS_ { gst_element_factory_make ("multifdsink", nullptr) }
	, Server_ { new HttpServer { this } }
	{
		if (!MSS_)
			qWarning () << Q_FUNC_INFO
					<< "cannot create multisocketsink";

		const auto convIn = gst_element_factory_make ("audioconvert", nullptr);

		gst_bin_add_many (GST_BIN (Elem_), Tee_, AudioQueue_, StreamQueue_, Encoder_, convIn, Muxer_, nullptr);

		TeeAudioPad_ = gst_element_request_pad (Tee_, TeeTemplate_, nullptr, nullptr);
		auto audioPad = gst_element_get_static_pad (AudioQueue_, "sink");
		gst_pad_link (TeeAudioPad_, audioPad);
		gst_object_unref (audioPad);

		gst_element_link_many (StreamQueue_, convIn, Encoder_, Muxer_, nullptr);
		g_object_set (G_OBJECT (MSS_),
				"unit-type", GST_FORMAT_TIME,
				"units-max", static_cast<gint64> (7 * GST_SECOND),
				"units-soft-max", static_cast<gint64> (3 * GST_SECOND),
				"recover-policy", 3,
				"timeout", static_cast<gint64> (10 * GST_SECOND),
				"sync-method", 1,
				nullptr);

		GstUtil::AddGhostPad (Tee_, Elem_, "sink");
		GstUtil::AddGhostPad (AudioQueue_, Elem_, "src");

		connect (Server_,
				SIGNAL (gotClient (int)),
				this,
				SLOT (handleClient (int)));
		connect (Server_,
				SIGNAL (clientDisconnected (int)),
				this,
				SLOT (handleClientDisconnected (int)));
	}

	HttpStreamFilter::~HttpStreamFilter ()
	{
		gst_element_release_request_pad (Tee_, TeeAudioPad_);
		gst_object_unref (TeeAudioPad_);

		gst_object_unref (Elem_);
	}

	QByteArray HttpStreamFilter::GetEffectId () const
	{
		return FilterId_;
	}

	QByteArray HttpStreamFilter::GetInstanceId () const
	{
		return InstanceId_;
	}

	IFilterConfigurator* HttpStreamFilter::GetConfigurator () const
	{
		return Configurator_;
	}

	GstElement* HttpStreamFilter::GetElement () const
	{
		return Elem_;
	}

	void HttpStreamFilter::PostAdd (IPath *path)
	{
		path->AddSyncHandler ([this] (GstBus*, GstMessage *msg) { return HandleError (msg); });
	}

	void HttpStreamFilter::CreatePad ()
	{
		gst_bin_add (GST_BIN (Elem_), MSS_);
		gst_element_link (Muxer_, MSS_);

		TeeStreamPad_ = gst_element_request_pad (Tee_, TeeTemplate_, nullptr, nullptr);
		auto streamPad = gst_element_get_static_pad (StreamQueue_, "sink");
		gst_pad_link (TeeStreamPad_, streamPad);
		gst_object_unref (streamPad);

		gst_element_sync_state_with_parent (MSS_);
	}

	void HttpStreamFilter::DestroyPad ()
	{
		gst_element_unlink (Muxer_, MSS_);
		gst_bin_remove (GST_BIN (Elem_), MSS_);

		auto streamPad = gst_element_get_static_pad (StreamQueue_, "sink");
		gst_pad_unlink (TeeStreamPad_, streamPad);
		gst_object_unref (streamPad);

		gst_element_release_request_pad (Tee_, TeeStreamPad_);
		gst_object_unref (TeeStreamPad_);

		TeeStreamPad_ = nullptr;
	}

	int HttpStreamFilter::HandleError (GstMessage *msg)
	{
		if (GST_MESSAGE_TYPE (msg) != GST_MESSAGE_ERROR)
			return GST_BUS_PASS;

		if (QList<GstElement*> { StreamQueue_, Encoder_, MSS_ }.contains (GST_ELEMENT (msg->src)))
		{
			qDebug () << Q_FUNC_INFO
					<< "detected stream error";
			return GST_BUS_DROP;
		}

		return GST_BUS_PASS;
	}

	void HttpStreamFilter::handleClient (int socket)
	{
		if (!ClientsCount_++)
			CreatePad ();

		g_signal_emit_by_name (MSS_, "add", socket);
	}

	void HttpStreamFilter::handleClientDisconnected (int socket)
	{
		g_signal_emit_by_name (MSS_, "remove", socket);

		if (!--ClientsCount_)
			DestroyPad ();
	}
}
}
}
