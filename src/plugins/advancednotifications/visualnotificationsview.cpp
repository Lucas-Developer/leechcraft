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

#include "visualnotificationsview.h"
#include <QQmlContext>
#include <QQmlError>
#include <QQmlEngine>
#include <QtDebug>
#include <util/util.h>
#include <util/sys/paths.h>
#include <util/qml/colorthemeproxy.h>
#include <util/qml/themeimageprovider.h>
#include <util/qml/qmlerrorwatcher.h>
#include <interfaces/core/icoreproxy.h>
#include "eventproxyobject.h"
#include "core.h"

namespace LeechCraft
{
namespace AdvancedNotifications
{
	VisualNotificationsView::VisualNotificationsView ()
	{
		setStyleSheet ("background: transparent");
		setWindowFlags (Qt::WindowStaysOnTopHint | Qt::ToolTip);
		setAttribute (Qt::WA_TranslucentBackground);

		new Util::QmlErrorWatcher { this };

		connect (this,
				SIGNAL (statusChanged (QQuickWidget::Status)),
				this,
				SLOT (handleStatusChanged (QQuickWidget::Status)));

		const auto& fileLocation = Util::GetSysPath (Util::SysPath::QML, "advancednotifications", "visualnotificationsview.qml");

		if (fileLocation.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "visualnotificationsview.qml isn't found";
			return;
		}

		qDebug () << Q_FUNC_INFO << "created";

		Location_ = QUrl::fromLocalFile (fileLocation);

		auto proxy = Core::Instance ().GetProxy ();
		rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (proxy->GetColorThemeManager (), this));
		engine ()->addImageProvider ("ThemeIcons", new Util::ThemeImageProvider (proxy));

		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::QML, ""))
			engine ()->addImportPath (cand);
	}

	void VisualNotificationsView::SetEvents (const QList<EventData>& events)
	{
		auto oldEvents { std::move (LastEvents_) };

		LastEvents_.clear ();
		for (const auto& ed : events)
		{
			const auto obj = new EventProxyObject (ed, this);
			connect (obj,
					SIGNAL (actionTriggered (const QString&, int)),
					this,
					SIGNAL (actionTriggered (const QString&, int)));
			connect (obj,
					SIGNAL (dismissEventRequested (const QString&)),
					this,
					SIGNAL (dismissEvent (const QString&)));
			LastEvents_ << obj;
		}

		rootContext ()->setContextProperty ("eventsModel",
				QVariant::fromValue<QList<QObject*>> (LastEvents_));

		setSource (Location_);

		qDeleteAll (oldEvents);
	}
}
}
