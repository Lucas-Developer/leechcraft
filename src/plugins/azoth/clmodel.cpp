/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "clmodel.h"
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>
#include <util/defaulthookproxy.h>
#include "interfaces/azoth/iclentry.h"
#include "interfaces/azoth/iaccount.h"
#include "core.h"
#include "transferjobmanager.h"

namespace LeechCraft
{
namespace Azoth
{
	const QString CLEntryFormat = "x-leechcraft/azoth-cl-entry";

	CLModel::CLModel (QObject *parent)
	: QStandardItemModel (parent)
	{
	}

	QStringList CLModel::mimeTypes () const
	{
		return QStringList (CLEntryFormat) << "text/uri-list" << "text/plain";
	}

	QMimeData* CLModel::mimeData (const QModelIndexList& indexes) const
	{
		QMimeData *result = new QMimeData;

		QByteArray encoded;
		QDataStream stream (&encoded, QIODevice::WriteOnly);

		QStringList names;
		QList<QUrl> urls;

		for (const auto& index : indexes)
		{
			if (index.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
				continue;

			QObject *entryObj = index
					.data (Core::CLREntryObject).value<QObject*> ();
			ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);
			if (!entry)
				continue;

			const QString& thisGroup = index.parent ()
					.data (Core::CLREntryCategory).toString ();

			if (entry->GetEntryType () == ICLEntry::ETChat)
				stream << entry->GetEntryID () << thisGroup;

			names << entry->GetEntryName ();

			urls << QUrl (entry->GetHumanReadableID ());
		}

		result->setData (CLEntryFormat, encoded);
		result->setText (names.join ("; "));
		result->setUrls (urls);

		return result;
	}

	bool CLModel::dropMimeData (const QMimeData *mime,
			Qt::DropAction action, int row, int, const QModelIndex& parent)
	{
		qDebug () << "drop" << mime->formats () << action;
		if (action == Qt::IgnoreAction)
			return true;

		if (PerformHooks (mime, row, parent))
			return true;

		if (TryDropContact (mime, row, parent) ||
				TryDropContact (mime, parent.row (), parent.parent ()))
			return true;

		if (TryDropFile (mime, parent))
			return true;

		return false;
	}

	Qt::DropActions CLModel::supportedDropActions () const
	{
		return static_cast<Qt::DropActions> (Qt::CopyAction | Qt::MoveAction | Qt::LinkAction);
	}

	bool CLModel::PerformHooks (const QMimeData *mime, int row, const QModelIndex& parent)
	{
		if (CheckHookDnDEntry2Entry (mime, row, parent))
			return true;

		return false;
	}

	bool CLModel::CheckHookDnDEntry2Entry (const QMimeData *mime, int row, const QModelIndex& parent)
	{
		if (row != -1 ||
				!mime->hasFormat (CLEntryFormat) ||
				parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return false;

		QDataStream stream (mime->data (CLEntryFormat));
		QString sid;
		stream >> sid;

		QObject *source = Core::Instance ().GetEntry (sid);
		if (!source)
			return false;

		QObject *target = parent.data (Core::CLREntryObject).value<QObject*> ();

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookDnDEntry2Entry (proxy, source, target);
		return proxy->IsCancelled ();
	}

	bool CLModel::TryDropContact (const QMimeData *mime, int row, const QModelIndex& parent)
	{
		if (!mime->hasFormat (CLEntryFormat))
			return false;

		if (parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETAccount)
			return false;

		QObject *accObj = parent.data (Core::CLRAccountObject).value<QObject*> ();
		IAccount *acc = qobject_cast<IAccount*> (accObj);
		if (!acc)
			return false;

		const QString& newGrp = parent.child (row, 0)
				.data (Core::CLREntryCategory).toString ();

		QDataStream stream (mime->data (CLEntryFormat));
		while (!stream.atEnd ())
		{
			QString id;
			QString oldGroup;
			stream >> id >> oldGroup;

			if (oldGroup == newGrp)
				continue;

			QObject *entryObj = Core::Instance ().GetEntry (id);
			ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);
			if (!entry)
				continue;

			QStringList groups = entry->Groups ();
			groups.removeAll (oldGroup);
			groups << newGrp;

			entry->SetGroups (groups);
		}

		return true;
	}

	bool CLModel::TryDropFile (const QMimeData* mime, const QModelIndex& parent)
	{
		// If MIME has CLEntryFormat, it's another serialized entry, we probably
		// don't want to send it.
		if (mime->hasFormat (CLEntryFormat))
			return false;

		if (parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return false;

		QObject *entryObj = parent.data (Core::CLREntryObject).value<QObject*> ();
		ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);

		const auto& urls = mime->urls ();
		if (urls.isEmpty ())
			return false;

		return Core::Instance ().GetTransferJobManager ()->OfferURLs (entry, urls);
	}
}
}
