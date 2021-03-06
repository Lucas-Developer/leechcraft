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

#include "geoip.h"
#include <QStringList>
#include <QFile>
#include <QtDebug>
#include <util/sll/monad.h>

#ifdef ENABLE_GEOIP
#include <GeoIP.h>
#endif

namespace LeechCraft
{
namespace BitTorrent
{
#ifdef ENABLE_GEOIP
	namespace
	{
		boost::optional<QString> FindDB ()
		{
			const QStringList geoipCands
			{
				"/usr/share/GeoIP",
				"/usr/local/share/GeoIP",
				"/var/lib/GeoIP"
			};

			for (const auto& cand : geoipCands)
			{
				const auto& name = cand + "/GeoIP.dat";
				if (QFile::exists (name))
					return { name };
			}

			return {};
		}
	}

	GeoIP::GeoIP ()
	{
		using Util::operator>>;

		const auto maybeImpl = FindDB () >>
				[] (const QString& path) -> boost::optional<ImplPtr_t>
				{
					const auto geoip = GeoIP_open (path.toStdString ().c_str (), GEOIP_STANDARD);

					qDebug () << Q_FUNC_INFO << "loading GeoIP from" << path << geoip;

					if (!geoip)
						return {};
					return { { geoip, &GeoIP_delete } };
				};
		Impl_ = maybeImpl.value_or (ImplPtr_t {});
	}

	namespace
	{
		boost::optional<QString> Code2Str (const char *code)
		{
			if (!code)
				return {};

			return QString::fromLatin1 (code, 2).toLower ();
		}
	}

	boost::optional<QString> GeoIP::GetCountry (const libtorrent::address& addr) const
	{
		if (!Impl_)
			return {};

		if (addr.is_v4 ())
			return Code2Str (GeoIP_country_code_by_ipnum (Impl_.get (), addr.to_v4 ().to_ulong ()));

		if (addr.is_v6 ())
		{
			const auto& bytes = addr.to_v6 ().to_bytes ();

			in6_addr in6addr;

			constexpr auto bytesSize = std::tuple_size<std::decay_t<decltype (bytes)>>::value;
			static_assert (sizeof (in6addr.__in6_u.__u6_addr8) == bytesSize,
					"Unexpected IPv6 address size");

			std::copy (bytes.begin (), bytes.end (), &in6addr.__in6_u.__u6_addr8 [0]);

			return Code2Str (GeoIP_country_code_by_ipnum_v6 (Impl_.get (), in6addr));
		}

		qWarning () << Q_FUNC_INFO
				<< "the address is neither IPv4 nor IPv6"
				<< addr.to_string ().c_str ();

		return {};
	}
#else
	GeoIP::GeoIP ()
	{
	}

	boost::optional<QString> GeoIP::GetCountry (const libtorrent::address&) const
	{
		return {};
	}
#endif
}
}
