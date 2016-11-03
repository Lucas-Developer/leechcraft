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

#include <QObject>
#include <QtDBus>

class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;

namespace LeechCraft
{
namespace LMP
{
class Player;

namespace MPRIS
{
	class MediaPlayer2Adaptor: public QDBusAbstractAdaptor
	{
		Q_OBJECT
		Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
		Q_CLASSINFO("D-Bus Introspection", ""
	"  <interface name=\"org.mpris.MediaPlayer2\">\n"
	"    <method name=\"Raise\"/>\n"
	"    <method name=\"Quit\"/>\n"
	"    <property access=\"read\" type=\"b\" name=\"CanQuit\"/>\n"
	"    <property access=\"read\" type=\"b\" name=\"CanSetFullscreen\"/>\n"
	"    <property access=\"read\" type=\"b\" name=\"CanQuit\"/>\n"
	"    <property access=\"read\" type=\"b\" name=\"HasTrackList\"/>\n"
	"    <property access=\"read\" type=\"s\" name=\"Identity\"/>\n"
	"    <property access=\"read\" type=\"s\" name=\"DesktopEntry\"/>\n"
	"    <property access=\"read\" type=\"as\" name=\"SupportedUriSchemes\"/>\n"
	"    <property access=\"read\" type=\"as\" name=\"SupportedMimeTypes\"/>\n"
	"  </interface>\n"
			"")

		QObject * const Tab_;
	public:
		MediaPlayer2Adaptor (QObject*, Player*);

	public:
		Q_PROPERTY (bool CanQuit READ GetCanQuit)
		bool GetCanQuit () const;

		Q_PROPERTY (bool CanSetFullscreen READ GetCanSetFullscreen)
		bool GetCanSetFullscreen () const;

		Q_PROPERTY (QString DesktopEntry READ GetDesktopEntry)
		QString GetDesktopEntry () const;

		Q_PROPERTY (bool HasTrackList READ GetHasTrackList)
		bool GetHasTrackList () const;

		Q_PROPERTY (QString Identity READ GetIdentity)
		QString GetIdentity () const;

		Q_PROPERTY (QStringList SupportedMimeTypes READ GetSupportedMimeTypes)
		QStringList GetSupportedMimeTypes () const;

		Q_PROPERTY (QStringList SupportedUriSchemes READ GetSupportedUriSchemes)
		QStringList GetSupportedUriSchemes () const;
	public slots:
		void Quit ();
		void Raise ();
	};
}
}
}
