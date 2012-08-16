/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2012  Georg Rudoy
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

#include "hotstreams.h"
#include <QIcon>
#include <QStandardItem>
#include <QTimer>
#include <interfaces/core/icoreproxy.h>
#include "somafmlistfetcher.h"
#include "radiostation.h"
#include "roles.h"

namespace LeechCraft
{
namespace HotStreams
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		auto somafm = new QStandardItem ("SomaFM");
		somafm->setData (Media::RadioType::None, Media::RadioItemRole::ItemType);
		somafm->setEditable (false);
		somafm->setIcon (QIcon (":/hotstreams/resources/images/somafm.png"));
		Roots_ ["somafm"] = somafm;
	}

	void Plugin::SecondInit ()
	{
		QTimer::singleShot (5000,
				this,
				SLOT (refreshRadios ()));
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.HotStreams";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "HotStreams";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Provides some radio streams like Digitally Imported and SomaFM to other plugins.");
	}

	QIcon Plugin::GetIcon () const
	{
		return QIcon ();
	}

	QList<QStandardItem*> Plugin::GetRadioListItems () const
	{
		return Roots_.values ();
	}

	Media::IRadioStation_ptr Plugin::GetRadioStation (QStandardItem *item, const QString&)
	{
		const auto& name = item->data (StreamItemRoles::PristineName).toString ();
		const auto& url = item->data (Media::RadioItemRole::RadioID).toUrl ();
		auto nam = Proxy_->GetNetworkAccessManager ();
		return Media::IRadioStation_ptr (new RadioStation (url, name, nam));
	}

	void Plugin::refreshRadios ()
	{
		auto clearRoot = [] (QStandardItem *item)
		{
			while (item->rowCount ())
				item->removeRow (0);
		};

		clearRoot (Roots_ ["somafm"]);

		new SomaFMListFetcher (Roots_ ["somafm"],
				Proxy_->GetNetworkAccessManager (), this);
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_hotstreams, LeechCraft::HotStreams::Plugin);

