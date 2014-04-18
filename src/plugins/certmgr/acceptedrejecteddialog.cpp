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

#include "acceptedrejecteddialog.h"
#include <algorithm>
#include <QStandardItemModel>
#include <QMessageBox>
#include "exceptionsmodel.h"

namespace LeechCraft
{
namespace CertMgr
{
	AcceptedRejectedDialog::AcceptedRejectedDialog (ICoreProxy_ptr proxy)
	: Proxy_ { proxy }
	, CoreSettings_ { QCoreApplication::organizationName (),
			QCoreApplication::applicationName () }
	, Model_ { new ExceptionsModel { CoreSettings_, this } }
	{
		CoreSettings_.beginGroup ("SSL exceptions");

		Model_->setHorizontalHeaderLabels ({ tr ("Address"), tr ("State") });
		PopulateModel ();

		Ui_.setupUi (this);
		Ui_.View_->setModel (Model_);

		connect (Ui_.View_->selectionModel (),
				SIGNAL (selectionChanged (QItemSelection, QItemSelection)),
				this,
				SLOT (handleSelectionChanged ()));
		handleSelectionChanged ();
	}

	AcceptedRejectedDialog::~AcceptedRejectedDialog ()
	{
		CoreSettings_.endGroup ();
	}

	void AcceptedRejectedDialog::PopulateModel ()
	{
		auto keys = CoreSettings_.allKeys ();
		std::sort (keys.begin (), keys.end ());

		for (const auto& key : keys)
			Model_->Add (key, CoreSettings_.value (key).toBool ());
	}

	void AcceptedRejectedDialog::on_RemoveButton__released ()
	{
		auto selected = Ui_.View_->selectionModel ()->selectedRows ();
		if (selected.isEmpty ())
			return;

		if (QMessageBox::question (this,
					tr ("Remove exceptions"),
					tr ("Are you sure you want to remove %n exception(s)?",
						0, selected.size ()),
					QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
			return;

		std::sort (selected.begin (), selected.end (),
				[] (const QModelIndex& i1, const QModelIndex& i2)
					{ return i1.row () > i2.row (); });

		for (const auto& item : selected)
		{
			const auto& title = item.sibling (item.row (), 0).data ().toString ();

			CoreSettings_.remove (title);

			Model_->removeRow (item.row ());
		}
	}

	void AcceptedRejectedDialog::handleSelectionChanged ()
	{
		const auto& selected = Ui_.View_->selectionModel ()->selectedRows ();
		Ui_.RemoveButton_->setEnabled (!selected.isEmpty ());
	}
}
}
