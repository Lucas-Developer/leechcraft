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

#include "core.h"
#include <QtDebug>
#include "itemsfiltermodel.h"
#include "itemswidget.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
	ItemsFilterModel::ItemsFilterModel (QObject *parent)
	: QSortFilterProxyModel (parent)
	, UnreadOnTop_ (XmlSettingsManager::Instance ()->
			property ("UnreadOnTop").toBool ())
	{
		setDynamicSortFilter (true);

		XmlSettingsManager::Instance ()->RegisterObject ("UnreadOnTop",
				this, "handleUnreadOnTopChanged");
	}

	void ItemsFilterModel::SetItemsWidget (ItemsWidget *w)
	{
		ItemsWidget_ = w;
	}

	void ItemsFilterModel::SetHideRead (bool hide)
	{
		HideRead_ = hide;
		invalidate ();
	}

	void ItemsFilterModel::SetItemTags (QList<ITagsManager::tag_id> tags)
	{
		if (tags.isEmpty ())
			TaggedItems_.clear ();
		else
		{
			const auto& sb = Core::Instance ().MakeStorageBackendForThread ();
			TaggedItems_ = QSet<IDType_t>::fromList (sb->GetItemsForTag (tags.takeFirst ()));

			for (const auto& tag : tags)
			{
				const auto& set = QSet<IDType_t>::fromList (sb->GetItemsForTag (tag));
				TaggedItems_.intersect (set);
				if (TaggedItems_.isEmpty ())
					TaggedItems_ << -1;
			}
		}

		invalidate ();
	}

	bool ItemsFilterModel::filterAcceptsRow (int sourceRow,
			const QModelIndex& sourceParent) const
	{
		if (HideRead_ &&
				ItemsWidget_->IsItemReadNotCurrent (sourceRow))
			return false;

		if (!ItemCategories_.isEmpty ())
		{
			const auto& itemCategories = ItemsWidget_->GetItemCategories (sourceRow);

			const bool categoryFound = itemCategories.isEmpty () ?
					true :
					std::any_of (itemCategories.begin (), itemCategories.end (),
							[this] (const QString& cat) { return ItemCategories_.contains (cat); });
			if (!categoryFound)
				return false;
		}

		if (!TaggedItems_.isEmpty () &&
				!TaggedItems_.contains (ItemsWidget_->GetItemIDFromRow (sourceRow)))
			return false;

		return QSortFilterProxyModel::filterAcceptsRow (sourceRow, sourceParent);
	}

	bool ItemsFilterModel::lessThan (const QModelIndex& left,
			const QModelIndex& right) const
	{
		if (left.column () == 1 &&
				right.column () == 1 &&
				UnreadOnTop_ &&
				!HideRead_)
		{
			bool lr = ItemsWidget_->IsItemRead (left.row ());
			bool rr = ItemsWidget_->IsItemRead (right.row ());
			if (lr && !rr)
				return true;
			else if (lr == rr)
				return QSortFilterProxyModel::lessThan (left, right);
			else
				return false;
		}
		return QSortFilterProxyModel::lessThan (left, right);
	}

	void ItemsFilterModel::categorySelectionChanged (const QStringList& categories)
	{
		ItemCategories_ = QSet<QString>::fromList (categories);
		invalidateFilter ();
	}

	void ItemsFilterModel::handleUnreadOnTopChanged ()
	{
		UnreadOnTop_ = XmlSettingsManager::Instance ()->
				property ("UnreadOnTop").toBool ();
		invalidateFilter ();
	}
}
}
