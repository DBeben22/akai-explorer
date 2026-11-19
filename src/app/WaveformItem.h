// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>

class Player;

/** Bar-style waveform of the loaded track with played/unplayed colouring. */
class WaveformItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(Player *source READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY styleChanged)
    Q_PROPERTY(QColor playedColor MEMBER m_playedColor NOTIFY styleChanged)
    Q_PROPERTY(QColor loopColor MEMBER m_loopColor NOTIFY styleChanged)
    Q_PROPERTY(bool showLoop MEMBER m_showLoop NOTIFY styleChanged)
public:
    explicit WaveformItem(QQuickItem *parent = nullptr);

    Player *player() const;
    void setPlayer(Player *player);
    qreal progress() const { return m_progress; }
    void setProgress(qreal p);

    void paint(QPainter *painter) override;

signals:
    void playerChanged();
    void progressChanged();
    void styleChanged();

private:
    QPointer<Player> m_player;
    qreal m_progress = 0.0;
    QColor m_color{0x4a, 0x52, 0x66};
    QColor m_playedColor{0xff, 0xff, 0xff};
    QColor m_loopColor{0xff, 0xff, 0xff, 0x20};
    bool m_showLoop = true;
};
