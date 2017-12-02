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

#include "nowplayingwidget.h"
#include <algorithm>
#include <QMouseEvent>
#include <QMenu>
#include <util/xpc/stddatafiltermenucreator.h>
#include <interfaces/core/iiconthememanager.h>
#include "mediainfo.h"
#include "core.h"
#include "localcollection.h"
#include "aalabeleventfilter.h"
#include "util.h"
#include "biowidget.h"
#include "similarview.h"

namespace LeechCraft
{
namespace LMP
{
	NowPlayingWidget::NowPlayingWidget (QWidget *parent)
	: QWidget (parent)
	, BioWidget_ (new BioWidget)
	, SimilarView_ (new SimilarView (Core::Instance ().GetProxy ()))
	{
		Ui_.setupUi (this);
		Ui_.BioPage_->layout ()->addWidget (BioWidget_);
		Ui_.SimilarPage_->layout ()->addWidget (SimilarView_);
		connect (Ui_.SimilarIncludeCollection_,
				SIGNAL (stateChanged (int)),
				this,
				SLOT (resetSimilarArtists ()));

		auto mgr = Core::Instance ().GetProxy ()->GetIconThemeManager ();
		Ui_.PrevLyricsButton_->setIcon (mgr->GetIcon ("go-previous"));
		Ui_.NextLyricsButton_->setIcon (mgr->GetIcon ("go-next"));

		updateLyricsSwitcher ();

		connect (BioWidget_,
				SIGNAL (gotArtistImage (QString, QUrl)),
				this,
				SIGNAL (gotArtistImage (QString, QUrl)));
	}

	void NowPlayingWidget::AddTab (const QString& tabName, QWidget *widget)
	{
		Ui_.CurrentSongTabs_->addTab (widget, tabName);
	}

	void NowPlayingWidget::SetSimilarArtists (Media::SimilarityInfos_t infos)
	{
		LastInfos_ = infos;

		if (Ui_.SimilarIncludeCollection_->checkState () != Qt::Checked)
		{
			auto col = Core::Instance ().GetLocalCollection ();
			auto pos = std::remove_if (infos.begin (), infos.end (),
					[col] (decltype (infos.front ()) item)
						{ return col->FindArtist (item.Artist_.Name_) >= 0; });
			infos.erase (pos, infos.end ());
		}

		SimilarView_->SetSimilarArtists (infos);
		SimilarView_->setVisible (!infos.isEmpty ());
	}

	void NowPlayingWidget::SetLyrics (const Media::LyricsResultItem& item)
	{
		if (item.Lyrics_.simplified ().isEmpty ())
			return;

		if (std::find_if (PossibleLyrics_.begin (), PossibleLyrics_.end (),
				Util::EqualityBy (&Media::LyricsResultItem::Lyrics_)) != PossibleLyrics_.end ())
			return;

		if (Ui_.LyricsBrowser_->toPlainText ().isEmpty ())
			Ui_.LyricsBrowser_->setHtml (item.Lyrics_);

		PossibleLyrics_ << item;
		updateLyricsSwitcher ();
	}

	void NowPlayingWidget::SetTrackInfo (const MediaInfo& info)
	{
		if (CurrentInfo_ == info)
			return;

		CurrentInfo_ = info;

		BioWidget_->SetCurrentArtist (info.Artist_);

		Ui_.AudioProps_->SetProps (info);

		PossibleLyrics_.clear ();
		Ui_.LyricsBrowser_->clear ();
		LyricsVariantPos_ = 0;
		updateLyricsSwitcher ();
	}

	void NowPlayingWidget::on_PrevLyricsButton__released ()
	{
		if (LyricsVariantPos_ <= 0)
			return;

		--LyricsVariantPos_;
		updateLyricsSwitcher ();
	}

	void NowPlayingWidget::on_NextLyricsButton__released ()
	{
		if (LyricsVariantPos_ >= PossibleLyrics_.size () - 1)
			return;

		++LyricsVariantPos_;
		updateLyricsSwitcher ();
	}

	void NowPlayingWidget::updateLyricsSwitcher ()
	{
		const auto& size = PossibleLyrics_.size ();

		const auto& str = size ?
				tr ("showing lyrics from %3 (%1 of %2)")
					.arg (LyricsVariantPos_ + 1)
					.arg (size)
					.arg (PossibleLyrics_.at (LyricsVariantPos_).ProviderName_):
				QString ();
		Ui_.LyricsCounter_->setText (str);

		if (LyricsVariantPos_ <= size - 1)
			Ui_.LyricsBrowser_->setHtml (PossibleLyrics_.at (LyricsVariantPos_).Lyrics_);

		Ui_.PrevLyricsButton_->setEnabled (LyricsVariantPos_);
		Ui_.NextLyricsButton_->setEnabled (LyricsVariantPos_ < size - 1);
	}

	void NowPlayingWidget::on_LyricsBrowser__customContextMenuRequested (const QPoint& p)
	{
		std::shared_ptr<QMenu> menu { Ui_.LyricsBrowser_->createStandardContextMenu (p) };

		const auto& cursor = Ui_.LyricsBrowser_->textCursor ();
		const auto& selection = cursor.selectedText ();

		if (!selection.isEmpty ())
		{
			const auto iem = Core::Instance ().GetProxy ()->GetEntityManager ();
			new Util::StdDataFilterMenuCreator { selection, iem, menu.get () };
		}

		menu->exec (Ui_.LyricsBrowser_->mapToGlobal (p));
	}

	void NowPlayingWidget::resetSimilarArtists ()
	{
		SetSimilarArtists (LastInfos_);
	}
}
}
