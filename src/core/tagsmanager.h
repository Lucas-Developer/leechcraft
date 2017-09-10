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

#ifndef TAGSMANAGER_H
#define TAGSMANAGER_H
#include <QAbstractItemModel>
#include <QMap>
#include <QUuid>
#include <QString>
#include <QMetaType>
#include "interfaces/core/itagsmanager.h"

namespace LeechCraft
{
	class TagsManager final : public QAbstractItemModel
							, public ITagsManager
	{
		Q_OBJECT
		Q_INTERFACES (ITagsManager)

		TagsManager ();
	public:
		typedef QMap<QUuid, QString> TagsDictionary_t;
	private:
		TagsDictionary_t Tags_;
	public:
		static TagsManager& Instance ();

		int columnCount (const QModelIndex&) const;
		QVariant data (const QModelIndex&, int) const;
		QModelIndex index (int, int, const QModelIndex&) const;
		QModelIndex parent (const QModelIndex&) const;
		int rowCount (const QModelIndex&) const;

		tag_id GetID (const QString&);
		QString GetTag (tag_id) const;
		QStringList GetAllTags () const;
		QStringList Split (const QString&) const;
		QStringList SplitToIDs (const QString&);
		QString Join (const QStringList&) const;
		QString JoinIDs (const QStringList&) const;
		QAbstractItemModel* GetModel ();
		QObject* GetQObject ();

		void RemoveTag (const QModelIndex&);
		void SetTag (const QModelIndex&, const QString&);
	private:
		tag_id InsertTag (const QString&);
		void ReadSettings ();
		void WriteSettings () const;
	signals:
		void tagsUpdated (const QStringList&);
	};
};

Q_DECLARE_METATYPE (LeechCraft::TagsManager::TagsDictionary_t)

#endif

