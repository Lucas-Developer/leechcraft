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

#include "xep0313reqiq.h"
#include <QDomElement>
#include <QtDebug>
#include <QXmppResultSet.h>
#include "xep0313manager.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	Xep0313ReqIq::Xep0313ReqIq (const QString& jid, const QString& startId, int count, Direction dir)
	: QXmppIq { QXmppIq::Get }
	, JID_ { jid }
	, ItemId_ { startId }
	, Count_ { count }
	, Dir_ { dir }
	{
	}

	void Xep0313ReqIq::parseElementFromChild (const QDomElement& element)
	{
		QXmppIq::parseElementFromChild (element);

		const auto& queryElem = element.firstChildElement ("query");
		JID_ = queryElem.firstChildElement ("with").text ();

		QXmppResultSetQuery q;
		q.parse (queryElem.firstChildElement ("set"));

		Count_ = q.max ();

		Dir_ = Direction::Unspecified;
		if (!q.after ().isNull ())
		{
			ItemId_ = q.after ().toLatin1 ();
			Dir_ = Direction::After;
		}
		else if (!q.before ().isNull ())
		{
			ItemId_ = q.before ().toLatin1 ();
			Dir_ = Direction::Before;
		}
	}

	void Xep0313ReqIq::toXmlElementFromChild (QXmlStreamWriter *writer) const
	{
		QXmppIq::toXmlElementFromChild (writer);

		writer->writeStartElement ("query");
		writer->writeAttribute ("xmlns", Xep0313Manager::GetNsUri ());

		if (!JID_.isEmpty ())
			writer->writeTextElement ("with", JID_);

		if (Count_ > 0 || !ItemId_.isNull ())
		{
			QXmppResultSetQuery q;
			if (Count_ > 0)
				q.setMax (Count_);
			if (!ItemId_.isNull ())
			{
				switch (Dir_)
				{
				case Direction::After:
					q.setAfter (ItemId_);
					break;
				case Direction::Before:
					q.setBefore (ItemId_);
					break;
				default:
					break;
				}
			}
			q.toXml (writer);
		}

		writer->writeEndElement ();
	}
}
}
}
