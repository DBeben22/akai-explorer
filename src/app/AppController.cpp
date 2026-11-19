// SPDX-License-Identifier: GPL-2.0-or-later
#include "AppController.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>

#include <cmath>

using namespace akx;

namespace {

QVector<float> computePeaks(const Audio &a, int buckets = 2400)
{
    const size_t frames = a.frames();
    if (frames == 0)
        return {};
    buckets = int(qMin<size_t>(size_t(buckets), frames));
    QVector<float> peaks(buckets * 2);
    for (int b = 0; b < buckets; ++b) {
        const size_t f0 = frames * size_t(b) / size_t(buckets);
        const size_t f1 = qMax(f0 + 1, frames * size_t(b + 1) / size_t(buckets));
        float lo = 1.0f, hi = -1.0f;
        for (size_t f = f0; f < f1 && f < frames; ++f) {
            float v = 0.0f;
            for (int c = 0; c < a.channels; ++c)
                v += a.pcm[f * size_t(a.channels) + size_t(c)];
            v /= 32768.0f * float(a.channels);
            lo = qMin(lo, v);
            hi = qMax(hi, v);
        }
        peaks[2 * b] = qMin(lo, 0.0f);
        peaks[2 * b + 1] = qMax(hi, 0.0f);
    }
    return peaks;
}

QString collapseSpaces(const std::string &s)
{
    return QString::fromStdString(s).simplified();
}

struct DecodeResult {
    bool ok = false;
    QString error;
    Track track;
};

} // namespace

AppController::AppController(Player *player, QObject *parent) : QObject(parent), m_player(player)
{
    QSettings st;
    m_options.mergeStereo = st.value(QStringLiteral("export/mergeStereo"), true).toBool();
    m_options.includeRaw = st.value(QStringLiteral("export/includeRaw"), false).toBool();
    connect(&m_library, &LibraryModel::selectionChanged, this, &AppController::onVolumeSelected);
    connect(m_player, &Player::problem, this, [this](const QString &m) { emit toast(m, true); });
}

AppController::~AppController()
{
    m_cancel = true;
    QThreadPool::globalInstance()->waitForDone();
    Session::instance().close();
}

void AppController::setBusy(bool on)
{
    if (on == m_busy)
        return;
    m_busy = on;
    emit busyChanged();
}

QUrl AppController::lastOpenFolder() const
{
    const QString d = QSettings().value(QStringLiteral("folders/open")).toString();
    return QUrl::fromLocalFile(!d.isEmpty() && QDir(d).exists() ? d : QDir::homePath());
}

QUrl AppController::lastExportFolder() const
{
    const QString d = QSettings().value(QStringLiteral("folders/export")).toString();
    if (!d.isEmpty() && QDir(d).exists())
        return QUrl::fromLocalFile(d);
    return QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
}

void AppController::setMergeStereo(bool on)
{
    if (on == m_options.mergeStereo)
        return;
    m_options.mergeStereo = on;
    QSettings().setValue(QStringLiteral("export/mergeStereo"), on);
    emit optionsChanged();
}

void AppController::setIncludeRaw(bool on)
{
    if (on == m_options.includeRaw)
        return;
    m_options.includeRaw = on;
    QSettings().setValue(QStringLiteral("export/includeRaw"), on);
    emit optionsChanged();
}

bool AppController::isImageFile(const QUrl &url) const
{
    const QFileInfo fi(url.toLocalFile());
    return fi.isFile();
}

/* ---- opening images -------------------------------------------------------- */

void AppController::openImages(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const QUrl &u : urls) {
        const QString p = u.isLocalFile() ? u.toLocalFile() : u.toString();
        if (!p.isEmpty())
            paths << p;
    }
    if (paths.isEmpty())
        return;
    QSettings().setValue(QStringLiteral("folders/open"), QFileInfo(paths.first()).absolutePath());
    emit foldersChanged();
    openPaths(paths, false);
}

void AppController::closeImage(int diskIndex)
{
    const auto &disks = m_library.disks();
    if (diskIndex < 0 || diskIndex >= int(disks.size()))
        return;
    QStringList rest = m_paths;
    rest.removeAll(QString::fromStdString(disks[size_t(diskIndex)].path));
    openPaths(rest, true);
}

void AppController::openPaths(const QStringList &paths, bool replace)
{
    if (m_exporting) {
        emit toast(tr("Wait for the export to finish before changing the library."), true);
        return;
    }
    QStringList all = replace ? QStringList() : m_paths;
    for (const QString &p : paths) {
        const QString abs = QFileInfo(p).absoluteFilePath();
        if (!all.contains(abs))
            all << abs;
    }
    // remember what the user was looking at, to restore it after the rescan
    QString keepPath, keepPart, keepVol;
    {
        const PartitionEntry *pe = nullptr;
        const DiskEntry *de = nullptr;
        if (const VolumeEntry *v = m_library.selectedVolume(&pe, &de)) {
            keepPath = QString::fromStdString(de->path);
            keepPart = QString::fromStdString(pe->letter);
            keepVol = QString::fromStdString(v->name);
        }
    }
    m_player->clear();
    m_files.setPlayingPosition(-1);
    ++m_playSerial; // drop decodes that are still running
    const int serial = ++m_openSerial;
    setBusy(true);

    std::vector<std::string> list;
    for (const QString &p : all)
        list.push_back(QFile::encodeName(p).toStdString());
    QPointer<AppController> self(this);
    (void)QtConcurrent::run([self, list, serial, keepPath, keepPart, keepVol, added = paths.size(), replace] {
        std::string err;
        std::vector<DiskEntry> disks;
        if (!list.empty()) {
            Session::instance().open(list, &err);
            disks = Session::instance().disks();
        } else {
            Session::instance().close();
        }
        QMetaObject::invokeMethod(qApp, [=] {
            if (!self || serial != self->m_openSerial)
                return;
            AppController *c = self;
            c->m_paths.clear();
            for (const DiskEntry &d : disks) {
                const QString p = QFile::decodeName(QByteArray::fromStdString(d.path));
                if (!c->m_paths.contains(p))
                    c->m_paths << p;
            }
            c->m_library.setDisks(disks);
            // select: a newly added image's first volume, else the previous volume, else the first one
            bool selected = false;
            const auto &ds = c->m_library.disks();
            auto trySelect = [&](const QString &path, const QString &part, const QString &vol) {
                for (int row = 0; row < c->m_library.rowCount(); ++row) {
                    const QModelIndex ix = c->m_library.index(row);
                    if (ix.data(LibraryModel::KindRole).toString() != QLatin1String("volume"))
                        continue;
                    const DiskEntry &d = ds[size_t(ix.data(LibraryModel::DiskIndexRole).toInt())];
                    if (QString::fromStdString(d.path) != path)
                        continue;
                    if (!vol.isNull() && ix.data(LibraryModel::LabelRole).toString() != vol)
                        continue;
                    c->m_library.activate(row);
                    const PartitionEntry *pe = nullptr;
                    if (vol.isNull() || (c->m_library.selectedVolume(&pe) && QString::fromStdString(pe->letter) == part))
                        return true;
                }
                return false;
            };
            if (!replace && added > 0 && !disks.empty()) {
                // jump to the last image that was just added
                for (auto it = ds.rbegin(); it != ds.rend() && !selected; ++it)
                    if (list.size() >= size_t(added)
                        && std::find(list.end() - added, list.end(), it->path) != list.end())
                        selected = trySelect(QString::fromStdString(it->path), QString(), QString());
            }
            if (!selected && !keepPath.isEmpty())
                selected = trySelect(keepPath, keepPart, keepVol);
            if (!selected)
                c->m_library.selectFirstVolume();
            c->setBusy(false);
            emit c->imagesChanged();
            if (!err.empty())
                emit c->toast(QString::fromStdString(err), true);
        }, Qt::QueuedConnection);
    });
}

/* ---- current volume ---------------------------------------------------------- */

void AppController::onVolumeSelected()
{
    m_files.setVolume(m_library.selectedVolume());
    emit volumeChanged();
}

QString AppController::volumeName() const
{
    const VolumeEntry *v = m_library.selectedVolume();
    return v ? QString::fromStdString(v->name) : QString();
}

QString AppController::volumeDetail() const
{
    const PartitionEntry *p = nullptr;
    const DiskEntry *d = nullptr;
    const VolumeEntry *v = m_library.selectedVolume(&p, &d);
    if (!v)
        return {};
    const QString image = QFileInfo(QString::fromStdString(d->path)).fileName();
    if (v->isTakes)
        return tr("Hard-disk recordings in %1").arg(image);
    if (d->partitions.size() == 1)
        return tr("%1 volume in %2").arg(QString::fromStdString(v->typeLabel), image);
    return tr("%1 volume on partition %2 of %3").arg(QString::fromStdString(v->typeLabel), QString::fromStdString(p->letter), image);
}

QString AppController::volumeLength() const
{
    const VolumeEntry *v = m_library.selectedVolume();
    if (!v)
        return {};
    double secs = 0.0;
    for (const FileEntry &f : v->files)
        if (FileModel::isAudio(f) && f.meta.valid)
            secs += f.meta.seconds();
    if (secs <= 0.0)
        return {};
    if (secs < 60.0)
        return tr("%1 s of audio").arg(QLocale().toString(secs, 'f', 1));
    const int total = int(secs + 0.5);
    return tr("%1 min %2 s of audio").arg(total / 60).arg(total % 60);
}

/* ---- preview ----------------------------------------------------------------- */

void AppController::playPosition(int position)
{
    const PartitionEntry *p = nullptr;
    const VolumeEntry *v = m_library.selectedVolume(&p);
    if (!v || position < 0 || position >= int(v->files.size()))
        return;
    const FileEntry &f = v->files[size_t(position)];
    if (!FileModel::isAudio(f)) {
        emit toast(tr("%1 is a %2 and has no audio to preview.").arg(collapseSpaces(f.name), QString::fromStdString(f.typeLabel)), false);
        return;
    }
    auto refOf = [&](const FileEntry &e) {
        FileRef r;
        r.part = p->index;
        r.volume = v->index;
        r.file = e.index;
        r.take = e.kind == FileKind::Take;
        return r;
    };
    // with "stereo pairs" on, a -L/-R sample is previewed together with its partner
    const bool pair = m_options.mergeStereo && f.stereoPartner >= 0;
    const FileEntry &left = pair && f.stereoSide == 'R' ? v->files[size_t(f.stereoPartner)] : f;
    const FileRef a = refOf(left);
    const FileRef b = pair ? refOf(v->files[size_t(left.stereoPartner)]) : FileRef();

    Track base;
    base.meta = left.meta;
    base.stereoPair = pair;
    base.title = pair ? collapseSpaces(left.title.substr(0, left.title.size() - 2)) : collapseSpaces(f.title);
    base.detail = tr("%1 in %2").arg(QString::fromStdString(f.typeLabel), QString::fromStdString(v->name));

    m_files.setPlayingPosition(position);
    m_player->setLoading(true);
    const int serial = ++m_playSerial;
    QPointer<AppController> self(this);
    (void)QtConcurrent::run([self, serial, a, b, pair, base] {
        DecodeResult res;
        res.track = base;
        std::string err;
        Audio l;
        if (Session::instance().decode(a, l, &err)) {
            if (pair) {
                Audio r;
                if (Session::instance().decode(b, r, &err) && r.pcm.size() == l.pcm.size()) {
                    Audio st;
                    st.sampleRate = l.sampleRate;
                    st.channels = 2;
                    st.pcm.resize(l.pcm.size() * 2);
                    for (size_t i = 0; i < l.pcm.size(); ++i) {
                        st.pcm[2 * i] = l.pcm[i];
                        st.pcm[2 * i + 1] = r.pcm[i];
                    }
                    l = std::move(st);
                }
            }
            res.track.audio = std::move(l);
            res.track.meta.frames = uint32_t(res.track.audio.frames());
            res.track.meta.channels = res.track.audio.channels;
            res.track.peaks = computePeaks(res.track.audio);
            res.ok = true;
        } else {
            res.error = QString::fromStdString(err);
        }
        QMetaObject::invokeMethod(qApp, [self, serial, res] {
            if (!self || serial != self->m_playSerial)
                return;
            self->m_player->setLoading(false);
            if (res.ok) {
                self->m_player->setTrack(res.track, true);
            } else {
                self->m_files.setPlayingPosition(-1);
                emit self->toast(tr("Cannot read %1: %2").arg(res.track.title, res.error), true);
            }
        }, Qt::QueuedConnection);
    });
}

void AppController::playRelative(int delta)
{
    const int rows = m_files.count();
    if (rows == 0 || delta == 0)
        return;
    int row = m_files.rowOfPosition(m_files.playingPosition());
    const VolumeEntry &v = m_files.volume();
    for (int step = 0; step < rows; ++step) {
        row = row < 0 ? (delta > 0 ? 0 : rows - 1) : row + (delta > 0 ? 1 : -1);
        if (row < 0 || row >= rows)
            return;
        const int pos = m_files.positionAt(row);
        const FileEntry &f = v.files[size_t(pos)];
        if (!FileModel::isAudio(f))
            continue;
        // skip the right channel when the pair is previewed as one item
        if (m_options.mergeStereo && f.stereoSide == 'R' && f.stereoPartner >= 0 && delta > 0
            && f.stereoPartner == m_files.playingPosition())
            continue;
        m_files.select(row, false, false);
        playPosition(pos);
        return;
    }
}

/* ---- export -------------------------------------------------------------------- */

void AppController::exportSelected(const QUrl &folder)
{
    const PartitionEntry *p = nullptr;
    const VolumeEntry *v = m_library.selectedVolume(&p);
    const std::vector<int> sel = m_files.selectedPositions();
    if (!v || sel.empty())
        return;
    const std::string dir = QFile::encodeName(folder.toLocalFile()).toStdString();
    std::vector<PlannedJob> jobs;
    for (const ExportJob &j : planExport(*p, *v, sel, m_options))
        jobs.push_back({j, dir});
    if (jobs.empty()) {
        emit toast(tr("Nothing to export: the selection has no audio. Turn on \"Also export programs and other files\" to save them unconverted."), false);
        return;
    }
    startExport(std::move(jobs), folder.toLocalFile());
}

void AppController::exportVolume(const QUrl &folder)
{
    const PartitionEntry *p = nullptr;
    const VolumeEntry *v = m_library.selectedVolume(&p);
    if (!v)
        return;
    // a whole volume gets its own folder, named after the volume
    const QString sub = QDir(folder.toLocalFile()).filePath(QString::fromStdString(safeFileName(v->name)));
    const std::string dir = QFile::encodeName(sub).toStdString();
    std::vector<PlannedJob> jobs;
    for (const ExportJob &j : planExport(*p, *v, {}, m_options))
        jobs.push_back({j, dir});
    if (jobs.empty()) {
        emit toast(tr("This volume has nothing to export."), false);
        return;
    }
    startExport(std::move(jobs), sub);
}

void AppController::exportEverything(const QUrl &folder)
{
    std::vector<PlannedJob> jobs;
    const QDir root(folder.toLocalFile());
    for (const DiskEntry &d : m_library.disks()) {
        const QString image = QFileInfo(QString::fromStdString(d.path)).completeBaseName();
        for (const PartitionEntry &p : d.partitions) {
            for (const VolumeEntry &v : p.volumes) {
                QString sub = root.filePath(QString::fromStdString(safeFileName(image.toStdString())));
                if (d.partitions.size() > 1)
                    sub = QDir(sub).filePath(QString::fromStdString(safeFileName(p.letter)));
                sub = QDir(sub).filePath(QString::fromStdString(safeFileName(v.name)));
                const std::string dir = QFile::encodeName(sub).toStdString();
                for (const ExportJob &j : planExport(p, v, {}, m_options))
                    jobs.push_back({j, dir});
            }
        }
    }
    if (jobs.empty()) {
        emit toast(tr("The opened images have nothing to export."), false);
        return;
    }
    startExport(std::move(jobs), root.absolutePath());
}

void AppController::startExport(std::vector<PlannedJob> jobs, const QString &folder)
{
    if (m_exporting)
        return;
    QSettings().setValue(QStringLiteral("folders/export"), folder);
    emit foldersChanged();
    m_exporting = true;
    m_cancel = false;
    m_exportDone = 0;
    m_exportTotal = int(jobs.size());
    m_exportLabel = tr("Exporting %1 files").arg(m_exportTotal);
    emit exportChanged();

    QPointer<AppController> self(this);
    (void)QtConcurrent::run([self, jobs = std::move(jobs), folder] {
        int ok = 0, failed = 0;
        QString firstError;
        for (const PlannedJob &pj : jobs) {
            if (!self || self->m_cancel)
                break;
            std::string err;
            QDir().mkpath(QFile::decodeName(QByteArray::fromStdString(pj.dir)));
            if (runExportJob(pj.job, pj.dir, &err)) {
                ++ok;
            } else {
                ++failed;
                if (firstError.isEmpty())
                    firstError = QStringLiteral("%1: %2").arg(QString::fromStdString(pj.job.title).simplified(), QString::fromStdString(err));
            }
            const int done = ok + failed;
            const QString title = QString::fromStdString(pj.job.fileName);
            QMetaObject::invokeMethod(qApp, [self, done, title] {
                if (!self)
                    return;
                self->m_exportDone = done;
                self->m_exportLabel = tr("Exporting %1 of %2: %3").arg(done).arg(self->m_exportTotal).arg(title);
                emit self->exportChanged();
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(qApp, [self, ok, failed, firstError, folder, total = int(jobs.size())] {
            if (!self)
                return;
            const bool cancelled = self->m_cancel && ok + failed < total;
            self->m_exporting = false;
            emit self->exportChanged();
            if (failed > 0)
                emit self->toast(tr("Exported %1 files, %2 failed. First problem: %3").arg(ok).arg(failed).arg(firstError), true);
            else if (cancelled)
                emit self->toast(tr("Export cancelled after %1 of %2 files. The files already written stay in %3").arg(ok).arg(total).arg(folder), false);
            else
                emit self->toast(ok == 1 ? tr("Exported 1 file to %1").arg(folder) : tr("Exported %1 files to %2").arg(ok).arg(folder), false);
        }, Qt::QueuedConnection);
    });
}

void AppController::cancelExport()
{
    m_cancel = true;
}
