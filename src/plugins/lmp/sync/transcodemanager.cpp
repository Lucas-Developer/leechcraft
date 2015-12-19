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

#include "transcodemanager.h"
#include <QStringList>
#include <QtDebug>
#include <QFileInfo>
#include <util/sll/prelude.h>
#include "transcodejob.h"

namespace LeechCraft
{
namespace LMP
{
	TranscodeManager::TranscodeManager (QObject *parent)
	: QObject (parent)
	{
	}

	namespace
	{
		bool IsLossless (const QString& filename)
		{
			return filename.endsWith (".flac", Qt::CaseInsensitive) ||
					filename.endsWith (".alac", Qt::CaseInsensitive);
		}
	}

	void TranscodeManager::Enqueue (QStringList files, const TranscodingParams& params)
	{
		if (params.FormatID_.isEmpty ())
		{
			Util::Map (files,
					[this, &params] (const QString& file)
						{ emit fileReady (file, file, params.FilePattern_); });
			return;
		}

		if (params.OnlyLossless_)
		{
			auto partPos = std::stable_partition (files.begin (), files.end (), IsLossless);

			for (; partPos != files.end (); ++partPos)
				emit fileReady (*partPos, *partPos, params.FilePattern_);

			files.erase (partPos, files.end ());
		}

		Queue_ += Util::Map (files,
				[&params] (const QString& file) { return qMakePair (file, params); });

		while (RunningJobs_.size () < params.NumThreads_ && !Queue_.isEmpty ())
			EnqueueJob (Queue_.takeFirst ());
	}

	void TranscodeManager::EnqueueJob (const QPair<QString, TranscodingParams>& pair)
	{
		auto job = new TranscodeJob (pair.first, pair.second, this);
		RunningJobs_ << job;
		connect (job,
				SIGNAL (done (TranscodeJob*, bool)),
				this,
				SLOT (handleDone (TranscodeJob*, bool)));
		emit fileStartedTranscoding (QFileInfo (pair.first).fileName ());
	}

	void TranscodeManager::handleDone (TranscodeJob *job, bool success)
	{
		RunningJobs_.removeAll (job);
		job->deleteLater ();

		if (!Queue_.isEmpty ())
			EnqueueJob (Queue_.takeFirst ());

		if (success)
			emit fileReady (job->GetOrigPath (), job->GetTranscodedPath (), job->GetTargetPattern ());
		else
			emit fileFailed (job->GetOrigPath ());
	}
}
}
