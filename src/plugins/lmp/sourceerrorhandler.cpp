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

#include "sourceerrorhandler.h"
#include <QFileInfo>
#include <util/xpc/util.h>
#include <interfaces/core/ientitymanager.h>
#include "engine/sourceobject.h"

namespace LeechCraft
{
namespace LMP
{
	SourceErrorHandler::SourceErrorHandler (SourceObject *source, IEntityManager *iem)
	: QObject { source }
	, Source_ { source }
	, IEM_ { iem }
	{
		connect (Source_,
				SIGNAL (error (QString, SourceError)),
				this,
				SLOT (handleSourceError (QString, SourceError)));
	}

	void SourceErrorHandler::handleSourceError (const QString& sourceText, SourceError error)
	{
		QString text;

		const auto& curSource = Source_->GetCurrentSource ();
		const auto& curPath = curSource.ToUrl ().path ();
		const auto& filename = "<em>" + QFileInfo { curPath }.fileName () + "</em>";
		switch (error)
		{
		case SourceError::MissingPlugin:
			text = tr ("Cannot find a proper audio decoder for file %1. "
					"You probably don't have all the codec plugins installed.")
					.arg (filename);
			text += "<br/>" + sourceText;
			emit nextTrack ();
			break;
		case SourceError::SourceNotFound:
			text = tr ("Audio source %1 not found, playing next track...")
					.arg (filename);
			emit nextTrack ();
			break;
		case SourceError::CannotOpenSource:
			text = tr ("Cannot open source %1, playing next track...")
					.arg (filename);
			emit nextTrack ();
			break;
		case SourceError::InvalidSource:
			text = tr ("Audio source %1 is invalid, playing next track...")
					.arg (filename);
			emit nextTrack ();
			break;
		case SourceError::DeviceBusy:
			text = tr ("Cannot play %1 because the output device is busy.")
					.arg (filename);
			break;
		case SourceError::Other:
			text = sourceText;
			break;
		}

		IEM_->HandleEntity (Util::MakeNotification ("LMP", text, PCritical_));
	}
}
}