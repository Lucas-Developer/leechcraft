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

#include "mainwindow.h"
#include <iostream>
#include <algorithm>
#include <QMessageBox>
#include <QCloseEvent>
#include <QCursor>
#include <QShortcut>
#include <QMenu>
#include <QSplashScreen>
#include <QTime>
#include <QDockWidget>
#include <QDesktopWidget>
#include <QWidgetAction>
#include <util/util.h>
#include <util/defaulthookproxy.h>
#include <util/shortcuts/shortcutmanager.h>
#include <interfaces/iactionsexporter.h>
#include <interfaces/ihavetabs.h>
#include "core.h"
#include "commonjobadder.h"
#include "xmlsettingsmanager.h"
#include "iconthemeengine.h"
#include "childactioneventfilter.h"
#include "tagsviewer.h"
#include "application.h"
#include "startupwizard.h"
#include "aboutdialog.h"
#include "newtabmenumanager.h"
#include "tabmanager.h"
#include "coreinstanceobject.h"
#include "coreplugin2manager.h"
#include "entitymanager.h"
#include "rootwindowsmanager.h"

using namespace LeechCraft;
using namespace LeechCraft::Util;

LeechCraft::MainWindow::MainWindow (int screen, bool isPrimary)
: IsPrimary_ (isPrimary)
, TrayIcon_ (0)
, IsShown_ (true)
, WasMaximized_ (false)
, IsQuitting_ (false)
, LeftDockToolbar_ (new QToolBar ())
, RightDockToolbar_ (new QToolBar ())
, TopDockToolbar_ (new QToolBar ())
, BottomDockToolbar_ (new QToolBar ())
{
	installEventFilter (new ChildActionEventFilter (this));

	Ui_.setupUi (this);
	Ui_.MainTabWidget_->SetWindow (this);

	addToolBar (Qt::LeftToolBarArea, LeftDockToolbar_);
	addToolBar (Qt::RightToolBarArea, RightDockToolbar_);
	addToolBar (Qt::TopToolBarArea, TopDockToolbar_);
	addToolBar (Qt::BottomToolBarArea, BottomDockToolbar_);

	if (Application::instance ()->arguments ().contains ("--desktop"))
	{
		setWindowFlags (Qt::FramelessWindowHint);
		connect (qApp->desktop (),
				SIGNAL (workAreaResized (int)),
				this,
				SLOT (handleWorkAreaResized (int)));
	}
}

void LeechCraft::MainWindow::Init ()
{
	setUpdatesEnabled (false);

	hide ();

	Core::Instance ().GetCoreInstanceObject ()->
			GetCorePluginManager ()->RegisterHookable (this);

	InitializeInterface ();

	connect (Core::Instance ().GetNewTabMenuManager (),
			SIGNAL (restoreTabActionAdded (QAction*)),
			this,
			SLOT (handleRestoreActionAdded (QAction*)));

	setUpdatesEnabled (true);

	if (!qobject_cast<Application*> (qApp)->GetVarMap ().count ("minimized"))
	{
		show ();
		activateWindow ();
		raise ();
	}
	else
	{
		IsShown_ = false;
		hide ();
	}

	WasMaximized_ = isMaximized ();
	Ui_.ActionFullscreenMode_->setChecked (isFullScreen ());
	QTimer::singleShot (700,
			this,
			SLOT (doDelayedInit ()));

	auto sm = Core::Instance ().GetCoreInstanceObject ()->GetCoreShortcutManager ();

	FullScreenShortcut_ = new QShortcut (QKeySequence (tr ("F11", "FullScreen")), this);
	FullScreenShortcut_->setContext (Qt::WidgetWithChildrenShortcut);
	connect (FullScreenShortcut_,
			SIGNAL (activated ()),
			this,
			SLOT (handleShortcutFullscreenMode ()));
	sm->RegisterShortcut ("FullScreen", {}, FullScreenShortcut_);

	CloseTabShortcut_ = new QShortcut (QString ("Ctrl+W"),
			this,
			SLOT (handleCloseCurrentTab ()),
			0);
	sm->RegisterShortcut ("CloseTab", {}, CloseTabShortcut_);

	sm->RegisterAction ("Settings", Ui_.ActionSettings_);
	sm->RegisterAction ("Quit", Ui_.ActionQuit_);
}

void LeechCraft::MainWindow::handleShortcutFullscreenMode ()
{
	on_ActionFullscreenMode__triggered (!isFullScreen ());
}

LeechCraft::MainWindow::~MainWindow ()
{
}

SeparateTabWidget* LeechCraft::MainWindow::GetTabWidget () const
{
	return Ui_.MainTabWidget_;
}

QSplitter* LeechCraft::MainWindow::GetMainSplitter () const
{
	return Ui_.MainSplitter_;
}

void LeechCraft::MainWindow::SetAdditionalTitle (const QString& title)
{
	if (title.isEmpty ())
		setWindowTitle ("LeechCraft");
	else
		setWindowTitle (QString ("%1 - LeechCraft").arg (title));
}

QMenu* LeechCraft::MainWindow::GetMainMenu () const
{
	return MenuButton_->menu ();
}

void LeechCraft::MainWindow::HideMainMenu ()
{
	MBAction_->setVisible (false);
}

QToolBar* LeechCraft::MainWindow::GetDockListWidget (Qt::DockWidgetArea area) const
{
	switch (area)
	{
	case Qt::LeftDockWidgetArea:
		return LeftDockToolbar_;
	case Qt::RightDockWidgetArea:
		return RightDockToolbar_;
	case Qt::TopDockWidgetArea:
		return TopDockToolbar_;
	case Qt::BottomDockWidgetArea:
		return BottomDockToolbar_;
	default:
		return 0;
	}
}

void LeechCraft::MainWindow::AddMenus (const QMap<QString, QList<QAction*>>& menus)
{
	Q_FOREACH (const QString& menuName, menus.keys ())
	{
		QMenu *toInsert = 0;
		if (menuName == "view")
			toInsert = MenuView_;
		else if (menuName == "tools")
			toInsert = MenuTools_;
		else
		{
			const auto& actions = MenuButton_->menu ()->actions ();
			for (auto action : actions)
				if (action->menu () &&
					action->text () == menuName)
				{
					toInsert = action->menu ();
					break;
				}
		}

		if (toInsert)
			toInsert->insertActions (toInsert->actions ().value (0, 0),
					menus [menuName]);
		else
		{
			QMenu *menu = new QMenu (menuName, MenuButton_->menu ());
			menu->addActions (menus [menuName]);
			MenuButton_->menu ()->insertMenu (MenuTools_->menuAction (), menu);
		}

		IconThemeEngine::Instance ().UpdateIconSet (menus [menuName]);
	}
}

void LeechCraft::MainWindow::RemoveMenus (const QMap<QString, QList<QAction*>>& menus)
{
	if (IsQuitting_)
		return;

	Q_FOREACH (const QString& menuName, menus.keys ())
	{
		QMenu *toRemove = 0;
		if (menuName == "view")
			toRemove = MenuView_;
		else if (menuName == "tools")
			toRemove = MenuTools_;

		if (toRemove)
			for (auto action : menus [menuName])
				toRemove->removeAction (action);
		else
		{
			auto menu = MenuButton_->menu ();
			for (auto action : menu->actions ())
				if (action->text () == menuName)
				{
					menu->removeAction (action);
					break;
				}
		}
	}
}

QMenu* LeechCraft::MainWindow::createPopupMenu ()
{
	auto menu = QMainWindow::createPopupMenu ();
	for (auto action : menu->actions ())
		if (action->text ().isEmpty ())
			menu->removeAction (action);

	return menu;
}

void LeechCraft::MainWindow::catchError (QString message)
{
	Entity e = Util::MakeEntity ("LeechCraft",
			QString (),
			AutoAccept | OnlyHandle,
			"x-leechcraft/notification");
	e.Additional_ ["Text"] = message;
	e.Additional_ ["Priority"] = PWarning_;
	Core::Instance ().handleGotEntity (e);
}

void LeechCraft::MainWindow::closeEvent (QCloseEvent *e)
{
	e->ignore ();

	if (Core::Instance ().GetRootWindowsManager ()->WindowCloseRequested (this))
		return;

	if (XmlSettingsManager::Instance ()->
			property ("ExitOnClose").toBool ())
		on_ActionQuit__triggered ();
	else
	{
		hide ();
		IsShown_ = false;
	}
}

void LeechCraft::MainWindow::InitializeInterface ()
{
	Ui_.MainTabWidget_->setObjectName ("org_LeechCraft_MainWindow_CentralTabWidget");
	Ui_.MainTabWidget_->SetTabsClosable (true);
	connect (Ui_.ActionAboutQt_,
			SIGNAL (triggered ()),
			qApp,
			SLOT (aboutQt ()));

	MenuView_ = new QMenu (tr ("View"), this);
	MenuView_->addSeparator ();
	MenuView_->addAction (Ui_.ActionShowStatusBar_);
	MenuView_->addAction (Ui_.ActionFullscreenMode_);
	MenuTools_ = new QMenu (tr ("Tools"), this);

#ifdef Q_OS_MAC
	Ui_.ActionFullscreenMode_->setVisible (false);
#endif

	Ui_.ActionAddTask_->setProperty ("ActionIcon", "list-add");
	Ui_.ActionCloseTab_->setProperty ("ActionIcon", "tab-close");
	Ui_.ActionSettings_->setProperty ("ActionIcon", "preferences-system");
	Ui_.ActionSettings_->setMenuRole (QAction::PreferencesRole);
	Ui_.ActionAboutLeechCraft_->setProperty ("ActionIcon", "help-about");
	Ui_.ActionAboutLeechCraft_->setMenuRole (QAction::AboutRole);
	Ui_.ActionAboutQt_->setIcon (qApp->style ()->
			standardIcon (QStyle::SP_MessageBoxQuestion).pixmap (32, 32));
	Ui_.ActionAboutQt_->setMenuRole (QAction::AboutQtRole);
	Ui_.ActionQuit_->setProperty ("ActionIcon", "application-exit");
	Ui_.ActionQuit_->setMenuRole (QAction::QuitRole);
	Ui_.ActionFullscreenMode_->setProperty ("ActionIcon", "view-fullscreen");
	Ui_.ActionFullscreenMode_->setParent (this);

	Ui_.MainTabWidget_->AddAction2TabBar (Ui_.ActionCloseTab_);
	connect (Ui_.MainTabWidget_,
			SIGNAL (newTabMenuRequested ()),
			this,
			SLOT (handleNewTabMenuRequested ()));

	XmlSettingsManager::Instance ()->RegisterObject ("ToolButtonStyle",
			this, "handleToolButtonStyleChanged");
	handleToolButtonStyleChanged ();

	QMenu *menu = new QMenu (this);
	menu->addAction (Ui_.ActionNewWindow_);
	menu->addMenu (Core::Instance ().GetNewTabMenuManager ()->GetNewTabMenu ());
	menu->addSeparator ();
	menu->addAction (Ui_.ActionAddTask_);
	menu->addSeparator ();
	menu->addMenu (MenuTools_);
	menu->addMenu (MenuView_);
	menu->addSeparator ();
	menu->addAction (Ui_.ActionSettings_);
	menu->addSeparator ();
	menu->addAction (Ui_.ActionAboutLeechCraft_);
	menu->addSeparator ();
	menu->addAction (Ui_.ActionRestart_);
	menu->addAction (Ui_.ActionQuit_);

	addAction (menu->menuAction ());

	MenuButton_ = new QToolButton (this);
	MenuButton_->setIcon (QIcon (":/resources/images/leechcraft.svg"));
	MenuButton_->setPopupMode (QToolButton::MenuButtonPopup);
	MenuButton_->setMenu (menu);
	MenuButton_->setPopupMode (QToolButton::InstantPopup);

	SetStatusBar ();
	ReadSettings ();

	MBAction_ = new QWidgetAction (this);
	MBAction_->setDefaultWidget (MenuButton_);
	Ui_.MainTabWidget_->AddAction2TabBarLayout (QTabBar::LeftSide, MBAction_);
}

void LeechCraft::MainWindow::SetStatusBar ()
{
	const int height = statusBar ()->sizeHint ().height ();

	QLBar_ = new QToolBar ();
	QLBar_->setIconSize (QSize (height - 1, height - 1));
	QLBar_->setMaximumHeight (height - 1);
	statusBar ()->addPermanentWidget (QLBar_);
}

void LeechCraft::MainWindow::ReadSettings ()
{
	QSettings settings ("Deviant", "Leechcraft");
	settings.beginGroup ("geometry");
	resize (settings.value ("size", QSize  (1150, 800)).toSize ());
	move   (settings.value ("pos",  QPoint (10, 10)).toPoint ());
	WasMaximized_ = settings.value ("maximized").toBool ();
	WasMaximized_ ? showMaximized () : showNormal ();
	settings.endGroup ();
	settings.beginGroup ("Window");
	Ui_.ActionShowStatusBar_->setChecked (settings.value ("StatusBarEnabled", true).toBool ());
	on_ActionShowStatusBar__triggered ();
	settings.endGroup ();
}

void LeechCraft::MainWindow::WriteSettings ()
{
	QSettings settings ("Deviant", "Leechcraft");
	settings.beginGroup ("geometry");
	settings.setValue ("size", size ());
	settings.setValue ("pos",  pos ());
	settings.setValue ("maximized", isMaximized ());
	settings.endGroup ();
	settings.beginGroup ("Window");
	settings.setValue ("StatusBarEnabled",
			Ui_.ActionShowStatusBar_->isChecked ());
	settings.endGroup ();
}

void LeechCraft::MainWindow::on_ActionAddTask__triggered ()
{
	CommonJobAdder adder (this);
	if (adder.exec () != QDialog::Accepted)
		return;

	QString name = adder.GetString ();
	if (!name.isEmpty ())
		Core::Instance ().TryToAddJob (name);
}

void LeechCraft::MainWindow::on_ActionNewWindow__triggered ()
{
	Core::Instance ().GetRootWindowsManager ()->MakeMainWindow ();
}

void LeechCraft::MainWindow::on_ActionCloseTab__triggered ()
{
	auto rootWM = Core::Instance ().GetRootWindowsManager ();
	rootWM->GetTabManager (this)->remove (Ui_.MainTabWidget_->GetLastContextMenuTab ());
}

void MainWindow::handleCloseCurrentTab ()
{
	auto rootWM = Core::Instance ().GetRootWindowsManager ();
	rootWM->GetTabManager (this)->remove (Ui_.MainTabWidget_->CurrentIndex ());
}

void LeechCraft::MainWindow::on_ActionSettings__triggered ()
{
	Core::Instance ().GetCoreInstanceObject ()->TabOpenRequested ("org.LeechCraft.SettingsPane");
}

void LeechCraft::MainWindow::on_ActionAboutLeechCraft__triggered ()
{
	AboutDialog *dia = new AboutDialog (this);
	dia->setAttribute (Qt::WA_DeleteOnClose);
	dia->show ();
}

void LeechCraft::MainWindow::on_ActionRestart__triggered()
{
	if (QMessageBox::question (this,
				"LeechCraft",
				tr ("Do you really want to restart?"),
				QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	static_cast<Application*> (qApp)->InitiateRestart ();
	QTimer::singleShot (1000,
			qApp,
			SLOT (quit ()));
}

void LeechCraft::MainWindow::on_ActionQuit__triggered ()
{
	if (XmlSettingsManager::Instance ()->property ("ConfirmQuit").toBool ())
	{
		QMessageBox mbox (QMessageBox::Question,
				"LeechCraft",
				tr ("Do you really want to quit?"),
				QMessageBox::Yes | QMessageBox::No,
				this);
		mbox.setDefaultButton (QMessageBox::No);

		QPushButton always (tr ("Always"));
		mbox.addButton (&always, QMessageBox::AcceptRole);

		if (mbox.exec () == QMessageBox::No)
			return;
		else if (mbox.clickedButton () == &always)
			XmlSettingsManager::Instance ()->setProperty ("ConfirmQuit", false);
	}

	setEnabled (false);
	qApp->quit ();
}

void LeechCraft::MainWindow::on_ActionShowStatusBar__triggered ()
{
	statusBar ()->setVisible (Ui_.ActionShowStatusBar_->isChecked ());
}

void LeechCraft::MainWindow::handleQuit ()
{
	WriteSettings ();
	hide ();

	IsQuitting_ = true;

	disconnect (Ui_.MainTabWidget_,
				0,
				0,
				0);

	TrayIcon_->hide ();
	delete TrayIcon_;
}

void LeechCraft::MainWindow::on_ActionFullscreenMode__triggered (bool full)
{
	if (full)
	{
		WasMaximized_ = isMaximized ();
		ShowMenuAndBar (false);
		showFullScreen ();
	}
	else if (WasMaximized_)
	{
		ShowMenuAndBar (true);
		showMaximized ();
		// Because shit happens on X11 otherwise
		QTimer::singleShot (200,
				this,
				SLOT (showMaximized ()));
	}
	else
	{
		ShowMenuAndBar (true);
		showNormal ();
	}
}

namespace
{
	Qt::ToolButtonStyle GetToolButtonStyle ()
	{
		QString style = XmlSettingsManager::Instance ()->
			property ("ToolButtonStyle").toString ();
		if (style == "iconOnly")
			return Qt::ToolButtonIconOnly;
		else if (style == "textOnly")
			return Qt::ToolButtonTextOnly;
		else if (style == "textBesideIcon")
			return Qt::ToolButtonTextBesideIcon;
		else
			return Qt::ToolButtonTextUnderIcon;
	}
};

void LeechCraft::MainWindow::handleToolButtonStyleChanged ()
{
	setToolButtonStyle (GetToolButtonStyle ());
}

void MainWindow::handleShowTrayIconChanged ()
{
	const bool isVisible = XmlSettingsManager::Instance ()->
			property ("ShowTrayIcon").toBool ();

	Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
	emit hookTrayIconVisibilityChanged (proxy, TrayIcon_, isVisible);
	if (proxy->IsCancelled ())
		return;

	TrayIcon_->setVisible (isVisible);
}

void LeechCraft::MainWindow::handleNewTabMenuRequested ()
{
	QMenu *ntmenu = Core::Instance ()
			.GetNewTabMenuManager ()->GetNewTabMenu ();
	ntmenu->popup (QCursor::pos ());
}

void MainWindow::handleRestoreActionAdded (QAction *act)
{
	Ui_.MainTabWidget_->InsertAction2TabBar (Ui_.ActionCloseTab_, act);
}

void LeechCraft::MainWindow::showHideMain ()
{
	IsShown_ = 1 - IsShown_;
	if (IsShown_)
	{
		show ();
		activateWindow ();
		raise ();
	}
	else
		hide ();
}

void LeechCraft::MainWindow::handleTrayIconActivated (QSystemTrayIcon::ActivationReason reason)
{
	switch (reason)
	{
		case QSystemTrayIcon::Context:
		case QSystemTrayIcon::Unknown:
			return;
		case QSystemTrayIcon::DoubleClick:
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::MiddleClick:
			showHideMain ();
			return;
	}
}

void MainWindow::handleWorkAreaResized (int screen)
{
	auto desktop = QApplication::desktop ();
	if (screen != desktop->screenNumber (this))
		return;

	const auto& available = desktop->availableGeometry (this);

	setGeometry (available);
	setFixedSize (available.size ());
}

void LeechCraft::MainWindow::doDelayedInit ()
{
	FillQuickLaunch ();
	FillTray ();
	FillToolMenu ();
	InitializeShortcuts ();

	setAcceptDrops (true);

	new StartupWizard (this);
}

void LeechCraft::MainWindow::FillQuickLaunch ()
{
	Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
	emit hookGonnaFillMenu (proxy);
	if (proxy->IsCancelled ())
		return;

	const auto& exporters = Core::Instance ().GetPluginManager ()->GetAllCastableTo<IActionsExporter*> ();
	Q_FOREACH (auto exp, exporters)
	{
		const auto& map = exp->GetMenuActions ();
		if (!map.isEmpty ())
			AddMenus (map);
	}

	proxy.reset (new Util::DefaultHookProxy);
	emit hookGonnaFillQuickLaunch (proxy);
	if (proxy->IsCancelled ())
		return;

	Q_FOREACH (auto exp, exporters)
	{
		const auto& actions = exp->GetActions (ActionsEmbedPlace::QuickLaunch);
		if (actions.isEmpty ())
			continue;

		IconThemeEngine::Instance ().UpdateIconSet (actions);

		QLBar_->addSeparator ();
		QLBar_->addActions (actions);
	}
}

void LeechCraft::MainWindow::FillTray ()
{
	QMenu *iconMenu = new QMenu (this);
	QMenu *menu = iconMenu->addMenu (tr ("LeechCraft menu"));
	menu->addAction (Ui_.ActionAddTask_);
	menu->addMenu (MenuView_);
	menu->addMenu (MenuTools_);
	iconMenu->addSeparator ();

	const auto& trayMenus = Core::Instance ().GetPluginManager ()->
			GetAllCastableTo<IActionsExporter*> ();
	Q_FOREACH (auto o, trayMenus)
	{
		const auto& actions = o->GetActions (ActionsEmbedPlace::TrayMenu);
		IconThemeEngine::Instance ().UpdateIconSet (actions);
		iconMenu->addActions (actions);
		if (actions.size ())
			iconMenu->addSeparator ();
	}

	iconMenu->addAction (Ui_.ActionQuit_);

	TrayIcon_ = new QSystemTrayIcon (QIcon ("lcicons:/resources/images/leechcraft.svg"), this);
	handleShowTrayIconChanged ();
	TrayIcon_->setContextMenu (iconMenu);
	connect (TrayIcon_,
			SIGNAL (activated (QSystemTrayIcon::ActivationReason)),
			this,
			SLOT (handleTrayIconActivated (QSystemTrayIcon::ActivationReason)));

	emit hookTrayIconCreated (Util::DefaultHookProxy_ptr (new Util::DefaultHookProxy), TrayIcon_);
	XmlSettingsManager::Instance ()->RegisterObject ("ShowTrayIcon",
			this, "handleShowTrayIconChanged");
	handleShowTrayIconChanged ();
}

void LeechCraft::MainWindow::FillToolMenu ()
{
	Q_FOREACH (IActionsExporter *e,
			Core::Instance ().GetPluginManager ()->
				GetAllCastableTo<IActionsExporter*> ())
	{
		const auto& acts = e->GetActions (ActionsEmbedPlace::ToolsMenu);
		IconThemeEngine::Instance ().UpdateIconSet (acts);
		MenuTools_->addActions (acts);
		if (acts.size ())
			MenuTools_->addSeparator ();
	}

	QMenu *ntm = Core::Instance ()
		.GetNewTabMenuManager ()->GetNewTabMenu ();
	Ui_.MainTabWidget_->SetAddTabButtonContextMenu (ntm);

	QMenu *atm = Core::Instance ()
		.GetNewTabMenuManager ()->GetAdditionalMenu ();

	int i = 0;
	Q_FOREACH (QAction *act, atm->actions ())
		Ui_.MainTabWidget_->InsertAction2TabBar (i++, act);
}

void LeechCraft::MainWindow::InitializeShortcuts ()
{
#ifndef Q_OS_MAC
	const auto sysModifier = Qt::CTRL;
#else
	const auto sysModifier = Qt::ALT;
#endif

	auto rootWM = Core::Instance ().GetRootWindowsManager ();
	auto tm = rootWM->GetTabManager (this);

	auto sm = Core::Instance ().GetCoreInstanceObject ()->GetCoreShortcutManager ();

	connect (new QShortcut (QKeySequence ("Ctrl+["), this),
			SIGNAL (activated ()),
			tm,
			SLOT (rotateLeft ()));
	connect (new QShortcut (QKeySequence ("Ctrl+]"), this),
			SIGNAL (activated ()),
			tm,
			SLOT (rotateRight ()));
	connect (new QShortcut (QKeySequence (sysModifier + Qt::Key_Tab), this),
			SIGNAL (activated ()),
			tm,
			SLOT (rotateRight ()));
	connect (new QShortcut (QKeySequence (sysModifier + Qt::SHIFT + Qt::Key_Tab), this),
			SIGNAL (activated ()),
			tm,
			SLOT (rotateLeft ()));

	auto leftShortcut = new QShortcut (QKeySequence ("Ctrl+PgUp"), this);
	connect (leftShortcut,
			SIGNAL (activated ()),
			tm,
			SLOT (rotateLeft ()));
	sm->RegisterShortcut ("SwitchToLeftTab", {}, leftShortcut, true);
	auto rightShortcut = new QShortcut (QKeySequence ("Ctrl+PgDown"), this);
	connect (rightShortcut,
			SIGNAL (activated ()),
			tm,
			SLOT (rotateRight ()));
	sm->RegisterShortcut ("SwitchToRightTab", {}, rightShortcut, true);

	connect (new QShortcut (QKeySequence (Qt::CTRL + Qt::Key_T), this),
			SIGNAL (activated ()),
			Ui_.MainTabWidget_,
			SLOT (handleNewTabShortcutActivated ()));

	auto prevTabSC = new QShortcut (QKeySequence (sysModifier + Qt::Key_Space), this);
	sm->RegisterShortcut ("SwitchToPrevTab", ActionInfo (), prevTabSC, true);
	connect (prevTabSC,
			SIGNAL (activated ()),
			Ui_.MainTabWidget_,
			SLOT (setPreviousTab ()));

	for (int i = 0; i < 10; ++i)
	{
		QString seqStr = QString ("Ctrl+\\, %1").arg (i);
		QShortcut *sc = new QShortcut (QKeySequence (seqStr), this);
		sc->setProperty ("TabNumber", i);

		connect (sc,
				SIGNAL (activated ()),
				tm,
				SLOT (navigateToTabNumber ()));
	}
}

void LeechCraft::MainWindow::ShowMenuAndBar (bool show)
{
	if (XmlSettingsManager::Instance ()->property ("ToolBarVisibilityManipulation").toBool ())
		Ui_.ActionFullscreenMode_->setChecked (!show);
}

void LeechCraft::MainWindow::keyPressEvent (QKeyEvent *e)
{
	int index = (e->key () & ~Qt::CTRL) - Qt::Key_0;
	if (index == 0)
		index = 10;
	--index;
	if (index >= 0 && index < std::min (10, Ui_.MainTabWidget_->WidgetCount ()))
		Ui_.MainTabWidget_->setCurrentTab (index);
}

void MainWindow::dragEnterEvent (QDragEnterEvent *event)
{
	auto mimeData = event->mimeData ();
	for (const QString& format : mimeData->formats ())
	{
		const Entity& e = Util::MakeEntity (mimeData->data (format),
				QString (),
				FromUserInitiated,
				format);

		if (EntityManager ().CouldHandle (e))
		{
			event->acceptProposedAction ();
			return;
		}
	}

	QMainWindow::dragEnterEvent (event);
}

void MainWindow::dropEvent (QDropEvent *event)
{
	auto mimeData = event->mimeData ();
	Q_FOREACH (const QString& format, mimeData->formats ())
	{
		const Entity& e = Util::MakeEntity (mimeData->data (format),
				QString (),
				FromUserInitiated,
				format);

		if (EntityManager ().HandleEntity (e))
		{
			event->acceptProposedAction ();
			break;
		}
	}

	QWidget::dropEvent (event);
}

