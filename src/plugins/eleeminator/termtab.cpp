/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2014  Georg Rudoy
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

#include "termtab.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QToolButton>
#include <QShortcut>
#include <QFontDialog>
#include <QUrl>
#include <QProcessEnvironment>
#include <QtDebug>
#include <qtermwidget.h>
#include <util/sll/slotclosure.h>
#include <util/xpc/util.h>
#include <util/shortcuts/shortcutmanager.h>
#include <interfaces/core/ientitymanager.h>
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Eleeminator
{
	TermTab::TermTab (const ICoreProxy_ptr& proxy, Util::ShortcutManager *scMgr,
			const TabClassInfo& tc, QObject *plugin)
	: CoreProxy_ { proxy }
	, TC_ (tc)
	, ParentPlugin_ { plugin }
	, Toolbar_ { new QToolBar { tr ("Terminal toolbar") } }
	, Term_ { new QTermWidget { false } }
	{
		auto lay = new QVBoxLayout;
		lay->setContentsMargins (0, 0, 0, 0);
		setLayout (lay);

		lay->addWidget (Term_);

		Term_->setFlowControlEnabled (true);
		Term_->setFlowControlWarningEnabled (true);
		Term_->setScrollBarPosition (QTermWidget::ScrollBarRight);

		auto systemEnv = QProcessEnvironment::systemEnvironment ();
		if (systemEnv.value ("TERM") != "xterm")
			systemEnv.remove ("TERM");
		if (!systemEnv.contains ("TERM"))
		{
			systemEnv.insert ("TERM", "xterm");
			Term_->setEnvironment (systemEnv.toStringList ());
		}

		Term_->startShellProgram ();

		connect (Term_,
				SIGNAL (finished ()),
				this,
				SLOT (handleFinished ()));

		connect (Term_,
				SIGNAL (urlActivated (QUrl)),
				this,
				SLOT (handleUrlActivated (QUrl)));

		auto savedFontVar = XmlSettingsManager::Instance ().property ("Font");
		if (!savedFontVar.isNull () && savedFontVar.canConvert<QFont> ())
			Term_->setTerminalFont (savedFontVar.value<QFont> ());

		QTimer::singleShot (0,
				Term_,
				SLOT (setFocus ()));

		SetupToolbar ();
		SetupShortcuts (scMgr);
	}

	TabClassInfo TermTab::GetTabClassInfo () const
	{
		return TC_;
	}

	QObject* TermTab::ParentMultiTabs ()
	{
		return ParentPlugin_;
	}

	QToolBar* TermTab::GetToolBar () const
	{
		return Toolbar_;
	}

	void TermTab::Remove ()
	{
		handleFinished ();
	}

	void TermTab::SetupToolbar ()
	{
		SetupColorsButton ();
		SetupFontsButton ();
	}

	void TermTab::SetupColorsButton ()
	{
		auto colorMenu = new QMenu { tr ("Color scheme"), this };
		colorMenu->menuAction ()->setProperty ("ActionIcon", "fill-color");
		connect (colorMenu,
				SIGNAL (triggered (QAction*)),
				this,
				SLOT (setColorScheme (QAction*)));
		connect (colorMenu,
				SIGNAL (hovered (QAction*)),
				this,
				SLOT (previewColorScheme (QAction*)));
		connect (colorMenu,
				SIGNAL (aboutToHide ()),
				this,
				SLOT (stopColorSchemePreview ()));

		const auto& lastScheme = XmlSettingsManager::Instance ()
				.Property ("LastColorScheme", "Linux").toString ();

		const auto colorActionGroup = new QActionGroup { colorMenu };
		for (const auto& colorScheme : QTermWidget::availableColorSchemes ())
		{
			auto act = colorMenu->addAction (colorScheme);
			act->setCheckable (true);
			act->setProperty ("ER/ColorScheme", colorScheme);

			if (colorScheme == lastScheme)
			{
				act->setChecked (true);
				setColorScheme (act);
			}

			colorActionGroup->addAction (act);
		}

		auto colorButton = new QToolButton { Toolbar_ };
		colorButton->setPopupMode (QToolButton::InstantPopup);
		colorButton->setMenu (colorMenu);
		colorButton->setProperty ("ActionIcon", "fill-color");

		Toolbar_->addWidget (colorButton);
	}

	void TermTab::SetupFontsButton ()
	{
		const auto action = Toolbar_->addAction (tr ("Select font..."),
				this, SLOT (selectFont ()));
		action->setProperty ("ActionIcon", "preferences-desktop-font");
	}

	void TermTab::SetupShortcuts (Util::ShortcutManager *manager)
	{
		auto copySc = new QShortcut { { "Ctrl+Shift+C" }, Term_, SLOT (copyClipboard ()) };
		manager->RegisterShortcut ("org.LeechCraft.Eleeminator.Copy", {}, copySc);
		auto pasteSc = new QShortcut { { "Ctrl+Shift+V" }, Term_, SLOT (pasteClipboard ()) };
		manager->RegisterShortcut ("org.LeechCraft.Eleeminator.Paste", {}, pasteSc);

		auto closeSc = new QShortcut { QString { "Ctrl+Shift+W" }, Term_ };
		new Util::SlotClosure<Util::NoDeletePolicy>
		{
			[this] { Remove (); },
			closeSc,
			SIGNAL (activated ()),
			this
		};

		manager->RegisterShortcut ("org.LeechCraft.Eleeminator.Close", {}, closeSc);
	}

	void TermTab::setColorScheme (QAction *schemeAct)
	{
		const auto& colorScheme = schemeAct->property ("ER/ColorScheme").toString ();
		if (colorScheme.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "empty color scheme for"
					<< schemeAct;
			return;
		}

		schemeAct->setChecked (true);

		Term_->setColorScheme (colorScheme);
		CurrentColorScheme_ = colorScheme;

		XmlSettingsManager::Instance ().setProperty ("LastColorScheme", colorScheme);
	}

	void TermTab::previewColorScheme (QAction *schemeAct)
	{
		const auto& colorScheme = schemeAct->property ("ER/ColorScheme").toString ();
		if (colorScheme.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "empty color scheme for"
					<< schemeAct;
			return;
		}

		Term_->setColorScheme (colorScheme);
	}

	void TermTab::stopColorSchemePreview ()
	{
		Term_->setColorScheme (CurrentColorScheme_);
	}

	void TermTab::selectFont ()
	{
		const auto& currentFont = Term_->getTerminalFont ();
		auto savedFont = XmlSettingsManager::Instance ()
				.Property ("Font", QVariant::fromValue (currentFont)).value<QFont> ();

		bool ok = false;
		const auto& font = QFontDialog::getFont (&ok, currentFont, this);
		if (!ok)
			return;

		Term_->setTerminalFont (font);

		XmlSettingsManager::Instance ().setProperty ("Font", QVariant::fromValue (font));
	}

	void TermTab::handleUrlActivated (const QUrl& url)
	{
		const auto& entity = Util::MakeEntity (url, {}, TaskParameter::FromUserInitiated);
		CoreProxy_->GetEntityManager ()->HandleEntity (entity);
	}

	void TermTab::handleFinished ()
	{
		emit remove (this);
		deleteLater ();
	}
}
}
