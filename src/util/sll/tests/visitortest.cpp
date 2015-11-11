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

#include "visitortest.h"
#include <QtTest>
#include <visitor.h>

QTEST_MAIN (LeechCraft::Util::VisitorTest)

namespace LeechCraft
{
namespace Util
{
	using Visitor_t = boost::variant<int, char, std::string, QString, double, float>;

	void VisitorTest::testBasicVisitor ()
	{
		Visitor_t v { 'a' };
		const auto& res = Visit (v,
					[] (char) { return true; },
					[] (int) { return false; },
					[] (std::string) { return false; },
					[] (QString) { return false; },
					[] (double) { return false; },
					[] (float) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testBasicVisitorGenericFallback ()
	{
		Visitor_t v { 'a' };
		const auto& res = Visit (v,
					[] (char) { return true; },
					[] (int) { return false; },
					[] (auto) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testBasicVisitorCoercion ()
	{
		Visitor_t v { 'a' };
		const auto& res = Visit (v,
					[] (int) { return true; },
					[] (std::string) { return false; },
					[] (QString) { return false; },
					[] (double) { return false; },
					[] (float) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testBasicVisitorCoercionGenericFallback ()
	{
		Visitor_t v { 'a' };
		const auto& res = Visit (v,
					[] (int) { return false; },
					[] (QString) { return false; },
					[] (auto) { return true; });
		QCOMPARE (res, true);
	}

#define NC nc = std::unique_ptr<int> {}

	void VisitorTest::testNonCopyableFunctors ()
	{
		Visitor_t v { 'a' };
		const auto& res = Visit (v,
					[NC] (char) { return true; },
					[NC] (int) { return false; },
					[NC] (std::string) { return false; },
					[NC] (QString) { return false; },
					[NC] (double) { return false; },
					[NC] (float) { return false; });
		QCOMPARE (res, true);
	}
}
}
