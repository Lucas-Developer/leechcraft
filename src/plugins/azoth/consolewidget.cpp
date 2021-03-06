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

#include "consolewidget.h"
#include <QDomDocument>
#include <QtDebug>
#include "interfaces/azoth/iaccount.h"

namespace LeechCraft
{
namespace Azoth
{
	ConsoleWidget::ConsoleWidget (QObject *obj, QWidget *parent)
	: QWidget (parent)
	, AsObject_ (obj)
	, AsAccount_ (qobject_cast<IAccount*> (obj))
	, AsConsole_ (qobject_cast<IHaveConsole*> (obj))
	, Format_ (AsConsole_->GetPacketFormat ())
	{
		Ui_.setupUi (this);

		TabClassInfo temp =
		{
			"ConsoleTab",
			tr ("IM console"),
			tr ("Protocol console, for example, XML console for a XMPP "
				"client protocol"),
			QIcon ("lcicons:/plugins/azoth/resources/images/sdtab.svg"),
			0,
			TFEmpty
		};
		TabClass_ = temp;

		connect (obj,
				SIGNAL (gotConsolePacket (QByteArray, IHaveConsole::PacketDirection, QString)),
				this,
				SLOT (handleConsolePacket (QByteArray, IHaveConsole::PacketDirection, QString)));

		AsConsole_->SetConsoleEnabled (true);
	}

	TabClassInfo ConsoleWidget::GetTabClassInfo () const
	{
		return TabClass_;
	}

	QObject* ConsoleWidget::ParentMultiTabs ()
	{
		return ParentMultiTabs_;
	}

	void ConsoleWidget::Remove ()
	{
		if (AsObject_)
			AsConsole_->SetConsoleEnabled (false);
		emit removeTab (this);
		deleteLater ();
	}

	QToolBar* ConsoleWidget::GetToolBar () const
	{
		return 0;
	}

	void ConsoleWidget::SetParentMultiTabs (QObject *obj)
	{
		ParentMultiTabs_ = obj;
	}

	QString ConsoleWidget::GetTitle () const
	{
		return tr ("%1: console").arg (AsAccount_->GetAccountName ());
	}

	void ConsoleWidget::handleConsolePacket (QByteArray data,
			IHaveConsole::PacketDirection direction, const QString& entryId)
	{
		const QString& filter = Ui_.EntryIDFilter_->text ();
		if (!filter.isEmpty () && !entryId.contains (filter, Qt::CaseInsensitive))
			return;

		const QString& color = direction == IHaveConsole::PacketDirection::Out ?
				"#56ED56" :			// rather green
				"#ED55ED";			// violet or something

		QString html = (direction == IHaveConsole::PacketDirection::Out ?
					QString::fromUtf8 ("→→→→→→ [%1] →→→→→→") :
					QString::fromUtf8 ("←←←←←← [%1] ←←←←←←"))
				.arg (QTime::currentTime ().toString ("HH:mm:ss.zzz"));
		html += "<br /><font color=\"" + color + "\">";
		switch (Format_)
		{
		case IHaveConsole::PacketFormat::Binary:
			html += "(base64) ";
			html += data.toBase64 ();
			break;
		case IHaveConsole::PacketFormat::XML:
		{
			QDomDocument doc;
			data.prepend ("<root>");
			data.append ("</root>");
			if (doc.setContent (data))
				data = doc.toByteArray (2);

			const auto markerSize = QString ("<root>\n").size ();
			data.chop (markerSize + 1);
			data = data.mid (markerSize);
			[[fallthrough]];
		}
		case IHaveConsole::PacketFormat::PlainText:
			html += QString::fromUtf8 (data
					.replace ('<', "&lt;")
					.replace ('\n', "<br/>")
					.replace (' ', "&nbsp;")
					.constData ());
			break;
		}
		html += "</font><br />";

		Ui_.PacketsBrowser_->append (html);
	}

	void ConsoleWidget::on_ClearButton__released ()
	{
		Ui_.PacketsBrowser_->clear ();
	}

	void ConsoleWidget::on_EnabledBox__toggled (bool enable)
	{
		AsConsole_->SetConsoleEnabled (enable);
	}
}
}
