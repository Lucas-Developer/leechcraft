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

#include <QtGlobal>

class QString;

namespace LeechCraft
{
namespace Azoth
{
	/** @brief Interface for accounts supporting user mood.
	 * 
	 * This interface can be implemented by account objects to advertise
	 * the support for publishing current user mood.
	 * 
	 * The mood concept in Azoth is based on the XMPP XEP-0107: User
	 * Mood (http://xmpp.org/extensions/xep-0107.html).
	 * 
	 * @sa IAccount
	 */
	class ISupportMood
	{
	public:
		virtual ~ISupportMood () {}

		/** @brief Publishes the current user mood.
		 * 
		 * The mood information is divided into two pieces:
		 * mood name (required) and an optional text.
		 * 
		 * The possible values of the mood name are
		 * listed in http://xmpp.org/extensions/xep-0107.html.
		 * 
		 * @param[in] mood The mood name.
		 * @param[in] text The additional text message (optional).
		 */
		virtual void SetMood (const QString& mood, const QString& text) = 0;
	};
}
}

Q_DECLARE_INTERFACE (LeechCraft::Azoth::ISupportMood,
		"org.Deviant.LeechCraft.Azoth.ISupportMood/1.0")
