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

#include "firstpage.h"
#include <QVariant>
#include "abstractimporter.h"

namespace LeechCraft
{
namespace NewLife
{
	FirstPage::FirstPage (QWidget *parent)
	: QWizardPage (parent)
	{
		Ui_.setupUi (this);
	}

	int FirstPage::nextId () const
	{
		return StartPages_ [GetImporter ()];
	}

	void FirstPage::SetupImporter (AbstractImporter *ai)
	{
		const auto& names = ai->GetNames ();
		const auto& icons = ai->GetIcons ();
		for (int i = 0; i < std::min (names.size (), icons.size ()); ++i)
			Ui_.SourceApplication_->addItem (icons.at (i),
					names.at (i),
					QVariant::fromValue<QObject*> (ai));

		QList<QWizardPage*> pages = ai->GetWizardPages ();
		if (pages.size ())
		{
			QWizardPage *first = pages.takeFirst ();
			StartPages_ [ai] = wizard ()->addPage (first);
			for (const auto page : pages)
				wizard ()->addPage (page);
		}
	}

	AbstractImporter* FirstPage::GetImporter () const
	{
		int currentIndex = Ui_.SourceApplication_->currentIndex ();
		if (currentIndex == -1)
			return 0;

		QObject *importerObject = Ui_.SourceApplication_->
			itemData (currentIndex).value<QObject*> ();
		return qobject_cast<AbstractImporter*> (importerObject);
	}

	QString FirstPage::GetSelectedName () const
	{
		return Ui_.SourceApplication_->currentText ();
	}
}
}
