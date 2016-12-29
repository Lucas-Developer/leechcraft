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

#pragma once

#include <memory>
#include <qwebpage.h>
#include <QUrl>
#include <QNetworkRequest>
#include <interfaces/structures.h>
#include <interfaces/core/ihookproxy.h>
#include <interfaces/core/icoreproxyfwd.h>
#include <interfaces/poshuku/poshukutypes.h>

namespace LeechCraft
{
namespace Poshuku
{
class IWebView;
class IProxyObject;

namespace WebKitView
{
	class JSProxy;
	class ExternalProxy;
	class CustomWebView;

	class CustomWebPage : public QWebPage
	{
		Q_OBJECT

		const ICoreProxy_ptr Proxy_;
		IProxyObject * const PoshukuProxy_;

		Qt::MouseButtons MouseButtons_;
		Qt::KeyboardModifiers Modifiers_;

		QUrl LoadingURL_;
		std::shared_ptr<JSProxy> JSProxy_;
		std::shared_ptr<ExternalProxy> ExternalProxy_;
		typedef QMap<QWebFrame*, QWebHistoryItem*> Frame2History_t;
		Frame2History_t Frame2History_;
		PageFormsData_t FilledState_;

		QMap<ErrorDomain, QMap<int, QStringList>> Error2Suggestions_;
	public:
		CustomWebPage (const ICoreProxy_ptr&, IProxyObject*, QObject* = nullptr);

		void SetButtons (Qt::MouseButtons);
		void SetModifiers (Qt::KeyboardModifiers);
		bool supportsExtension (Extension) const;
		bool extension (Extension, const ExtensionOption*, ExtensionReturn*);
	private slots:
		void handleContentsChanged ();
		void handleDatabaseQuotaExceeded (QWebFrame*, QString);
		void handleDownloadRequested (QNetworkRequest);
		void handleFrameCreated (QWebFrame*);
		void handleJavaScriptWindowObjectCleared ();
		void handleGeometryChangeRequested (const QRect&);
		void handleLinkClicked (const QUrl&);
		void handleLinkHovered (const QString&, const QString&, const QString&);
		void handleLoadFinished (bool);
		void handleLoadStarted ();
		void handleUnsupportedContent (QNetworkReply*);
		void handleWindowCloseRequested ();
		void fillForms (QWebFrame*);
	protected:
		virtual bool acceptNavigationRequest (QWebFrame*,
				const QNetworkRequest&, QWebPage::NavigationType);
		virtual QString chooseFile (QWebFrame*, const QString&);
		virtual QObject* createPlugin (const QString&, const QUrl&,
				const QStringList&, const QStringList&);
		virtual QWebPage* createWindow (WebWindowType);
		virtual void javaScriptAlert (QWebFrame*, const QString&);
		virtual bool javaScriptConfirm (QWebFrame*, const QString&);
		virtual void javaScriptConsoleMessage (const QString&, int, const QString&);
		virtual bool javaScriptPrompt (QWebFrame*, const QString&, const QString&, QString*);
		virtual QString userAgentForUrl (const QUrl&) const;
	private:
		bool HandleExtensionProtocolUnknown (const ErrorPageExtensionOption*);
		void FillErrorSuggestions ();
		QString MakeErrorReplyContents (int, const QUrl&,
				const QString&, ErrorDomain = WebKit) const;
		QWebFrame* FindFrame (const QUrl&);
		void HandleForms (QWebFrame*, const QNetworkRequest&,
				QWebPage::NavigationType);
	signals:
		void loadingURL (const QUrl&);
		void storeFormData (const PageFormsData_t&);
		void delayedFillForms (QWebFrame*);

		void webViewCreated (CustomWebView*, bool);

		// Hook support signals
		void hookAcceptNavigationRequest (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebFrame *frame,
				QNetworkRequest request,
				QWebPage::NavigationType type);
		void hookChooseFile (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebFrame *frame,
				QString suggested);
		void hookContentsChanged (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page);
		void hookCreatePlugin (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QString clsid,
				QUrl url,
				QStringList params,
				QStringList values);
		void hookCreateWindow (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebPage::WebWindowType type);
		void hookDatabaseQuotaExceeded (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *sourcePage,
				QWebFrame *sourceFrame,
				QString databaseName);
		void hookDownloadRequested (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *sourcePage,
				QNetworkRequest downloadRequest);
		void hookExtension (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebPage::Extension extension,
				const QWebPage::ExtensionOption* extensionOption,
				QWebPage::ExtensionReturn* extensionReturn);
		void hookFrameCreated (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebFrame *frameCreated);
		void hookGeometryChangeRequested (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QRect rect);
		void hookJavaScriptAlert (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebFrame *frame,
				QString msg);
		void hookJavaScriptConfirm (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebFrame *frame,
				QString msg);
		void hookJavaScriptConsoleMessage (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QString msg,
				int line,
				QString sourceId);
		void hookJavaScriptPrompt (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QWebFrame *frame,
				QString msg,
				QString defValue,
				QString resultString);
		void hookJavaScriptWindowObjectCleared (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *sourcePage,
				QWebFrame *frameCleared);
		void hookLinkClicked (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QUrl url);
		void hookLinkHovered (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QString link,
				QString title,
				QString textContent);
		void hookLoadFinished (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				bool result);
		void hookLoadStarted (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page);
		void hookSupportsExtension (LeechCraft::IHookProxy_ptr proxy,
				const QWebPage *page,
				QWebPage::Extension extension) const;
		void hookUnsupportedContent (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page,
				QNetworkReply *reply);
		void hookWebPageConstructionBegin (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page);
		void hookWebPageConstructionEnd (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page);
		void hookWindowCloseRequested (LeechCraft::IHookProxy_ptr proxy,
				QWebPage *page);
	};
}
}
}
