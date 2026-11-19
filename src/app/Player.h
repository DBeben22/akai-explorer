// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "AkaiSession.h"

#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <memory>

/** A decoded sample ready for preview. */
struct Track {
    QString title;
    QString detail;
    akx::Audio audio;
    akx::SampleMeta meta;
    QVector<float> peaks; // (min, max) pairs in -1..1, mono mix
    bool stereoPair = false;
};

/** Pull-mode source for QAudioSink with optional looping. */
class PcmDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit PcmDevice(QObject *parent = nullptr) : QIODevice(parent) {}

    void setData(const QByteArray &data, int bytesPerFrame);
    void setLoopRegion(qint64 startFrame, qint64 endFrame); // endFrame <= startFrame: whole sample
    void setLooping(bool on) { m_looping = on; }
    void seekFrame(qint64 frame);
    qint64 frame() const { return m_bytesPerFrame ? m_pos.load() / m_bytesPerFrame : 0; }
    qint64 frames() const { return m_bytesPerFrame ? m_data.size() / m_bytesPerFrame : 0; }
    bool finished() const { return m_finished; }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_data.size() + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QByteArray m_data;
    int m_bytesPerFrame = 2;
    std::atomic<qint64> m_pos{0};
    std::atomic<qint64> m_loopStart{0}, m_loopEnd{0};
    std::atomic<bool> m_looping{false};
    std::atomic<bool> m_finished{false};
};

class Player : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY trackChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY trackChanged)
    Q_PROPERTY(QStringList facts READ facts NOTIFY trackChanged)
    Q_PROPERTY(qreal progress READ progress WRITE seek NOTIFY progressChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY progressChanged)
    Q_PROPERTY(QString durationText READ durationText NOTIFY trackChanged)
    Q_PROPERTY(bool hasLoop READ hasLoop NOTIFY trackChanged)
    Q_PROPERTY(qreal loopStart READ loopStart NOTIFY trackChanged)
    Q_PROPERTY(qreal loopEnd READ loopEnd NOTIFY trackChanged)
    Q_PROPERTY(bool looping READ looping WRITE setLooping NOTIFY loopingChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool audioAvailable READ audioAvailable CONSTANT)
public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    bool loaded() const { return m_loaded; }
    bool loading() const { return m_loading; }
    void setLoading(bool on);
    bool playing() const { return m_playing; }
    QString title() const { return m_track.title; }
    QString detail() const { return m_track.detail; }
    QStringList facts() const { return m_facts; }
    qreal progress() const;
    QString positionText() const;
    QString durationText() const;
    bool hasLoop() const { return m_loaded && m_track.meta.hasLoop; }
    qreal loopStart() const;
    qreal loopEnd() const;
    bool looping() const { return m_looping; }
    void setLooping(bool on);
    qreal volume() const { return m_volume; }
    void setVolume(qreal v);
    bool audioAvailable() const { return m_audioAvailable; }

    const QVector<float> &peaks() const { return m_track.peaks; }

    void setTrack(const Track &track, bool autoplay);
    void clear();

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void seek(qreal fraction);

signals:
    void trackChanged();
    void loadingChanged();
    void playingChanged();
    void progressChanged();
    void loopingChanged();
    void volumeChanged();
    void problem(const QString &message);

private:
    void teardownSink();
    bool setupSink();
    void applyVolume();
    void poll();

    Track m_track;
    QStringList m_facts;
    bool m_loaded = false;
    bool m_loading = false;
    bool m_playing = false;
    bool m_looping = false;
    bool m_audioAvailable = false;
    qreal m_volume = 0.8;
    qreal m_idleProgress = 0.0; // position while no sink is active
    // The sink pulls audio in chunks (often ~100 ms), so the read position moves in
    // steps. The displayed playhead runs on a clock and is steered towards the
    // position that is actually audible (read position minus what is still queued).
    double m_shownFrame = 0.0;
    int m_outRate = 0;
    QElapsedTimer m_clock;
    std::unique_ptr<QAudioSink> m_sink;
    PcmDevice m_device;
    QTimer m_poll;
};
