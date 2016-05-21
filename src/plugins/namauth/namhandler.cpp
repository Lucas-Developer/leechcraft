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

#include "namhandler.h"
#include <QNetworkAccessManager>
#include <QFontMetrics>
#include <QAuthenticator>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QApplication>
#include "authenticationdialog.h"
#include "storagebackend.h"

namespace LeechCraft
{
namespace NamAuth
{
	NamHandler::NamHandler (StorageBackend *sb, QNetworkAccessManager *nam)
	: QObject { nam }
	, SB_ { sb }
	, NAM_ { nam }
	{
		connect (nam,
				SIGNAL (authenticationRequired (QNetworkReply*, QAuthenticator*)),
				this,
				SLOT (handleAuthentication (QNetworkReply*, QAuthenticator*)));
		connect (nam,
				SIGNAL (proxyAuthenticationRequired (QNetworkProxy, QAuthenticator*)),
				this,
				SLOT (handleAuthentication (QNetworkProxy, QAuthenticator*)));
	}

	void NamHandler::DoCommonAuth (const QString& msg, QAuthenticator *authen)
	{
		const auto& realm = authen->realm ();

		auto suggestedUser = authen->user ();
		auto suggestedPassword = authen->password ();

		if (suggestedUser.isEmpty ())
			SB_->GetAuth (realm, suggestedUser, suggestedPassword);

		AuthenticationDialog dia (msg, suggestedUser, suggestedPassword, qApp->activeWindow ());
		if (dia.exec () == QDialog::Rejected)
			return;

		const auto& login = dia.GetLogin ();
		const auto& password = dia.GetPassword ();
		authen->setUser (login);
		authen->setPassword (password);

		if (dia.ShouldSave ())
			SB_->SetAuth (realm, login, password);
	}

	void NamHandler::handleAuthentication (QNetworkReply *reply,
			QAuthenticator *authen)
	{
		const auto& msg = tr ("%1 (%2) requires authentication.")
				.arg (authen->realm ())
				.arg ("<em>" + reply->url ().toString () + "</em>");

		DoCommonAuth (msg, authen);
	}

	void NamHandler::handleAuthentication (const QNetworkProxy& proxy,
			QAuthenticator *authen)
	{
		const auto& msg = tr ("%1 (%2) requires authentication.")
				.arg (authen->realm ())
				.arg ("<em>" + proxy.hostName () + "</em>");

		DoCommonAuth (msg, authen);
	}
}
}
