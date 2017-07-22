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
#include "keywordsmanagerwidget.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>
#include <QStandardItemModel>
#include "keywords.h"
#include "editkeyworddialog.h"

namespace LeechCraft
{
namespace Poshuku
{
namespace Keywords
{
	KeywordsManagerWidget::KeywordsManagerWidget (QStandardItemModel *model, Plugin *plugin)
	: Model_ (model)
	, Plugin_ (plugin)
	, Keywords_ (QCoreApplication::organizationName (),
			QCoreApplication::applicationName () + "_Poshuku_Keywords")
	{
		Ui_.setupUi (this);
		Ui_.Items_->setModel (Model_);
	}

	void KeywordsManagerWidget::on_Add__released ()
	{
		EditKeywordDialog addDialog { {}, {} };

		if (addDialog.exec () != QDialog::Accepted)
			return;

		const QString& keyword = addDialog.GetKeyword ();
		const QString& url = addDialog.GetUrl ();

		if (url.isEmpty () || keyword.isEmpty ())
			return;

		bool alreadyExists = Keywords_.allKeys ().contains (keyword);

		if (alreadyExists &&
				QMessageBox::question (this,
						tr ("Keyword already exists"),
						tr ("The keyword %1 already exists. Do you want to update the URL for this keyword?")
							.arg ("<em>" + keyword + "</em>"),
						QMessageBox::Yes | QMessageBox::No,
						QMessageBox::Yes) == QMessageBox::No)
				return;

		Keywords_.setValue (keyword, url);

		if (alreadyExists)
		{
			for (int i = 0; i < Model_->rowCount (); i++)
				if (Model_->item (i, 0)->text () == keyword)
				{
					Model_->item (i, 1)->setText (url);
					break;
				}
		}
		else
		{
			const QList<QStandardItem*> items
			{
				new QStandardItem (keyword),
				new QStandardItem (url)
			};
			for (const auto item : items)
				item->setEditable (false);
			Model_->appendRow (items);
		}
		Plugin_->UpdateKeywords (keyword, url);
	}

	void KeywordsManagerWidget::on_Modify__released ()
	{
		const auto& selected = Ui_.Items_->currentIndex ();
		if (!selected.isValid ())
			return;

		const auto row = selected.row ();

		const auto& keyword = Model_->item (row, 0)->text ();
		const auto& url = Model_->item (row, 1)->text ();
		EditKeywordDialog editDialog { url, keyword };

		if (editDialog.exec () != QDialog::Accepted)
			return;

		if (keyword == editDialog.GetKeyword () && url == editDialog.GetUrl ())
			return;

		if (keyword != editDialog.GetKeyword ())
			Keywords_.remove (keyword);

		Keywords_.setValue (editDialog.GetKeyword (), editDialog.GetUrl ());
		Model_->item (row, 0)->setText (editDialog.GetKeyword ());
		Model_->item (row, 1)->setText (editDialog.GetUrl ());
		Plugin_->UpdateKeywords (editDialog.GetKeyword (), editDialog.GetUrl ());
	}

	void KeywordsManagerWidget::on_Remove__released ()
	{
		const auto& selected = Ui_.Items_->currentIndex ();

		if (!selected.isValid ())
			return;

		const auto& keyword = Model_->item (selected.row (), 0)->text ();

		Keywords_.remove (keyword);
		Model_->removeRow (selected.row ());
		Plugin_->RemoveKeyword (keyword);
	}
}
}
}
