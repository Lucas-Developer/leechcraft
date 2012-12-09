/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2012  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "quarkmanager.h"
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QStandardItem>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFile>
#include <QtDebug>
#include <interfaces/iquarkcomponentprovider.h>
#include "viewmanager.h"
#include "sbview.h"
#include "quarksettingsmanager.h"

namespace LeechCraft
{
namespace SB2
{
	QuarkManager::QuarkManager (const QuarkComponent& comp, ViewManager *manager)
	: QObject (manager)
	, ViewMgr_ (manager)
	, URL_ (comp.Url_)
	, SettingsManager_ (0)
	{
		qDebug () << Q_FUNC_INFO << "adding" << comp.Url_;
		auto ctx = manager->GetView ()->rootContext ();
		for (const auto& pair : comp.StaticProps_)
			ctx->setContextProperty (pair.first, pair.second);
		for (const auto& pair : comp.DynamicProps_)
			ctx->setContextProperty (pair.first, pair.second);
		for (const auto& pair : comp.ImageProviders_)
			manager->GetView ()->engine ()->addImageProvider (pair.first, pair.second);

		CreateSettings ();
	}

	bool QuarkManager::HasSettings () const
	{
		return SettingsManager_;
	}

	void QuarkManager::ShowSettings ()
	{
		if (!HasSettings ())
			return;

		QDialog dia;
		dia.setLayout (new QVBoxLayout ());
		dia.layout ()->addWidget (XSD_.get ());

		auto box = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect (box,
				SIGNAL (accepted ()),
				&dia,
				SLOT (accept ()));
		connect (box,
				SIGNAL (rejected ()),
				&dia,
				SLOT (reject ()));
		connect (box,
				SIGNAL (accepted ()),
				XSD_.get (),
				SLOT (accept ()));
		connect (box,
				SIGNAL (rejected ()),
				XSD_.get (),
				SLOT (reject ()));
		dia.layout ()->addWidget (box);

		dia.exec ();
		XSD_->setParent (0);
	}

	void QuarkManager::CreateSettings ()
	{
		if (!URL_.isLocalFile ())
			return;

		const auto& localName = URL_.toLocalFile ();
		const auto& settingsName = localName + ".settings";
		if (!QFile::exists (settingsName))
			return;

		XSD_.reset (new Util::XmlSettingsDialog);
		SettingsManager_ = new QuarkSettingsManager (URL_, ViewMgr_->GetView ()->rootContext ());
		XSD_->RegisterObject (SettingsManager_, settingsName);
	}
}
}
