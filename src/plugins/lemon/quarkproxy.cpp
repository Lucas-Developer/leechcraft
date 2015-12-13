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

#include "quarkproxy.h"
#include "trafficdialog.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Lemon
{
	QuarkProxy::QuarkProxy (TrafficManager *tm, QObject *parent)
	: QObject { parent }
	, TrafficMgr_ { tm }
	{
		XmlSettingsManager::Instance ().RegisterObject ("DownloadColor",
				this, "downloadGraphColorChanged");
		XmlSettingsManager::Instance ().RegisterObject ("UploadColor",
				this, "uploadGraphColorChanged");
	}

	QColor QuarkProxy::GetDownloadGraphColor () const
	{
		return XmlSettingsManager::Instance ().property ("DownloadColor").value<QColor> ();
	}

	QColor QuarkProxy::GetUploadGraphColor () const
	{
		return XmlSettingsManager::Instance ().property ("UploadColor").value<QColor> ();
	}

	void QuarkProxy::showGraph (const QString& ifaceName)
	{
		if (auto dia = Iface2Dialog_ [ifaceName])
		{
			delete dia;
			return;
		}

		auto dia = new TrafficDialog (ifaceName, TrafficMgr_);
		dia->setAttribute (Qt::WA_DeleteOnClose);
		dia->show ();
		Iface2Dialog_ [ifaceName] = dia;
	}
}
}
