// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QColor>
#include <QQuickPaintedItem>

/** Small vector icon set drawn with QPainter (no icon font / SVG dependency). */
class IconItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name NOTIFY changed)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
public:
    explicit IconItem(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

signals:
    void changed();

private:
    QString m_name;
    QColor m_color{Qt::white};
};
