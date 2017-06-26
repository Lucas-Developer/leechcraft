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

#include "formats.h"
#include <algorithm>
#include <QProcess>
#include <QtDebug>
#include <util/sll/prelude.h>
#include <util/sll/unreachable.h>
#include "transcodingparams.h"

namespace LeechCraft
{
namespace LMP
{
	QString Format::GetFileExtension () const
	{
		return GetFormatID ();
	}

	QString Format::GetCodecID () const
	{
		return GetCodecName ();
	}

	QStringList Format::ToFFmpeg (const TranscodingParams& params) const
	{
		QStringList result
		{
			"-acodec",
			GetCodecID ()
		};
		StandardQualityAppend (result, params);
		return result;
	}

	void Format::StandardQualityAppend (QStringList& result, const TranscodingParams& params) const
	{
		const auto& num = GetBitrateLabels (params.BitrateType_).value (params.Quality_);
		switch (params.BitrateType_)
		{
		case Format::BitrateType::CBR:
			result << "-ab"
					<< (QString::number (num) + "k");
			break;
		case Format::BitrateType::VBR:
			result << "-aq"
					<< QString::number (num);
			break;
		}
	}

	class OggFormat : public Format
	{
	public:
		QString GetFormatID () const
		{
			return "ogg";
		}

		QString GetFormatName () const
		{
			return "OGG Vorbis";
		}

		QString GetCodecName () const
		{
			return "vorbis";
		}

		QString GetCodecID () const
		{
			return "libvorbis";
		}

		QList<BitrateType> GetSupportedBitrates() const
		{
			return { BitrateType::VBR, BitrateType::CBR };
		}

		QList<int> GetBitrateLabels (BitrateType type) const
		{
			switch (type)
			{
			case BitrateType::CBR:
				return { 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 500 };
			case BitrateType::VBR:
				return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
			}

			Util::Unreachable ();
		}
	};

	class AACFormatBase : public Format
	{
	public:
		QString GetFileExtension () const
		{
			return "m4a";
		}

		QList<BitrateType> GetSupportedBitrates () const
		{
			return { BitrateType::VBR, BitrateType::CBR };
		}

		QList<int> GetBitrateLabels (BitrateType type) const
		{
			switch (type)
			{
			case BitrateType::CBR:
				return { 24, 56, 76, 92, 128, 144, 176, 180, 192, 200, 224 };
			case BitrateType::VBR:
			{
				QList<int> result;
				for (int i = 0; i <= 10; ++i)
					result << i * 25 + 5;
				return result;
			}
			}

			Util::Unreachable ();
		}

		QStringList ToFFmpeg (const TranscodingParams& params) const
		{
			QStringList result;
			AppendCodec (result);
			StandardQualityAppend (result, params);
			return result;
		}
	protected:
		virtual void AppendCodec (QStringList& result) const = 0;
	};

	class AACFormat : public AACFormatBase
	{
	public:
		QString GetFormatID () const
		{
			return "aac-free";
		}

		QString GetFormatName () const
		{
			return QObject::tr ("AAC (free)");
		}

		QString GetCodecName () const
		{
			return "aac";
		}
	protected:
		void AppendCodec (QStringList& result) const
		{
			result << "-acodec" << "aac" << "-strict" << "-2";
		}
	};

	class FAACFormat : public AACFormatBase
	{
	public:
		QString GetFormatID () const
		{
			return "aac-nonfree";
		}

		QString GetFormatName () const
		{
			return QObject::tr ("AAC (non-free libfaac implementation)");
		}

		QString GetCodecName () const
		{
			return "libfaac";
		}
	protected:
		void AppendCodec (QStringList& result) const
		{
			result << "-acodec" << "libfaac";
		}
	};

	class MP3Format : public Format
	{
	public:
		QString GetFormatID () const
		{
			return "mp3";
		}

		QString GetFormatName () const
		{
			return "MP3";
		}

		QString GetCodecName () const
		{
			return "mp3";
		}

		QList<BitrateType> GetSupportedBitrates () const
		{
			return { BitrateType::CBR, BitrateType::VBR };
		}

		QList<int> GetBitrateLabels (BitrateType type) const
		{
			switch (type)
			{
			case BitrateType::CBR:
				return { 64, 96, 128, 144, 160, 192, 224, 256, 320 };
			case BitrateType::VBR:
				return { -9, -8, -7, -6, -5, -4, -3, -2, -1 };
			}

			Util::Unreachable ();
		}
	};

	class WMAFormat : public Format
	{
	public:
		QString GetFormatID () const
		{
			return "wma";
		}

		QString GetFormatName () const
		{
			return "Windows Media Audio";
		}

		QString GetCodecName () const
		{
			return "wmav2";
		}

		QList<BitrateType> GetSupportedBitrates () const
		{
			return { BitrateType::CBR };
		}

		QList<int> GetBitrateLabels (BitrateType type) const
		{
			if (type == BitrateType::CBR)
				return { 65, 75, 88, 106, 133, 180, 271, 545 };

			qWarning () << Q_FUNC_INFO
					<< "unknown bitrate type";
			return QList<int> ();
		}
	};

	QString Formats::S_FFmpegCodecs_;

	Formats::Formats ()
	{
		if (S_FFmpegCodecs_.isEmpty ())
		{
			QProcess ffmpegProcess;
			ffmpegProcess.start ("ffmpeg", QStringList ("-codecs"));
			ffmpegProcess.waitForFinished (1000);
			S_FFmpegCodecs_ = ffmpegProcess.readAllStandardOutput ();
		}

		Formats_ << std::make_shared<OggFormat> ();
		Formats_ << std::make_shared<AACFormat> ();
		Formats_ << std::make_shared<FAACFormat> ();
		Formats_ << std::make_shared<MP3Format> ();
		Formats_ << std::make_shared<WMAFormat> ();

		EnabledFormats_ = Util::Filter (Formats_,
				[] (const Format_ptr& format)
				{
					return S_FFmpegCodecs_.contains (QRegExp (".EA... " + format->GetCodecName ()));
				});
	}

	QList<Format_ptr> Formats::GetFormats () const
	{
		return EnabledFormats_;
	}

	Format_ptr Formats::GetFormat (const QString& id) const
	{
		const auto pos = std::find_if (EnabledFormats_.begin (), EnabledFormats_.end (),
				[&id] (const Format_ptr format) { return format->GetFormatID () == id; });
		return pos == EnabledFormats_.end () ?
				Format_ptr () :
				*pos;
	}
}
}
