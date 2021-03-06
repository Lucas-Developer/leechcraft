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

#include "filter.h"
#include <QDataStream>
#include <QtDebug>

namespace LeechCraft
{
namespace Poshuku
{
namespace CleanWeb
{
	QDataStream& operator<< (QDataStream& out, const FilterOption& opt)
	{
		qint8 version = 3;
		out << version
			<< static_cast<qint8> (opt.Case_)
			<< static_cast<qint8> (opt.MatchType_)
			<< opt.Domains_
			<< opt.NotDomains_
			<< static_cast<qint8> (opt.ThirdParty_);
		return out;
	}

	QDataStream& operator>> (QDataStream& in, FilterOption& opt)
	{
		qint8 version = 0;
		in >> version;

		if (version < 1 || version > 3)
		{
			qWarning () << Q_FUNC_INFO
				<< "unknown version"
				<< version;
			return in;
		}

		qint8 cs;
		in >> cs;
		opt.Case_ = cs ?
			Qt::CaseInsensitive :
			Qt::CaseSensitive;
		qint8 mt;
		in >> mt;
		opt.MatchType_ = static_cast<FilterOption::MatchType> (mt);
		in >> opt.Domains_
			>> opt.NotDomains_;

		if (version == 2)
		{
			bool abort = false;
			in >> abort;
			opt.ThirdParty_ = abort ?
					FilterOption::ThirdParty::Yes :
					FilterOption::ThirdParty::Unspecified;
		}
		if (version >= 3)
		{
			qint8 tpVal;
			in >> tpVal;
			opt.ThirdParty_ = static_cast<FilterOption::ThirdParty> (tpVal);
		}

		return in;
	}

	bool operator== (const FilterOption& f1, const FilterOption& f2)
	{
		return f1.ThirdParty_ == f2.ThirdParty_ &&
				f1.Case_ == f2.Case_ &&
				f1.MatchType_ == f2.MatchType_ &&
				f1.Domains_ == f2.Domains_ &&
				f1.NotDomains_ == f2.NotDomains_;
	}

	bool operator!= (const FilterOption& f1, const FilterOption& f2)
	{
		return !(f1 == f2);
	}

	QDebug operator<< (QDebug dbg, const FilterOption& option)
	{
		dbg << "FilterOption {"
				<< "CS:" << (option.Case_ == Qt::CaseSensitive) << "; "
				<< "Match type:" << option.MatchType_ << "; "
				<< "Match objects:" << option.MatchObjects_ << "; "
				<< "Domains:" << option.Domains_ << "; "
				<< "!domains:" << option.NotDomains_ << "; "
				<< "Selector:" << option.HideSelector_ << "; "
				<< "Third party requests:" << static_cast<int> (option.ThirdParty_)
				<< "}";
		return dbg;
	}

	QDebug operator<< (QDebug dbg, const FilterItem& item)
	{
		dbg << "FilterItem {"
				<< "RX:" << item.RegExp_.GetPattern () << "; "
				<< "Plain: " << item.PlainMatcher_ << ": "
				<< "Opts: " << item.Option_
				<< "}";
		return dbg;
	}

	QDataStream& operator<< (QDataStream& out, const FilterItem& item)
	{
		out << static_cast<quint8> (2)
			<< QString::fromUtf8 (item.PlainMatcher_)
			<< item.RegExp_.GetPattern ()
			<< static_cast<quint8> (item.RegExp_.GetCaseSensitivity ())
			<< item.Option_;
		return out;
	}

	QDataStream& operator>> (QDataStream& in, FilterItem& item)
	{
		quint8 version = 0;
		in >> version;
		if (version < 1 || version > 2)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown version"
					<< version;
			return in;
		}

		QString origStr;
		in >> origStr;
		item.PlainMatcher_ = origStr.toUtf8 ();
		if (version == 1)
		{
			QRegExp rx;
			in >> rx;
			item.RegExp_ = Util::RegExp (rx.pattern (), rx.caseSensitivity ());
		}
		else if (version == 2)
		{
			QString str;
			quint8 cs;
			in >> str >> cs;
			item.RegExp_ = Util::RegExp (str, static_cast<Qt::CaseSensitivity> (cs));
		}
		in >> item.Option_;
		return in;
	}

	Filter& Filter::operator+= (const Filter& f)
	{
		Filters_ << f.Filters_;
		Exceptions_ << f.Exceptions_;
		return *this;
	}
}
}
}
