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

#include "categoriesselector.h"
#include <QCoreApplication>
#include <QSettings>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include "vgrabber.h"
#include "categorymodifier.h"

namespace LeechCraft
{
namespace vGrabber
{
	CategoriesSelector::CategoriesSelector (FindProxy::FindProxyType type,
			vGrabber *vgr, QWidget *parent)
	: QWidget (parent)
	, Parent_ (vgr)
	, Type_ (type)
	{
		Ui_.setupUi (this);

		ReadSettings ();
	}

	QStringList CategoriesSelector::GetCategories () const
	{
		QStringList categories;
		for (int i = 0, size = Ui_.CategoriesTree_->topLevelItemCount ();
				i < size; ++i)
		{
			QTreeWidgetItem *item = Ui_.CategoriesTree_->topLevelItem (i);
			categories << item->data (0, Qt::UserRole).toString ();
		}
		return categories;
	}

	QStringList CategoriesSelector::GetHRCategories () const
	{
		QStringList result;
		Q_FOREACH (QString id, GetCategories ())
			result << Parent_->GetProxy ()->
				GetTagsManager ()->GetTag (id);
		return result;
	}

	void CategoriesSelector::ReadSettings ()
	{
		Ui_.CategoriesTree_->clear ();

		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_vGrabber");
		settings.beginGroup ("Categories");

		int size = settings.beginReadArray (QString::number (Type_));
		QList<QTreeWidgetItem*> items;
		for (int i = 0; i < size; ++i)
		{
			settings.setArrayIndex (i);
			QString id = settings.value ("ID").toString ();
			QString name = Parent_->GetProxy ()->
				GetTagsManager ()->GetTag (id);

			QTreeWidgetItem *item = new QTreeWidgetItem (Ui_.CategoriesTree_,
					QStringList (name));
			item->setData (0, Qt::UserRole, id);
			items << item;
		}

		if (items.size ())
			Ui_.CategoriesTree_->addTopLevelItems (items);
		else
		{
			switch (Type_)
			{
				case FindProxy::FPTAudio:
					AddItem ("music");
					WriteSettings ();
					Deleted_.clear ();
					Added_.clear ();
					break;
				case FindProxy::FPTVideo:
					AddItem ("videos");
					WriteSettings ();
					Deleted_.clear ();
					Added_.clear ();
					break;
			}
		}

		settings.endArray ();
		settings.endGroup ();
	}

	void CategoriesSelector::WriteSettings ()
	{
		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_vGrabber");
		settings.beginGroup ("Categories");
		settings.beginWriteArray (QString::number (Type_));
		for (int i = 0, size = Ui_.CategoriesTree_->topLevelItemCount ();
				i < size; ++i)
		{
			settings.setArrayIndex (i);
			QTreeWidgetItem *item = Ui_.CategoriesTree_->topLevelItem (i);
			settings.setValue ("ID",
					item->data (0, Qt::UserRole).toString ());
		}
		settings.endArray ();
		settings.endGroup ();
	}

	void CategoriesSelector::AddItem (const QString& name)
	{
		QString id = Parent_->GetProxy ()->
			GetTagsManager ()->GetID (name);

		QTreeWidgetItem *item = new QTreeWidgetItem (Ui_.CategoriesTree_,
				QStringList (name));
		item->setData (0, Qt::UserRole, id);
		Ui_.CategoriesTree_->addTopLevelItem (item);

		if (Deleted_.contains (id))
			Deleted_.removeAll (id);
		else
			Added_ << id;
	}

	void CategoriesSelector::accept ()
	{
		WriteSettings ();

		emit goingToAccept (Added_, Deleted_);

		Deleted_.clear ();
		Added_.clear ();
	}

	void CategoriesSelector::reject ()
	{
		ReadSettings ();

		Deleted_.clear ();
		Added_.clear ();
	}

	void CategoriesSelector::on_Add__released ()
	{
		CategoryModifier cm (QString (), this);
		cm.setWindowTitle (tr ("Add category"));
		if (cm.exec () != QDialog::Accepted)
			return;

		QStringList splitted = Parent_->GetProxy ()->
			GetTagsManager ()->Split (cm.GetText ());
		Q_FOREACH (QString cat, splitted)
			AddItem (cat);
	}

	void CategoriesSelector::on_Modify__released ()
	{
		QTreeWidgetItem *item = Ui_.CategoriesTree_->currentItem ();
		if (!item)
			return;

		CategoryModifier cm (QString (item->text (0)));
		cm.setWindowTitle (tr ("Modify category"));
		if (cm.exec () != QDialog::Accepted)
			return;

		QStringList splitted = Parent_->GetProxy ()->
			GetTagsManager ()->Split (cm.GetText ());
		Q_FOREACH (QString cat, splitted)
			AddItem (cat);

		QString id = item->data (0, Qt::UserRole).toString ();
		if (Added_.contains (id))
			Added_.removeAll (id);
		else
			Deleted_ << id;
		delete item;
	}

	void CategoriesSelector::on_Remove__released ()
	{
		QTreeWidgetItem *item = Ui_.CategoriesTree_->currentItem ();
		if (item &&
				Ui_.CategoriesTree_->topLevelItemCount () > 1)
		{
			QString id = item->data (0, Qt::UserRole).toString ();
			if (Added_.contains (id))
				Added_.removeAll (id);
			else
				Deleted_ << id;
			delete item;
		}
	}
}
}
