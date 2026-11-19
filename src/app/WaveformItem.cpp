// SPDX-License-Identifier: GPL-2.0-or-later
#include "WaveformItem.h"

#include "Player.h"

#include <QPainter>

#include <cmath>

WaveformItem::WaveformItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &WaveformItem::styleChanged, this, [this] { update(); });
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { update(); });
}

Player *WaveformItem::player() const
{
    return m_player;
}

void WaveformItem::setPlayer(Player *player)
{
    if (player == m_player)
        return;
    if (m_player)
        m_player->disconnect(this);
    m_player = player;
    if (m_player)
        connect(m_player, &Player::trackChanged, this, [this] { update(); });
    emit playerChanged();
    update();
}

void WaveformItem::setProgress(qreal p)
{
    if (qFuzzyCompare(p + 1.0, m_progress + 1.0))
        return;
    m_progress = p;
    emit progressChanged();
    update();
}

void WaveformItem::paint(QPainter *painter)
{
    const qreal w = width(), h = height();
    if (w < 4 || h < 4)
        return;
    const qreal barW = 2.0, gap = 1.0, pitch = barW + gap;
    const int bars = qMax(1, int(w / pitch));
    const qreal mid = h / 2.0;
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(Qt::NoPen);

    const QVector<float> empty;
    const QVector<float> &peaks = m_player ? m_player->peaks() : empty;
    const int buckets = peaks.size() / 2;

    if (m_showLoop && m_player && m_player->hasLoop()) {
        const qreal x0 = m_player->loopStart() * w, x1 = m_player->loopEnd() * w;
        painter->fillRect(QRectF(x0, 0, qMax<qreal>(1.0, x1 - x0), h), m_loopColor);
    }

    const qreal playedX = m_progress * w;
    for (int i = 0; i < bars; ++i) {
        qreal lo = 0.0, hi = 0.0;
        if (buckets > 0) {
            const int b0 = int(qint64(i) * buckets / bars);
            const int b1 = qMax(b0 + 1, int(qint64(i + 1) * buckets / bars));
            for (int b = b0; b < b1 && b < buckets; ++b) {
                lo = qMin<qreal>(lo, peaks[2 * b]);
                hi = qMax<qreal>(hi, peaks[2 * b + 1]);
            }
        }
        const qreal x = i * pitch;
        // at least a 2 px dash so that silence still reads as a time line
        const qreal top = qMin(mid - 1.0, mid - hi * (mid - 1.0));
        const qreal bottom = qMax(mid + 1.0, mid - lo * (mid - 1.0));
        painter->fillRect(QRectF(x, std::floor(top), barW, std::ceil(bottom - top)),
                          x + barW <= playedX ? m_playedColor : m_color);
    }
}
