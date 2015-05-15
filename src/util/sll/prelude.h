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

#include <type_traits>
#include <iterator>
#include <QPair>
#include <QStringList>
#include <boost/optional.hpp>
#include "oldcppkludges.h"

namespace LeechCraft
{
namespace Util
{
	template<typename T1, typename T2, template<typename U> class Container, typename F>
	auto ZipWith (const Container<T1>& c1, const Container<T2>& c2, F f) -> Container<typename std::result_of<F (T1, T2)>::type>
	{
		Container<typename std::result_of<F (T1, T2)>::type> result;

		using std::begin;
		using std::end;

		auto i1 = begin (c1), e1 = end (c1);
		auto i2 = begin (c2), e2 = end (c2);
		for ( ; i1 != e1 && i2 != e2; ++i1, ++i2)
			result.push_back (f (*i1, *i2));
		return result;
	}

	template<typename T1, typename T2,
		template<typename U> class Container,
		template<typename U1, typename U2> class Pair = QPair>
	auto Zip (const Container<T1>& c1, const Container<T2>& c2) -> Container<Pair<T1, T2>>
	{
		return ZipWith (c1, c2,
				[] (const T1& t1, const T2& t2) -> Pair<T1, T2>
					{ return { t1, t2}; });
	}

	template<typename T>
	struct WrapType
	{
		using type = T;
	};

	template<typename T>
	using WrapType_t = typename WrapType<T>::type;

	template<>
	struct WrapType<QList<QString>>
	{
		using type = QStringList;
	};

	namespace detail
	{
		template<typename Res, typename T>
		void Append (Res& result, T&& val, decltype (result.push_back (std::forward<T> (val)))* = nullptr)
		{
			result.push_back (std::forward<T> (val));
		}

		template<typename Res, typename T>
		void Append (Res& result, T&& val, decltype (result.insert (std::forward<T> (val)))* = nullptr)
		{
			result.insert (std::forward<T> (val));
		}

		template<typename T, typename F>
		constexpr bool IsInvokableWithConstImpl (typename std::result_of<F (const T&)>::type*)
		{
			return true;
		}

		template<typename T, typename F>
		constexpr bool IsInvokableWithConstImpl (...)
		{
			return false;
		}

		template<typename T, typename F>
		constexpr bool IsInvokableWithConst ()
		{
			return IsInvokableWithConstImpl<typename std::decay<T>::type, F> (0);
		}
	}

	template<typename T, template<typename U> class Container, typename F>
	auto Map (const Container<T>& c, F f) -> typename std::enable_if<!std::is_same<void, decltype (Invoke (f, std::declval<T> ()))>::value,
			WrapType_t<Container<typename std::decay<decltype (Invoke (f, std::declval<T> ()))>::type>>>::type
	{
		Container<typename std::decay<decltype (Invoke (f, std::declval<T> ()))>::type> result;
		for (auto&& t : c)
			detail::Append (result, Invoke (f, t));
		return result;
	}

	template<template<typename...> class Container, typename F, template<typename> class ResultCont = QList, typename... ContArgs>
	auto Map (const Container<ContArgs...>& c, F f) -> typename std::enable_if<!std::is_same<void, decltype (Invoke (f, *c.begin ()))>::value,
			WrapType_t<ResultCont<typename std::decay<decltype (Invoke (f, *c.begin ()))>::type>>>::type
	{
		ResultCont<typename std::decay<decltype (Invoke (f, *c.begin ()))>::type> cont;
		for (auto&& t : c)
			detail::Append (cont, Invoke (f, t));
		return cont;
	}

	template<template<typename...> class Container, typename F, typename... ContArgs>
	auto Map (Container<ContArgs...>& c, F f) -> typename std::enable_if<std::is_same<void, decltype (Invoke (f, *c.begin ()))>::value>::type
	{
		for (auto&& t : c)
			Invoke (f, t);
	}

	template<template<typename...> class Container, typename F, typename... ContArgs>
	auto Map (const Container<ContArgs...>& c, F f) -> typename std::enable_if<std::is_same<void, decltype (Invoke (f, *c.begin ()))>::value>::type
	{
		auto copy = c;
		Map (copy, f);
	}

#ifndef USE_CPP14
	template<typename F>
	QList<typename std::decay<typename std::result_of<F (QString)>::type>::type> Map (const QStringList& c, F f)
	{
		QList<typename std::decay<typename std::result_of<F (QString)>::type>::type> result;
		for (auto&& t : c)
			result.push_back (Invoke (f, t));
		return result;
	}
#endif

	template<typename T, template<typename U> class Container, typename F>
	auto Map (const Container<T>& c, F f) -> typename std::enable_if<std::is_same<void, decltype (Invoke (f, std::declval<T> ()))>::value, void>::type
	{
		for (auto&& t : c)
			Invoke (f, t);
	}

	template<typename T, template<typename U> class Container, typename F>
	auto Filter (const Container<T>& c, F f) -> Container<T>
	{
		Container<T> result;
		std::copy_if (c.begin (), c.end (), std::back_inserter (result), f);
		return result;
	}

	template<template<typename> class Container, typename T>
	Container<T> Concat (const Container<Container<T>>& containers)
	{
		Container<T> result;
		for (const auto& cont : containers)
			std::copy (cont.begin (), cont.end (), std::back_inserter (result));
		return result;
	}

	template<template<typename...> class Container, typename... ContArgs>
	auto Concat (const Container<ContArgs...>& containers) -> typename std::decay<decltype (*containers.begin ())>::type
	{
		typename std::decay<decltype (*containers.begin ())>::type result;
		for (const auto& cont : containers)
			std::copy (cont.begin (), cont.end (), std::back_inserter (result));
		return result;
	}

	template<template<typename> class Container, typename T>
	Container<Container<T>> SplitInto (size_t numChunks, const Container<T>& container)
	{
		Container<Container<T>> result;

		const size_t chunkSize = container.size () / numChunks;
		for (size_t i = 0; i < numChunks; ++i)
		{
			Container<T> subcont;
			const auto start = container.begin () + chunkSize * i;
			const auto end = start + chunkSize;
			std::copy (start, end, std::back_inserter (subcont));
			result.push_back (subcont);
		}

		const auto lastStart = container.begin () + chunkSize * numChunks;
		const auto lastEnd = container.end ();
		std::copy (lastStart, lastEnd, std::back_inserter (result.front ()));

		return result;
	}

	template<template<typename Pair, typename... Rest> class Cont, template<typename K, typename V> class Pair, typename K, typename V, typename KV, typename... Rest>
	boost::optional<V> Lookup (const KV& key, const Cont<Pair<K, V>, Rest...>& cont)
	{
		for (const auto& pair : cont)
			if (pair.first == key)
				return pair.second;

		return {};
	}

#ifdef USE_CPP14
	template<typename R>
	auto ComparingBy (R r)
	{
		return [r] (const auto& left, const auto& right) { return r (left) < r (right); };
	}

	auto Apply = [] (const auto& t) { return t (); };
#else
	namespace detail
	{
		template<typename R>
		struct ComparingByClosure
		{
			const R R_;

			template<typename T>
			bool operator() (const T& left, const T& right) const
			{
				return R_ (left) < R_ (right);
			}
		};
	}

	template<typename R>
	detail::ComparingByClosure<R> ComparingBy (R r)
	{
		return detail::ComparingByClosure<R> { r };
	}

	struct
	{
		template<typename T>
		typename std::result_of<T ()>::type operator() (const T& t) const
		{
			return t ();
		}
	} Apply;
#endif
}
}
