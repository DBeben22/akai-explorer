// SPDX-License-Identifier: GPL-2.0-or-later
#include "IconItem.h"

#include <QPainter>
#include <QPainterPath>

IconItem::IconItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setImplicitWidth(20);
    setImplicitHeight(20);
    connect(this, &IconItem::changed, this, [this] { update(); });
}

// All icons are designed on a 24 x 24 grid.
void IconItem::paint(QPainter *p)
{
    const qreal s = qMin(width(), height()) / 24.0;
    p->translate((width() - 24 * s) / 2, (height() - 24 * s) / 2);
    p->scale(s, s);
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(m_color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPainterPath stroke, fill;
    const QString &n = m_name;

    if (n == QLatin1String("play")) {
        fill.moveTo(8, 5); fill.lineTo(19.5, 12); fill.lineTo(8, 19); fill.closeSubpath();
    } else if (n == QLatin1String("pause")) {
        fill.addRoundedRect(QRectF(6.5, 5, 4, 14), 1, 1);
        fill.addRoundedRect(QRectF(13.5, 5, 4, 14), 1, 1);
    } else if (n == QLatin1String("chevron-right")) {
        stroke.moveTo(9.5, 6.5); stroke.lineTo(15, 12); stroke.lineTo(9.5, 17.5);
    } else if (n == QLatin1String("chevron-down")) {
        stroke.moveTo(6.5, 9.5); stroke.lineTo(12, 15); stroke.lineTo(17.5, 9.5);
    } else if (n == QLatin1String("close")) {
        stroke.moveTo(6.5, 6.5); stroke.lineTo(17.5, 17.5); stroke.moveTo(17.5, 6.5); stroke.lineTo(6.5, 17.5);
    } else if (n == QLatin1String("plus")) {
        stroke.moveTo(12, 5); stroke.lineTo(12, 19); stroke.moveTo(5, 12); stroke.lineTo(19, 12);
    } else if (n == QLatin1String("check")) {
        stroke.moveTo(5, 12.5); stroke.lineTo(10, 17.5); stroke.lineTo(19, 7);
    } else if (n == QLatin1String("search")) {
        stroke.addEllipse(QPointF(10.5, 10.5), 6, 6);
        stroke.moveTo(15, 15); stroke.lineTo(20, 20);
    } else if (n == QLatin1String("export")) {
        stroke.moveTo(12, 4); stroke.lineTo(12, 15);
        stroke.moveTo(7, 10.5); stroke.lineTo(12, 15.5); stroke.lineTo(17, 10.5);
        stroke.moveTo(5, 20); stroke.lineTo(19, 20);
    } else if (n == QLatin1String("folder")) {
        stroke.moveTo(3.5, 7); stroke.lineTo(3.5, 18); stroke.lineTo(20.5, 18); stroke.lineTo(20.5, 8.5);
        stroke.lineTo(12, 8.5); stroke.lineTo(10, 6); stroke.lineTo(4.5, 6); stroke.lineTo(3.5, 7);
    } else if (n == QLatin1String("floppy")) {
        stroke.moveTo(4, 4); stroke.lineTo(16.5, 4); stroke.lineTo(20, 7.5); stroke.lineTo(20, 20);
        stroke.lineTo(4, 20); stroke.closeSubpath();
        stroke.addRect(QRectF(8, 4, 7, 5));
        stroke.addRect(QRectF(7.5, 13, 9, 7));
    } else if (n == QLatin1String("disk")) {
        stroke.addRoundedRect(QRectF(3.5, 5, 17, 14), 2.5, 2.5);
        stroke.addEllipse(QPointF(11, 12), 3.6, 3.6);
        stroke.moveTo(17, 15.5); stroke.lineTo(17.01, 15.5);
    } else if (n == QLatin1String("partition")) {
        stroke.addRoundedRect(QRectF(4, 5, 16, 6), 1.5, 1.5);
        stroke.addRoundedRect(QRectF(4, 13, 16, 6), 1.5, 1.5);
    } else if (n == QLatin1String("volume")) { // a volume (folder of sounds): stacked lines
        stroke.moveTo(5, 7); stroke.lineTo(19, 7);
        stroke.moveTo(5, 12); stroke.lineTo(19, 12);
        stroke.moveTo(5, 17); stroke.lineTo(14, 17);
    } else if (n == QLatin1String("wave")) {
        stroke.moveTo(4, 10.5); stroke.lineTo(4, 13.5);
        stroke.moveTo(8, 7.5); stroke.lineTo(8, 16.5);
        stroke.moveTo(12, 4.5); stroke.lineTo(12, 19.5);
        stroke.moveTo(16, 8.5); stroke.lineTo(16, 15.5);
        stroke.moveTo(20, 10.5); stroke.lineTo(20, 13.5);
    } else if (n == QLatin1String("file")) {
        stroke.moveTo(6.5, 3.5); stroke.lineTo(14, 3.5); stroke.lineTo(18, 7.5); stroke.lineTo(18, 20.5);
        stroke.lineTo(6.5, 20.5); stroke.closeSubpath();
        stroke.moveTo(13.5, 3.5); stroke.lineTo(13.5, 8); stroke.lineTo(18, 8);
    } else if (n == QLatin1String("loop")) {
        stroke.moveTo(16.5, 3.5); stroke.lineTo(19.5, 6.5); stroke.lineTo(16.5, 9.5);
        stroke.moveTo(19.5, 6.5); stroke.lineTo(8, 6.5); stroke.quadTo(4.5, 6.5, 4.5, 10); stroke.lineTo(4.5, 11.5);
        stroke.moveTo(7.5, 14.5); stroke.lineTo(4.5, 17.5); stroke.lineTo(7.5, 20.5);
        stroke.moveTo(4.5, 17.5); stroke.lineTo(16, 17.5); stroke.quadTo(19.5, 17.5, 19.5, 14); stroke.lineTo(19.5, 12.5);
    } else if (n == QLatin1String("speaker")) {
        stroke.moveTo(4, 9.5); stroke.lineTo(4, 14.5); stroke.lineTo(7.5, 14.5); stroke.lineTo(12, 18.5);
        stroke.lineTo(12, 5.5); stroke.lineTo(7.5, 9.5); stroke.closeSubpath();
        stroke.moveTo(15.5, 9); stroke.quadTo(18, 12, 15.5, 15);
        stroke.moveTo(18, 6.5); stroke.quadTo(22.5, 12, 18, 17.5);
    }

    if (!fill.isEmpty()) {
        p->setPen(Qt::NoPen);
        p->setBrush(m_color);
        p->drawPath(fill);
    }
    if (!stroke.isEmpty()) {
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(stroke);
    }
}
