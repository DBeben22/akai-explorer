// SPDX-License-Identifier: GPL-2.0-or-later
#include "FileModel.h"

#include <QLocale>

using namespace akx;

FileModel::FileModel(QObject *parent) : QAbstractListModel(parent) {}

int FileModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QHash<int, QByteArray> FileModel::roleNames() const
{
    return {{TitleRole, "title"}, {NameRole, "name"}, {KindRole, "kind"}, {TypeRole, "type"},
            {RateRole, "rate"}, {DurationRole, "duration"}, {SizeRole, "size"}, {AudioRole, "audio"},
            {SelectedRole, "selected"}, {PositionRole, "position"}, {StereoRole, "stereo"},
            {PlayingRole, "playing"}, {BitsRole, "bits"}};
}

static QString durationText(double secs)
{
    if (secs >= 60.0) {
        const int total = int(secs + 0.5);
        return QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0'));
    }
    if (secs > 0.0 && secs < 0.1) // very short one-shots: milliseconds are more telling than "0.00 s"
        return QStringLiteral("%1 ms").arg(QLocale().toString(secs * 1000.0, 'f', secs < 0.01 ? 1 : 0));
    return QStringLiteral("%1 s").arg(QLocale().toString(secs, 'f', secs < 10.0 ? 2 : 1));
}

QVariant FileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const int pos = m_rows[index.row()];
    const FileEntry &f = m_volume.files[size_t(pos)];
    switch (role) {
    case TitleRole: return QString::fromStdString(f.title);
    case NameRole: return QString::fromStdString(f.name);
    case KindRole: return QString::fromLatin1(kindName(f.kind));
    case TypeRole: return QString::fromStdString(f.typeLabel);
    case AudioRole: return isAudio(f);
    case PositionRole: return pos;
    case SelectedRole: return m_selected.contains(pos);
    case PlayingRole: return pos == m_playing || (m_playing >= 0 && f.stereoPartner == m_playing && isAudio(f)
                                                   && m_volume.files[size_t(m_playing)].stereoPartner == pos);
    case StereoRole:
        if (f.meta.channels == 2)
            return QStringLiteral("Stereo");
        return f.stereoSide == 'L' ? QStringLiteral("Left") : f.stereoSide == 'R' ? QStringLiteral("Right") : QString();
    case RateRole:
        return f.meta.valid ? QStringLiteral("%1 kHz").arg(QLocale().toString(f.meta.sampleRate / 1000.0, 'f', 1)) : QString();
    case BitsRole: return f.meta.valid ? QStringLiteral("%1-bit").arg(f.meta.bits) : QString();
    case DurationRole: return f.meta.valid ? durationText(f.meta.seconds()) : QString();
    case SizeRole: return QLocale().formattedDataSize(f.sizeBytes, f.sizeBytes < 1024 * 10 ? 1 : 0, QLocale::DataSizeTraditionalFormat);
    default: return {};
    }
}

void FileModel::setVolume(const VolumeEntry *volume)
{
    beginResetModel();
    m_volume = volume ? *volume : VolumeEntry();
    m_selected.clear();
    m_anchorRow = -1;
    m_playing = -1;
    m_filterText.clear();
    m_kindFilter = QStringLiteral("all");
    m_audioCount = 0;
    for (const FileEntry &f : m_volume.files)
        m_audioCount += isAudio(f) ? 1 : 0;
    m_rows.clear();
    for (int i = 0; i < int(m_volume.files.size()); ++i)
        m_rows.push_back(i);
    endResetModel();
    emit filterChanged();
    emit countChanged();
    emit selectionChanged();
    emit playingChanged();
}

void FileModel::refilter()
{
    beginResetModel();
    m_rows.clear();
    const QString needle = m_filterText.trimmed();
    for (int i = 0; i < int(m_volume.files.size()); ++i) {
        const FileEntry &f = m_volume.files[size_t(i)];
        if (m_kindFilter == QLatin1String("audio") && !isAudio(f))
            continue;
        if (m_kindFilter == QLatin1String("other") && isAudio(f))
            continue;
        if (!needle.isEmpty() && !QString::fromStdString(f.name).contains(needle, Qt::CaseInsensitive)
            && !QString::fromStdString(f.typeLabel).contains(needle, Qt::CaseInsensitive))
            continue;
        m_rows.push_back(i);
    }
    // a hidden file must not stay selected: "Export selected" only acts on what is visible
    const QSet<int> visible(m_rows.cbegin(), m_rows.cend());
    m_selected.intersect(visible);
    m_anchorRow = -1;
    endResetModel();
    emit countChanged();
    emit selectionChanged();
}

void FileModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    refilter();
    emit filterChanged();
}

void FileModel::setKindFilter(const QString &kind)
{
    if (kind == m_kindFilter)
        return;
    m_kindFilter = kind;
    refilter();
    emit filterChanged();
}

int FileModel::selectedAudioCount() const
{
    int n = 0;
    for (int pos : m_selected)
        n += isAudio(m_volume.files[size_t(pos)]) ? 1 : 0;
    return n;
}

std::vector<int> FileModel::selectedPositions() const
{
    std::vector<int> v(m_selected.cbegin(), m_selected.cend());
    std::sort(v.begin(), v.end());
    return v;
}

int FileModel::positionAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows[row] : -1;
}

int FileModel::rowOfPosition(int position) const
{
    return m_rows.indexOf(position);
}

void FileModel::notifyRows(const QVector<int> &roles)
{
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1), roles);
}

void FileModel::select(int row, bool toggle, bool range)
{
    if (row < 0 || row >= m_rows.size())
        return;
    if (range && m_anchorRow >= 0 && m_anchorRow < m_rows.size()) {
        if (!toggle)
            m_selected.clear();
        for (int r = qMin(row, m_anchorRow); r <= qMax(row, m_anchorRow); ++r)
            m_selected.insert(m_rows[r]);
    } else if (toggle) {
        const int pos = m_rows[row];
        if (!m_selected.remove(pos))
            m_selected.insert(pos);
        m_anchorRow = row;
    } else {
        m_selected.clear();
        m_selected.insert(m_rows[row]);
        m_anchorRow = row;
    }
    notifyRows({SelectedRole});
    emit selectionChanged();
}

void FileModel::selectAll()
{
    m_selected = QSet<int>(m_rows.cbegin(), m_rows.cend());
    notifyRows({SelectedRole});
    emit selectionChanged();
}

void FileModel::clearSelection()
{
    if (m_selected.isEmpty())
        return;
    m_selected.clear();
    notifyRows({SelectedRole});
    emit selectionChanged();
}

void FileModel::setPlayingPosition(int position)
{
    if (position == m_playing)
        return;
    m_playing = position;
    notifyRows({PlayingRole});
    emit playingChanged();
}
