/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2011  Oleg Linkin
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


#include "firefoxprofileselectpage.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSettings>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QTemporaryFile>
#include <QXmlStreamWriter>
#include <util/xpc/util.h>

namespace LeechCraft
{
namespace NewLife
{
namespace Importers
{
	FirefoxProfileSelectPage::FirefoxProfileSelectPage (const ICoreProxy_ptr& proxy, QWidget *parent)
	: EntityGeneratingPage { proxy, parent }
	{
		Ui_.setupUi (this);
		DB_.reset (new QSqlDatabase (QSqlDatabase::addDatabase ("QSQLITE", "Import connection")));
	}

	FirefoxProfileSelectPage::~FirefoxProfileSelectPage ()
	{
		QSqlDatabase::database ("Import connection").close ();
		DB_.reset ();
		QSqlDatabase::removeDatabase ("Import connection");
	}

	int FirefoxProfileSelectPage::nextId () const
	{
		return -1;
	}

	void FirefoxProfileSelectPage::initializePage ()
	{
		connect (wizard (),
				SIGNAL (accepted ()),
				this,
				SLOT (handleAccepted ()));

		connect (Ui_.ProfileList_,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (checkImportDataAvailable (int)));

		GetProfileList (field ("ProfileFile").toString ());
	}

	void FirefoxProfileSelectPage::GetProfileList (const QString& filename)
	{
		QSettings settings (filename, QSettings::IniFormat);
		Ui_.ProfileList_->clear ();
		Ui_.ProfileList_->addItem (tr ("Default"));
		for (const auto& name : settings.childGroups ())
		{
			settings.beginGroup (name);
			Ui_.ProfileList_->addItem (settings.value ("Name").toString ());
			settings.endGroup ();
		}
	}

	void FirefoxProfileSelectPage::checkImportDataAvailable (int index)
	{
		Ui_.HistoryImport_->setChecked (false);
		Ui_.BookmarksImport_->setChecked (false);
		Ui_.RssImport_->setChecked (false);

		if (!index)
		{
			Ui_.HistoryImport_->setEnabled (false);
			Ui_.BookmarksImport_->setEnabled (false);
			Ui_.RssImport_->setEnabled (false);
			return;
		}

		if (IsFirefoxRunning ())
		{
			QMessageBox::critical (0,
					"LeechCraft",
						tr ("Please close Firefox before importing."));
			Ui_.ProfileList_->setCurrentIndex (0);
			return;
		}

		QString profilePath = GetProfileDirectory (Ui_.ProfileList_->currentText ());

		QString rssSql ("SELECT COUNT(ann.id) FROM moz_items_annos ann,moz_bookmarks bm "
				"WHERE ann.item_id IN (SELECT item_id FROM moz_items_annos WHERE "
				" anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name "
				"= 'livemark/feedURI')) AND (ann.anno_attribute_id = 4 OR "
				"ann.anno_attribute_id = 5) AND bm.id = ann.item_id");
		QString bookmarksSql ("SELECT COUNT(pl.url) FROM moz_bookmarks bm, moz_places pl "
				"WHERE bm.parent NOT IN (SELECT ann.item_id FROM moz_items_annos "
				"ann, moz_bookmarks bm WHERE ann.item_id IN (SELECT item_id FROM "
				"moz_items_annos WHERE anno_attribute_id = (SELECT id FROM "
				"moz_anno_attributes WHERE name='livemark/feedURI')) AND "
				"ann.anno_attribute_id <> 3 AND ann.anno_attribute_id <> 7 AND bm.id"
				"= ann.item_id) AND bm.fk IS NOT NULL AND bm.fk IN (SELECT id "
				"FROM moz_places WHERE url LIKE 'http%' OR url LIKE 'ftp%' OR url "
				"like 'file%') AND bm.id > 100 AND bm.fk = pl.id AND bm.title NOT NULL");
		QString historySql ("SELECT COUNT(moz_places.url) FROM moz_historyvisits,"
				"moz_places WHERE moz_places.id = moz_historyvisits.place_id");

		if (profilePath.isEmpty ())
			return;

		QSqlQuery query = GetQuery (bookmarksSql);
		QSqlRecord record = query.record();
		Ui_.BookmarksImport_->setEnabled (record.value (0).toInt ());

		query = GetQuery (historySql);
		record = query.record();
		Ui_.HistoryImport_->setEnabled (record.value (0).toInt ());

		query = GetQuery (bookmarksSql);
		record = query.record();
		Ui_.RssImport_->setEnabled (record.value (0).toInt ());
	}


	QString FirefoxProfileSelectPage::GetProfileDirectory (const QString& profileName) const
	{
		QString profilesFile = field ("ProfileFile").toString ();
		QSettings settings (profilesFile, QSettings::IniFormat);
		QString profilePath;
		for (const atuo& groupName : settings.childGroups ())
		{
			// Call settings.endGroup() on scope exit no matter what.
			std::shared_ptr<void> guard (static_cast<void*> (0),
					[&settings] (void*) { settings.endGroup (); });
			settings.beginGroup (groupName);
			if (settings.value ("Name").toString () == profileName)
			{
				profilePath = settings.value ("Path").toString ();
				break;
			}
		}
		if (profilePath.isEmpty ())
			return QString ();

		QFileInfo file (profilesFile);
		profilePath = file.absolutePath ().append ("/").append (profilePath);

		return profilePath;
	}

	QList<QVariant> FirefoxProfileSelectPage::GetHistory ()
	{
		QString sql ("SELECT moz_places.url, moz_places.title, moz_historyvisits.visit_date "
				"FROM moz_historyvisits, moz_places WHERE moz_places.id = moz_historyvisits.place_id");
		QSqlQuery query = GetQuery (sql);
		if (query.isValid ())
		{
			QList<QVariant> history;
			do
			{
				QMap<QString, QVariant> record;
				record ["URL"] = query.value (0).toString ();
				record ["Title"] = query.value (1).toString ();
				record ["DateTime"] = QDateTime::fromTime_t (query.value (2).toLongLong () / 1000000);
				history.push_back (record);
			}
			while (query.next ());
			return history;
		}
		return QList<QVariant> ();
	}

	QList<QVariant> FirefoxProfileSelectPage::GetBookmarks ()
	{
		QString sql ("SELECT bm.title, pl.url FROM moz_bookmarks bm, moz_places pl "
				"WHERE bm.parent NOT IN (SELECT ann.item_id FROM moz_items_annos "
				"ann, moz_bookmarks bm WHERE ann.item_id IN (SELECT item_id FROM "
				"moz_items_annos WHERE anno_attribute_id = (SELECT id FROM "
				"moz_anno_attributes WHERE name='livemark/feedURI')) AND "
				"ann.anno_attribute_id <> 3 AND ann.anno_attribute_id <> 7 AND bm.id"
				"= ann.item_id) AND bm.fk IS NOT NULL AND bm.fk IN (SELECT id "
				"FROM moz_places WHERE url LIKE 'http%' OR url LIKE 'ftp%' OR url "
				"like 'file%') AND bm.id > 100 AND bm.fk = pl.id AND bm.title NOT NULL");
		QSqlQuery bookmarksQuery = GetQuery (sql);
		if (bookmarksQuery.isValid ())
		{
			QList<QVariant> bookmarks;
			QString tagsSql_p1 ("SELECT title from moz_bookmarks WHERE id IN ("
					"SELECT bm.parent FROM moz_bookmarks bm, moz_places pl "
					" WHERE pl.url='");
			QString tagsSql_p2 ("' AND bm.title IS NULL AND bm.fk = pl.id)");
			QMap<QString, QVariant> record;
			do
			{
				QString tagsSql = tagsSql_p1 + bookmarksQuery.value (1).toString () + tagsSql_p2;
				QSqlQuery tagsQuery = GetQuery (tagsSql);

				QStringList tags;
				do
				{
					QString tag = tagsQuery.value (0).toString ();
					if (!tag.isEmpty ())
						tags << tag;
				}
				while (tagsQuery.next ());

				record ["Tags"] = tags;
				record ["Title"] = bookmarksQuery.value (0).toString ();
				record ["URL"] = bookmarksQuery.value (1).toString ();
				bookmarks.push_back (record);
			}
			while (bookmarksQuery.next ());

			return bookmarks;
		}
		return QList<QVariant> ();
	}

	QString FirefoxProfileSelectPage::GetImportOpmlFile ()
	{
		QString rssSql ("SELECT ann.id, ann.item_id, ann.anno_attribute_id, ann.content,"
				"bm.title FROM moz_items_annos ann,moz_bookmarks bm WHERE ann.item_id"
				" IN (SELECT item_id FROM moz_items_annos WHERE anno_attribute_id = (SELECT"
				" id FROM moz_anno_attributes WHERE name = 'livemark/feedURI')) AND ("
				"ann.anno_attribute_id = 4 OR ann.anno_attribute_id = 5) AND "
				"bm.id = ann.item_id");
		QSqlQuery rssQuery = GetQuery (rssSql);

		if (!rssQuery.isValid ())
			return QString ();

		QSqlQuery query (*DB_);
		query.exec ("SELECT id FROM moz_anno_attributes WHERE name='livemark/siteURI'");
		if (!query.next ())
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot select 'livemark/siteURI'";
			return {};
		}
		int site = query.value (0).toInt ();
		query.exec ("SELECT id FROM moz_anno_attributes WHERE name='livemark/feedURI'");
		if (!query.next ())
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot select 'livemark/feedURI'";
			return {};
		}
		int feed = query.value (0).toInt ();

		QList<QVariant> opmlData;
		int prevItemId = -1;

		QMap<QString, QVariant> omplLine;
		do
		{
			if (rssQuery.value (2).toInt () == site)
				omplLine ["SiteUrl"] = rssQuery.value (3).toString ();
			if (rssQuery.value (2).toInt () == feed)
				omplLine ["FeedUrl"] = rssQuery.value (3).toString ();
			if (prevItemId == rssQuery.value (1).toInt ())
				opmlData.push_back (omplLine);
			else
			{
				prevItemId = rssQuery.value (1).toInt ();
				omplLine ["Title"] = rssQuery.value (4).toString ();
			}
		}
		while (rssQuery.next ());

		QTemporaryFile file ("firefox_XXXXXX.opml");
		file.setAutoRemove (false);
		if (!file.open ())
		{
			SendEntity (Util::MakeNotification ("Firefox Import",
					tr ("OPML file for importing RSS cannot be created: %1")
							.arg (file.errorString ()),
					PCritical_));
			return QString ();
		}

		QXmlStreamWriter streamWriter (&file);
		streamWriter.setAutoFormatting (true);
		streamWriter.writeStartDocument ();
		streamWriter.writeStartElement ("opml");
		streamWriter.writeAttribute ("version", "1.0");
		streamWriter.writeStartElement ("head");
		streamWriter.writeStartElement ("text");
		streamWriter.writeEndElement ();
		streamWriter.writeEndElement ();
		streamWriter.writeStartElement ("body");
		streamWriter.writeStartElement ("outline");
		streamWriter.writeAttribute ("text", "Live Bookmarks");
		for (const auto& hRowVar : opmlData)
		{
			streamWriter.writeStartElement ("outline");
			QMap<QString, QVariant> hRow = hRowVar.toMap ();
			QXmlStreamAttributes attr;
			attr.append ("title", hRow ["Title"].toString ());
			attr.append ("htmlUrl", hRow ["SiteUrl"].toString ());
			attr.append ("xmlUrl", hRow ["FeedUrl"].toString ());
			attr.append ("text", hRow ["Title"].toString ());
			streamWriter.writeAttributes (attr);
			streamWriter.writeEndElement ();
		}
		streamWriter.writeEndElement ();
		streamWriter.writeEndElement ();
		streamWriter.writeEndDocument ();

		QString filename = file.fileName ();
		return filename;
	}

	QSqlQuery FirefoxProfileSelectPage::GetQuery (const QString& sql)
	{
		QString profilePath = GetProfileDirectory (Ui_.ProfileList_->currentText ());

		if (profilePath.isEmpty ())
			return QSqlQuery ();

		DB_->setDatabaseName (profilePath + "/places.sqlite");

		if (!DB_->open ())
		{
			qWarning () << Q_FUNC_INFO
					<< "could not open database"
					<< DB_->lastError ().text ();
			SendEntity (Util::MakeNotification (tr ("Firefox Import"),
						tr ("Could not open Firefox database: %1.")
							.arg (DB_->lastError ().text ()),
						PCritical_));
		}
		else
		{
			QSqlQuery query (*DB_);
			query.exec (sql);
			if (query.isActive ())
			{
				query.next ();
				return query;
			}
		}
		return QSqlQuery ();
	}

	void FirefoxProfileSelectPage::handleAccepted ()
	{
		if (IsFirefoxRunning ())
			return;

		if (Ui_.HistoryImport_->isEnabled () && Ui_.HistoryImport_->isChecked ())
		{
			Entity eHistory = Util::MakeEntity (QUrl::fromLocalFile (GetProfileDirectory (Ui_.ProfileList_->currentText ())),
				QString (),
				FromUserInitiated,
				"x-leechcraft/browser-import-data");

			eHistory.Additional_ ["BrowserHistory"] = GetHistory ();
			SendEntity (eHistory);
		}

		if (Ui_.BookmarksImport_->isEnabled () && Ui_.BookmarksImport_->isChecked ())
		{
			Entity eBookmarks = Util::MakeEntity (QUrl::fromLocalFile (GetProfileDirectory (Ui_.ProfileList_->currentText ())),
					QString (),
					FromUserInitiated,
					"x-leechcraft/browser-import-data");

			eBookmarks.Additional_ ["BrowserBookmarks"] = GetBookmarks ();
			SendEntity (eBookmarks);
		}

		if (Ui_.RssImport_->isEnabled () && Ui_.RssImport_->isChecked ())
		{
			QString opmlFile = GetImportOpmlFile ();
			Entity eRss = Util::MakeEntity (QUrl::fromLocalFile (opmlFile),
					QString (),
					FromUserInitiated,
					"text/x-opml");

			eRss.Additional_ ["RemoveAfterHandling"] = true;
			SendEntity (eRss);
		}
		DB_->close ();
	}

	bool FirefoxProfileSelectPage::IsFirefoxRunning()
	{
		QFileInfo ffStarted (GetProfileDirectory (Ui_.ProfileList_->currentText ()) + "/lock");
		return ffStarted.isSymLink ();
	}
}
}
}
