/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "plotitem.h"
#include <QStyleOption>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_renderer.h>

Q_DECLARE_METATYPE (QList<QPointF>)

namespace LeechCraft
{
namespace Util
{
	PlotItem::PlotItem (QDeclarativeItem *parent)
	: QDeclarativeItem (parent)
	{
		setFlag (QGraphicsItem::ItemHasNoContents, false);
	}

	QList<QPointF> PlotItem::GetPoints () const
	{
		return Points_;
	}

	void PlotItem::SetPoints (const QList<QPointF>& pts)
	{
		if (pts == Points_)
			return;

		Points_ = pts;
		emit pointsChanged ();
		update ();
	}

	void PlotItem::paint (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget*)
	{
		QwtPlot plot;
		plot.setAxisAutoScale (QwtPlot::xBottom, false);
		plot.setAxisAutoScale (QwtPlot::yLeft, false);
		plot.enableAxis (QwtPlot::yLeft, false);
		plot.enableAxis (QwtPlot::xBottom, false);
		plot.resize (option->rect.size ());
		plot.setAxisScale (QwtPlot::xBottom, 0, Points_.size ());
		plot.setAxisScale (QwtPlot::yLeft, 0, 100);
		plot.setAutoFillBackground (false);
		plot.setCanvasBackground (Qt::transparent);

		QwtPlotCurve curve;

		QColor percentColor ("#FF4B10");
		curve.setPen (QPen (percentColor));
		percentColor.setAlpha (20);
		curve.setBrush (percentColor);

		curve.setRenderHint (QwtPlotItem::RenderAntialiased);
		curve.attach (&plot);

		curve.setSamples (Points_.toVector ());
		plot.replot ();

		QwtPlotRenderer {}.render (&plot, painter, option->rect);
	}
}
}
