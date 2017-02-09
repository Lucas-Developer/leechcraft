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

#include "tracksselectordialog.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QDesktopWidget>
#include <util/sll/prelude.h>

namespace LeechCraft
{
namespace LMP
{
namespace PPL
{
	class TracksSelectorDialog::TracksModel : public QAbstractItemModel
	{
		const QStringList HeaderLabels_;

		enum Header : uint8_t
		{
			ScrobbleSummary,
			Artist,
			Album,
			Track,
			Date,
			Collection
		};
		static constexpr uint8_t MaxPredefinedHeader = Header::Collection;

		const Media::IAudioScrobbler::BackdatedTracks_t Tracks_;

		QVector<QVector<bool>> Scrobble_;
	public:
		TracksModel (const Media::IAudioScrobbler::BackdatedTracks_t&,
			const QList<Media::IAudioScrobbler*>&, QObject* = nullptr);

		QModelIndex index (int, int, const QModelIndex&) const override;
		QModelIndex parent (const QModelIndex&) const override;
		int rowCount (const QModelIndex&) const override;
		int columnCount (const QModelIndex&) const override;
		QVariant data (const QModelIndex&, int) const override;

		QVariant headerData (int, Qt::Orientation, int) const override;

		Qt::ItemFlags flags (const QModelIndex& index) const override;
		bool setData (const QModelIndex& index, const QVariant& value, int role) override;
	};

	TracksSelectorDialog::TracksModel::TracksModel (const Media::IAudioScrobbler::BackdatedTracks_t& tracks,
			const QList<Media::IAudioScrobbler*>& scrobblers, QObject *parent)
	: QAbstractItemModel { parent }
	, HeaderLabels_
	{
		[&scrobblers]
		{
			const QStringList predefined
			{
				{},
				tr ("Artist"),
				tr ("Album"),
				tr ("Track"),
				tr ("Date"),
				tr ("Record in collection")
			};
			const auto& scrobbleNames = Util::Map (scrobblers,
					[] (Media::IAudioScrobbler *scrob) { return scrob->GetServiceName (); });

			return predefined + scrobbleNames;
		} ()
	}
	, Tracks_ { tracks }
	, Scrobble_ { tracks.size (), QVector<bool> (scrobblers.size () + 1, true) }
	{
	}

	QModelIndex TracksSelectorDialog::TracksModel::index (int row, int column, const QModelIndex& parent) const
	{
		return parent.isValid () ?
				QModelIndex {} :
				createIndex (row, column);
	}

	QModelIndex TracksSelectorDialog::TracksModel::parent (const QModelIndex&) const
	{
		return {};
	}

	int TracksSelectorDialog::TracksModel::rowCount (const QModelIndex& parent) const
	{
		return parent.isValid () ?
				0 :
				Tracks_.size ();
	}

	int TracksSelectorDialog::TracksModel::columnCount (const QModelIndex& parent) const
	{
		return parent.isValid () ?
				0 :
				HeaderLabels_.size ();
	}

	QVariant TracksSelectorDialog::TracksModel::data (const QModelIndex& index, int role) const
	{
		switch (role)
		{
		case Qt::DisplayRole:
		{
			const auto& record = Tracks_.value (index.row ());

			switch (index.column ())
			{
			case Header::ScrobbleSummary:
				return {};
			case Header::Artist:
				return record.first.Artist_;
			case Header::Album:
				return record.first.Album_;
			case Header::Track:
				return record.first.Title_;
			case Header::Date:
				return record.second.toString ();
			case Header::Collection:
				return {};
			}

			return {};
		}
		case Qt::CheckStateRole:
		{
			switch (index.column ())
			{
			case Header::ScrobbleSummary:
			{
				const auto& flags = Scrobble_.value (index.row ());
				if (std::all_of (flags.begin (), flags.end (), Util::Id))
					return Qt::Checked;
				if (std::none_of (flags.begin (), flags.end (), Util::Id))
					return Qt::Unchecked;
				return Qt::PartiallyChecked;
			}
			case Header::Artist:
			case Header::Album:
			case Header::Track:
			case Header::Date:
				return {};
			case Header::Collection:
				// fall through after switch
				break;
			}

			return Scrobble_.value (index.row ()).value (index.column () - MaxPredefinedHeader) ?
					Qt::Checked :
					Qt::Unchecked;
		}
		default:
			return {};
		}
	}

	QVariant TracksSelectorDialog::TracksModel::headerData (int section, Qt::Orientation orientation, int role) const
	{
		if (role != Qt::DisplayRole)
			return {};

		switch (orientation)
		{
		case Qt::Horizontal:
			return HeaderLabels_.value (section);
		case Qt::Vertical:
			return QString::number (section + 1);
		default:
			return {};
		}
	}

	Qt::ItemFlags TracksSelectorDialog::TracksModel::flags (const QModelIndex& index) const
	{
		switch (index.column ())
		{
		case Header::Artist:
		case Header::Album:
		case Header::Track:
		case Header::Date:
			return QAbstractItemModel::flags (index);
		default:
			return Qt::ItemIsSelectable |
					Qt::ItemIsEnabled |
					Qt::ItemIsUserCheckable;
		}
	}

	bool TracksSelectorDialog::TracksModel::setData (const QModelIndex& index, const QVariant& value, int role)
	{
		if (role != Qt::CheckStateRole)
			return false;

		const auto shouldScrobble = value.toInt () == Qt::Checked;

		auto& scrobbles = Scrobble_ [index.row ()];

		const auto emitDataChanged = [this, &index]
		{
			emit dataChanged (index.sibling (index.row (), 0),
					index.sibling (index.row (), columnCount (index.parent ()) - 1));
		};

		switch (index.column ())
		{
		case Header::Artist:
		case Header::Album:
		case Header::Track:
		case Header::Date:
			return false;
		case Header::ScrobbleSummary:
			std::fill (scrobbles.begin (), scrobbles.end (), shouldScrobble);
			emitDataChanged ();
			return true;
		default:
			scrobbles [index.column () - MaxPredefinedHeader] = shouldScrobble;
			emitDataChanged ();
			return true;
		}
	}

	TracksSelectorDialog::TracksSelectorDialog (const Media::IAudioScrobbler::BackdatedTracks_t& tracks,
			const QList<Media::IAudioScrobbler*>& scrobblers,
			QWidget *parent)
	: QDialog { parent }
	, Model_ { new TracksModel { tracks, scrobblers, this } }
	{
		Ui_.setupUi (this);
		Ui_.Tracks_->setModel (Model_);

		FixSize ();
	}

	void TracksSelectorDialog::FixSize ()
	{
		Ui_.Tracks_->resizeColumnsToContents ();

		const auto Margin = 50;

		int totalWidth = Margin + Ui_.Tracks_->verticalHeader ()->width ();

		const auto header = Ui_.Tracks_->horizontalHeader ();
		for (int j = 0; j < Model_->columnCount ({}); ++j)
			totalWidth += std::max (header->sectionSize (j),
					Ui_.Tracks_->sizeHintForIndex (Model_->index (0, j, {})).width ());

		if (totalWidth < size ().width ())
			return;

		const auto desktop = qApp->desktop ();
		const auto& availableGeometry = desktop->availableGeometry (this);
		if (totalWidth > availableGeometry.width ())
			return;

		setGeometry (QStyle::alignedRect (Qt::LeftToRight,
				Qt::AlignCenter,
				{ totalWidth, height () },
				availableGeometry));

		show ();
	}
}
}
}
