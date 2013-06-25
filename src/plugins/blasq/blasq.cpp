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

#include "blasq.h"
#include <QIcon>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "interfaces/blasq/iservicesplugin.h"
#include "interfaces/blasq/iaccount.h"
#include "xmlsettingsmanager.h"
#include "accountswidget.h"
#include "servicesmanager.h"
#include "accountsmanager.h"
#include "photostab.h"

namespace LeechCraft
{
namespace Blasq
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		ServicesMgr_ = new ServicesManager;
		AccountsMgr_ = new AccountsManager (ServicesMgr_);

		XSD_.reset (new Util::XmlSettingsDialog);
		XSD_->RegisterObject (&XmlSettingsManager::Instance (), "blasqsettings.xml");

		XSD_->SetCustomWidget ("AccountsWidget", new AccountsWidget (ServicesMgr_, AccountsMgr_));

		PhotosTabTC_ = TabClassInfo
		{
			GetUniqueID () + "/Photos",
			tr ("Blasq"),
			tr ("All the photos stored in the cloud"),
			GetIcon (),
			1,
			TFOpenableByRequest | TFSuggestOpening
		};
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Blasq";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Blasq";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Client for cloud image storage services like Flickr or Picasa.");
	}

	QIcon Plugin::GetIcon () const
	{
		return QIcon ();
	}

	TabClasses_t Plugin::GetTabClasses () const
	{
		return { PhotosTabTC_ };
	}

	void Plugin::TabOpenRequested (const QByteArray& tcId)
	{
		if (tcId == PhotosTabTC_.TabClass_)
		{
			auto tab = new PhotosTab (AccountsMgr_, PhotosTabTC_, this, Proxy_);
			connect (tab,
					SIGNAL (removeTab (QWidget*)),
					this,
					SIGNAL (removeTab (QWidget*)));
			emit addNewTab (PhotosTabTC_.VisibleName_, tab);
		}
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< tcId;
	}

	QSet<QByteArray> Plugin::GetExpectedPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Blasq.General";
		result << "org.LeechCraft.Blasq.ServicePlugin";
		return result;
	}

	void Plugin::AddPlugin (QObject *plugin)
	{
		if (auto isp = qobject_cast<IServicesPlugin*> (plugin))
			ServicesMgr_->AddPlugin (isp);
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return XSD_;
	}

	ImageServiceInfos_t Plugin::GetServices () const
	{
		ImageServiceInfos_t result;
		for (auto acc : AccountsMgr_->GetAccounts ())
			result.append ({ acc->GetID (), acc->GetName () });
		return result;
	}

	IPendingImgSourceRequest* Plugin::RequestImages (const QByteArray& serviceId)
	{
		return nullptr;
	}

	IPendingImgSourceRequest* Plugin::StartDefaultChooser ()
	{
		return nullptr;
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_blasq, LeechCraft::Blasq::Plugin);
