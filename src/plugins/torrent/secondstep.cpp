#include <QFileDialog>
#include <plugininterface/proxy.h>
#include "secondstep.h"
#include "settingsmanager.h"

SecondStep::SecondStep (QWidget *parent)
: QWizardPage (parent)
{
	setupUi (this);
	registerField ("Files", FilesWidget_);
}

void SecondStep::on_AddPath__released ()
{
	QStringList paths = QFileDialog::getOpenFileNames (this, tr ("Select one or more paths to add"), SettingsManager::Instance ()->GetLastAddDirectory ());
	if (paths.isEmpty ())
		return;
	
	QStringList files = paths;
	for (int i = 0; i < files.size (); ++i)
	{
		QString path = files.at (i);
		QTreeWidgetItem *item = new QTreeWidgetItem (FilesWidget_);
		item->setText (0, Proxy::Instance ()->MakePrettySize (QFileInfo (path).size ()));
		item->setText (1, path);
	}
}

void SecondStep::on_RemoveSelected__released ()
{
	QList<QTreeWidgetItem*> items = FilesWidget_->selectedItems ();
	qDeleteAll (items);
}

void SecondStep::on_Clear__released ()
{
	FilesWidget_->clear ();
}

