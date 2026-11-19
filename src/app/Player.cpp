// SPDX-License-Identifier: GPL-2.0-or-later
#include "Player.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QLocale>
#include <QMediaDevices>
#include <QSettings>

#include <cmath>
#include <cstring>

/* ---- PcmDevice ------------------------------------------------------------ */

void PcmDevice::setData(const QByteArray &data, int bytesPerFrame)
{
    m_data = data;
    m_bytesPerFrame = qMax(1, bytesPerFrame);
    m_pos = 0;
    m_loopStart = 0;
    m_loopEnd = 0;
    m_finished = false;
}

void PcmDevice::setLoopRegion(qint64 startFrame, qint64 endFrame)
{
    const qint64 n = frames();
    startFrame = qBound<qint64>(0, startFrame, n);
    endFrame = qBound<qint64>(0, endFrame, n);
    if (endFrame <= startFrame) {
        startFrame = 0;
        endFrame = 0;
    }
    m_loopStart = startFrame * m_bytesPerFrame;
    m_loopEnd = endFrame * m_bytesPerFrame;
}

void PcmDevice::seekFrame(qint64 frame)
{
    m_pos = qBound<qint64>(0, frame, frames()) * m_bytesPerFrame;
    m_finished = false;
}

qint64 PcmDevice::readData(char *data, qint64 maxlen)
{
    maxlen -= maxlen % m_bytesPerFrame;
    const qint64 size = m_data.size();
    qint64 done = 0;
    qint64 pos = m_pos.load();
    while (done < maxlen && size > 0) {
        const bool loop = m_looping.load();
        const qint64 ls = m_loopStart.load(), le = m_loopEnd.load();
        const bool region = le > ls;
        // while looping, play up to the loop end (unless the user seeked behind it)
        const qint64 end = (loop && region && pos < le) ? le : size;
        if (pos >= end) {
            if (!loop)
                break;
            pos = region ? ls : 0;
            continue;
        }
        const qint64 n = qMin(maxlen - done, end - pos);
        memcpy(data + done, m_data.constData() + pos, size_t(n));
        done += n;
        pos += n;
    }
    m_pos = pos;
    if (done == 0 && maxlen > 0)
        m_finished = true;
    return done;
}

/* ---- Player --------------------------------------------------------------- */

static QString timeText(double secs)
{
    const int whole = int(secs);
    const int ms = int((secs - whole) * 1000.0 + 0.5) % 1000;
    return QStringLiteral("%1:%2.%3").arg(whole / 60).arg(whole % 60, 2, 10, QLatin1Char('0')).arg(ms, 3, 10, QLatin1Char('0'));
}

static QString noteName(int midi)
{
    // AKAI convention: MIDI note 60 is shown as C3
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return QStringLiteral("%1%2").arg(QLatin1String(names[midi % 12])).arg(midi / 12 - 2);
}

Player::Player(QObject *parent) : QObject(parent)
{
    m_audioAvailable = !QMediaDevices::defaultAudioOutput().isNull();
    m_volume = QSettings().value(QStringLiteral("player/volume"), 0.8).toReal();
    m_looping = QSettings().value(QStringLiteral("player/looping"), false).toBool();
    m_device.open(QIODevice::ReadOnly);
    m_poll.setInterval(30);
    connect(&m_poll, &QTimer::timeout, this, &Player::poll);
}

Player::~Player()
{
    teardownSink();
}

void Player::setLoading(bool on)
{
    if (on == m_loading)
        return;
    m_loading = on;
    emit loadingChanged();
}

qreal Player::progress() const
{
    if (!m_loaded)
        return 0.0;
    const qint64 n = m_device.frames();
    if (n <= 0)
        return m_idleProgress;
    return qBound(0.0, (m_playing ? m_shownFrame : qreal(m_device.frame())) / qreal(n), 1.0);
}

QString Player::positionText() const
{
    return m_loaded ? timeText(progress() * m_track.meta.seconds()) : QString();
}

QString Player::durationText() const
{
    return m_loaded ? timeText(m_track.meta.seconds()) : QString();
}

qreal Player::loopStart() const
{
    return hasLoop() && m_track.meta.frames ? qreal(m_track.meta.loopStart) / m_track.meta.frames : 0.0;
}

qreal Player::loopEnd() const
{
    return hasLoop() && m_track.meta.frames ? qreal(m_track.meta.loopEnd) / m_track.meta.frames : 0.0;
}

void Player::setLooping(bool on)
{
    if (on == m_looping)
        return;
    m_looping = on;
    m_device.setLooping(on);
    QSettings().setValue(QStringLiteral("player/looping"), on);
    emit loopingChanged();
}

void Player::setVolume(qreal v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v, m_volume))
        return;
    m_volume = v;
    applyVolume();
    QSettings().setValue(QStringLiteral("player/volume"), v);
    emit volumeChanged();
}

void Player::applyVolume()
{
    if (m_sink)
        m_sink->setVolume(QAudio::convertVolume(m_volume, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));
}

void Player::teardownSink()
{
    m_poll.stop();
    if (m_sink) {
        m_sink->disconnect(this);
        m_sink->stop();
        m_sink.reset();
    }
}

// Convert the track to a format the output device accepts and create the sink.
bool Player::setupSink()
{
    const akx::Audio &a = m_track.audio;
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull() || a.pcm.empty() || a.sampleRate == 0)
        return false;

    QAudioFormat fmt;
    fmt.setSampleFormat(QAudioFormat::Int16);
    fmt.setChannelCount(2);
    fmt.setSampleRate(int(a.sampleRate));
    const size_t frames = a.frames();
    size_t outFrames = frames;
    double step = 1.0;
    if (!dev.isFormatSupported(fmt)) {
        // unusual vintage sample rates (e.g. 15 kHz): resample to the device rate
        int rate = dev.preferredFormat().sampleRate();
        if (rate <= 0)
            rate = 48000;
        fmt.setSampleRate(rate);
        if (!dev.isFormatSupported(fmt))
            return false;
        step = double(a.sampleRate) / double(rate);
        outFrames = size_t(double(frames) / step);
    }
    QByteArray bytes(qsizetype(outFrames * 4), Qt::Uninitialized);
    qint16 *out = reinterpret_cast<qint16 *>(bytes.data());
    const int ch = a.channels;
    for (size_t i = 0; i < outFrames; ++i) {
        const double srcPos = double(i) * step;
        const size_t i0 = qMin(size_t(srcPos), frames - 1);
        const size_t i1 = qMin(i0 + 1, frames - 1);
        const double t = srcPos - double(i0);
        for (int c = 0; c < 2; ++c) {
            const int sc = ch == 2 ? c : 0;
            const double v = (1.0 - t) * a.pcm[i0 * size_t(ch) + size_t(sc)] + t * a.pcm[i1 * size_t(ch) + size_t(sc)];
            out[i * 2 + size_t(c)] = qint16(std::lround(v));
        }
    }
    m_device.setData(bytes, 4);
    if (m_track.meta.hasLoop)
        m_device.setLoopRegion(qint64(double(m_track.meta.loopStart) / step), qint64(double(m_track.meta.loopEnd) / step));
    m_device.setLooping(m_looping);

    m_outRate = fmt.sampleRate();
    m_sink = std::make_unique<QAudioSink>(dev, fmt);
    // Qt's default buffer size is kept: the playhead compensates for what is queued (see poll())
    applyVolume();
    return true;
}

void Player::setTrack(const Track &track, bool autoplay)
{
    teardownSink();
    m_track = track;
    m_loaded = true;
    m_idleProgress = 0.0;
    m_device.setData(QByteArray(), 4);

    const akx::SampleMeta &m = m_track.meta;
    m_facts.clear();
    m_facts << QStringLiteral("%1 kHz").arg(QLocale().toString(m.sampleRate / 1000.0, 'f', 1));
    m_facts << (m.compressed ? tr("%1-bit, compressed").arg(m.bits) : tr("%1-bit").arg(m.bits));
    m_facts << (m_track.audio.channels == 2 ? (m_track.stereoPair ? tr("Stereo pair") : tr("Stereo")) : tr("Mono"));
    if (m.rootKey >= 0 && m.rootKey < 128)
        m_facts << tr("Root key %1").arg(noteName(m.rootKey));
    if (!m.playMode.empty())
        m_facts << QString::fromStdString(m.playMode);

    if (m_audioAvailable && !setupSink() && !m_track.audio.pcm.empty())
        emit problem(tr("The audio device does not accept this sample format."));
    if (m_playing) {
        m_playing = false;
        emit playingChanged();
    }
    emit trackChanged();
    emit progressChanged();
    if (autoplay)
        play();
}

void Player::clear()
{
    teardownSink();
    m_track = Track();
    m_facts.clear();
    m_loaded = false;
    m_device.setData(QByteArray(), 4);
    if (m_playing) {
        m_playing = false;
        emit playingChanged();
    }
    emit trackChanged();
    emit progressChanged();
}

void Player::play()
{
    if (!m_loaded || m_playing)
        return;
    if (!m_sink) {
        if (!m_audioAvailable)
            emit problem(tr("No audio output device found. Preview is silent, export still works."));
        return;
    }
    if (m_device.finished() || m_device.frame() >= m_device.frames())
        m_device.seekFrame(0);
    if (m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();
    else
        m_sink->start(&m_device);
    m_playing = true;
    m_shownFrame = double(m_device.frame());
    m_clock.restart();
    m_poll.start();
    emit playingChanged();
}

void Player::pause()
{
    if (!m_playing)
        return;
    // stop (not suspend): frees the device, and the buffered 60 ms are negligible
    const qint64 at = qint64(m_shownFrame);
    m_sink->stop();
    m_device.seekFrame(at);
    m_playing = false;
    m_poll.stop();
    emit playingChanged();
    emit progressChanged();
}

void Player::toggle()
{
    if (m_playing)
        pause();
    else
        play();
}

void Player::seek(qreal fraction)
{
    if (!m_loaded)
        return;
    fraction = qBound(0.0, fraction, 1.0);
    m_idleProgress = fraction;
    m_device.seekFrame(qint64(fraction * qreal(m_device.frames())));
    m_shownFrame = double(m_device.frame());
    emit progressChanged();
}

void Player::poll()
{
    if (m_playing && m_sink) {
        const double dt = double(m_clock.restart()) / 1000.0;
        const qint64 queued = qMax<qint64>(0, m_sink->bufferSize() - m_sink->bytesFree()) / 4;
        const double audible = double(qMax<qint64>(0, m_device.frame() - queued));
        m_shownFrame += dt * m_outRate;
        const double error = audible - m_shownFrame;
        if (qAbs(error) > 0.25 * m_outRate) // seek, loop wrap or a stalled device: jump
            m_shownFrame = audible;
        else if (m_device.finished())       // tail of the sample: just run out on the clock
            m_shownFrame = qMin(m_shownFrame, double(m_device.frames()));
        else
            m_shownFrame = qMin(m_shownFrame + 0.05 * error, double(m_device.frame()));
    }
    emit progressChanged();
    if (m_playing && m_device.finished() && m_sink && m_sink->state() != QAudio::ActiveState) {
        m_sink->stop();
        m_device.seekFrame(0);
        m_playing = false;
        m_poll.stop();
        emit playingChanged();
        emit progressChanged();
    }
}
