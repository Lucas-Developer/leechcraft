#include "core.h"
#include <algorithm>
#include <memory>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <QIcon>
#include <QtDebug>
#include <plugininterface/tagscompletionmodel.h>
#include "browserwidget.h"
#include "customwebview.h"
#include "favoritesmodel.h"
#include "addtofavoritesdialog.h"
#include "xmlsettingsmanager.h"
#include "filtermodel.h"

Core::Core ()
{
	FavoritesModel_ = new FavoritesModel (this);

	FavoritesFilterModel_ = new FilterModel (this);
	FavoritesFilterModel_->setSourceModel (FavoritesModel_);
	FavoritesFilterModel_->setDynamicSortFilter (true);

	FavoriteTagsCompletionModel_ = new TagsCompletionModel (this);
	FavoriteTagsCompletionModel_->
		UpdateTags (XmlSettingsManager::Instance ()->
				Property ("FavoriteTags", tr ("untagged")).toStringList ());
	connect (FavoriteTagsCompletionModel_,
			SIGNAL (tagsUpdated (const QStringList&)),
			this,
			SLOT (favoriteTagsUpdated (const QStringList&)));
}

Core& Core::Instance ()
{
	static Core core;
	return core;
}

void Core::Release ()
{
}

bool Core::IsValidURL (const QString& url) const
{
	return QUrl (url).isValid ();
}

BrowserWidget* Core::NewURL (const QString& url)
{
	BrowserWidget *widget = new BrowserWidget;
	widget->SetURL (QUrl (url));

	connect (widget,
			SIGNAL (titleChanged (const QString&)),
			this,
			SLOT (handleTitleChanged (const QString&)));
	connect (widget,
			SIGNAL (iconChanged (const QIcon&)),
			this,
			SLOT (handleIconChanged (const QIcon&)));
	connect (widget,
			SIGNAL (needToClose ()),
			this,
			SLOT (handleNeedToClose ()));
	connect (widget,
			SIGNAL (addToFavorites (const QString&, const QString&)),
			this,
			SLOT (handleAddToFavorites (const QString&, const QString&)));

	Widgets_.push_back (widget);

	emit addNewTab (tr ("Loading..."), widget);
	return widget;
}

CustomWebView* Core::MakeWebView ()
{
	return NewURL ("")->GetView ();
}

FilterModel* Core::GetFavoritesModel () const
{
	return FavoritesFilterModel_;
}

TagsCompletionModel* Core::GetFavoritesTagsCompletionModel () const
{
	return FavoriteTagsCompletionModel_;
}

void Core::handleTitleChanged (const QString& newTitle)
{
	emit changeTabName (dynamic_cast<QWidget*> (sender ()), newTitle);
}

void Core::handleIconChanged (const QIcon& newIcon)
{
	emit changeTabIcon (dynamic_cast<QWidget*> (sender ()), newIcon);
}

void Core::handleNeedToClose ()
{
	BrowserWidget *w = dynamic_cast<BrowserWidget*> (sender ());
	emit removeTab (w);

	std::remove (Widgets_.begin (), Widgets_.end (), w);
	w->deleteLater ();
}

void Core::handleAddToFavorites (const QString& title, const QString& url)
{
	std::auto_ptr<AddToFavoritesDialog> dia (new AddToFavoritesDialog (title,
				url,
				GetFavoritesTagsCompletionModel (),
				qApp->activeWindow ()));
	if (dia->exec () == QDialog::Rejected)
		return;

	FavoritesModel_->AddItem (dia->GetTitle (), url, dia->GetTags ());
	FavoriteTagsCompletionModel_->UpdateTags (dia->GetTags ());
}

void Core::favoriteTagsUpdated (const QStringList& tags)
{
	XmlSettingsManager::Instance ()->setProperty ("FavoriteTags", tags);
}

