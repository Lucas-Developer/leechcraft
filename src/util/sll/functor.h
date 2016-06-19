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

#include <boost/optional.hpp>
#include "oldcppkludges.h"
#include "typeclassutil.h"

namespace LeechCraft
{
namespace Util
{
	/** @brief The Functor class is used for types that can be mapped over.
	 *
	 * Minimal complete definition:
	 * - Apply() function and FmapResult_t alias.
	 *
	 * For a reference imolementation please see InstanceFunctor<boost::optional<T>>.
	 *
	 * @tparam T The functor type instantiated with some concrete
	 * containee type.
	 */
	template<typename T>
	struct InstanceFunctor
	{
		using UndefinedTag = void;

		/** @brief The type of the functor after its elements were
		 * mapped by the function \em F.
		 *
		 * This type should correspond to the return type of the Apply()
		 * function when passed this functor and a function of type
		 * \em F.
		 *
		 * @tparam F The type of the function to apply to the elements
		 * inside this functor.
		 */
		template<typename F>
		using FmapResult_t = detail::ImplementationType;

		/** @brief Applies the \em function to the each of the elements
		 * inside the \em functor.
		 *
		 * @param[in] functor The functor whose values are subject to
		 * \em function.
		 * @param[in] function The function that should be applied to the
		 * values in the \em functor.
		 * @return A functor of type FmapResult_t<F> where each element
		 * the result of applying the \em function to the corresponding element
		 * in the source \em functor.
		 *
		 * @tparam F The type of the \em function to apply to the elements
		 * in the function.
		 */
		template<typename F>
		static FmapResult_t<F> Apply (const T& functor, const F& function);
	};

	namespace detail
	{
		template<typename T>
		constexpr bool IsFunctorImpl (int, typename InstanceFunctor<T>::UndefinedTag* = nullptr)
		{
			return false;
		}

		template<typename T>
		constexpr bool IsFunctorImpl (float)
		{
			return true;
		}
	}

	/** @brief Checks whether the given type has a Functor instance for it.
	 *
	 * @return Whether type T implements the Functor class.
	 *
	 * @tparam T The type to check.
	 */
	template<typename T>
	constexpr bool IsFunctor ()
	{
		return detail::IsFunctorImpl<T> (0);
	}

	/** @brief The result type of the contents of the functor \em T mapped
	 * by function \em F.
	 *
	 * @tparam T The type of the functor.
	 * @tparam F The type of the function to apply to the elements inside the
	 * functor.
	 */
	template<typename T, typename F>
	using FmapResult_t = typename InstanceFunctor<T>::template FmapResult_t<F>;

	template<typename T, typename F, typename = EnableIf_t<IsFunctor<T> ()>>
	FmapResult_t<T, F> Fmap (const T& functor, const F& f)
	{
		return InstanceFunctor<T>::Apply (functor, f);
	}

	template<typename MF, typename F>
	auto operator* (const F& f, const MF& value) -> decltype (Fmap (value, f))
	{
		return Fmap (value, f);
	}

	template<typename MF, typename F>
	auto operator* (const MF& value, const F& f) -> decltype (Fmap (value, f))
	{
		return Fmap (value, f);
	}

	// Implementations
	template<typename T>
	struct InstanceFunctor<boost::optional<T>>
	{
		template<typename F>
		using FmapResult_t = boost::optional<Decay_t<ResultOf_t<F (T)>>>;

		template<typename F>
		static FmapResult_t<F> Apply (const boost::optional<T>& t, const F& f)
		{
			if (!t)
				return {};

			return { Invoke (f, *t) };
		}
	};
}
}
