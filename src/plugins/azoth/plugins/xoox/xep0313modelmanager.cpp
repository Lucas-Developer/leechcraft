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

#include "xep0313modelmanager.h"
#include <QStandardItemModel>
#include <QtDebug>
#include "glooxaccount.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	Xep0313ModelManager::Xep0313ModelManager (GlooxAccount *acc)
	: QObject { acc }
	, Model_ { new QStandardItemModel { this } }
	{
		Model_->setHorizontalHeaderLabels ({ tr ("Entry name"), tr ("JID") });

		connect (acc,
				SIGNAL (gotCLItems (QList<QObject*>)),
				this,
				SLOT (handleGotCLItems (QList<QObject*>)));
		connect (acc,
				SIGNAL (removedCLItems (QList<QObject*>)),
				this,
				SLOT (handleRemovedCLItems (QList<QObject*>)));
	}

	QAbstractItemModel* Xep0313ModelManager::GetModel () const
	{
		return Model_;
	}

	QString Xep0313ModelManager::Index2Jid (const QModelIndex& index) const
	{
		const auto item = Model_->itemFromIndex (index.sibling (index.row (), 0));
		return Jid2Item_.key (item);
	}

	QModelIndex Xep0313ModelManager::Jid2Index (const QString& jid) const
	{
		const auto item = Jid2Item_.value (jid);
		if (!item)
		{
			qWarning () << Q_FUNC_INFO
					<< "no index for item"
					<< item;
			return {};
		}
		return item->index ();
	}

	void Xep0313ModelManager::PerformWithEntries (const QList<QObject*>& items,
			const std::function<void (ICLEntry*)>& f)
	{
		for (auto itemObj : items)
		{
			auto entry = qobject_cast<ICLEntry*> (itemObj);
			if (entry->GetEntryType () == ICLEntry::EntryType::MUC)
				continue;

			f (entry);
		}
	}

	void Xep0313ModelManager::handleGotCLItems (const QList<QObject*>& items)
	{
		PerformWithEntries (items,
				[this] (ICLEntry *entry)
				{
					const auto& jid = entry->GetHumanReadableID ();
					if (Jid2Item_.contains (jid))
						return;

					const QList<QStandardItem*> row
					{
						new QStandardItem (entry->GetEntryName ()),
						new QStandardItem (jid)
					};
					for (const auto item : row)
					{
						item->setEditable (false);
						item->setData (QVariant::fromValue (entry->GetQObject ()),
								ServerHistoryRole::CLEntry);
					}
					Jid2Item_ [jid] = row.first ();

					Model_->appendRow (row);
				});
	}

	void Xep0313ModelManager::handleRemovedCLItems (const QList<QObject*>& items)
	{
		PerformWithEntries (items,
				[this] (ICLEntry *entry) -> void
				{
					const auto& jid = entry->GetHumanReadableID ();
					if (!Jid2Item_.contains (jid))
						return;

					const auto row = Jid2Item_.take (jid)->row ();
					Model_->removeRow (row);
				});
	}
}
}
}
