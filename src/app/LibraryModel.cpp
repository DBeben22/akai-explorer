// SPDX-License-Identifier: GPL-2.0-or-later
#include "LibraryModel.h"

#include <QFileInfo>
#include <QLocale>

using namespace akx;

LibraryModel::LibraryModel(QObject *parent) : QAbstractListModel(parent) {}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {{LabelRole, "label"}, {DetailRole, "detail"}, {DepthRole, "depth"}, {KindRole, "kind"},
            {ExpandedRole, "expanded"}, {ExpandableRole, "expandable"}, {SelectedRole, "selected"},
            {CountRole, "count"}, {DiskIndexRole, "diskIndex"}, {FloppyRole, "floppy"}};
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows[index.row()];
    const DiskEntry &d = m_disks[size_t(r.d)];
    const bool flat = d.partitions.size() == 1; // single partition: volumes hang directly below the image
    const QLocale loc;
    switch (role) {
    case KindRole: return r.kind == 0 ? QStringLiteral("image") : r.kind == 1 ? QStringLiteral("partition")
                                                                              : QStringLiteral("volume");
    case DepthRole: return r.kind == 0 ? 0 : r.kind == 1 ? 1 : (flat ? 1 : 2);
    case DiskIndexRole: return r.d;
    case FloppyRole: return QString::fromStdString(d.typeLabel).startsWith(QLatin1String("Floppy"));
    case ExpandableRole: return r.kind != 2;
    case ExpandedRole: return r.kind == 2 ? false : !m_collapsed.contains(key(r.d, r.kind == 0 ? -1 : r.p));
    case SelectedRole: return r.kind == 2 && r.d == m_selD && r.p == m_selP && r.v == m_selV;
    case LabelRole:
        if (r.kind == 0)
            return QFileInfo(QString::fromStdString(d.path)).fileName();
        if (r.kind == 1) {
            const QString letter = QString::fromStdString(d.partitions[size_t(r.p)].letter);
            return letter.startsWith(QLatin1String("DD")) ? tr("Recordings %1").arg(letter) : tr("Partition %1").arg(letter);
        }
        return QString::fromStdString(d.partitions[size_t(r.p)].volumes[size_t(r.v)].name);
    case DetailRole:
        if (r.kind == 0)
        {
            // short form for the narrow sidebar
            const QString type = QString::fromStdString(d.typeLabel);
            const QString kind = type.contains(QLatin1String("low density")) ? tr("DD floppy")
                                 : type.contains(QLatin1String("high density")) ? tr("HD floppy")
                                 : type.startsWith(QLatin1String("S900")) ? tr("S900 hard disk") : tr("Hard disk");
            return QStringLiteral("%1, %2").arg(kind, loc.formattedDataSize(qint64(d.sizeBytes), 1, QLocale::DataSizeTraditionalFormat));
        }
        if (r.kind == 1)
            return tr("%1 free").arg(loc.formattedDataSize(qint64(d.partitions[size_t(r.p)].freeBytes), 1, QLocale::DataSizeTraditionalFormat));
        return QString::fromStdString(d.partitions[size_t(r.p)].volumes[size_t(r.v)].typeLabel);
    case CountRole:
        return r.kind == 2 ? int(d.partitions[size_t(r.p)].volumes[size_t(r.v)].files.size()) : -1;
    default: return {};
    }
}

void LibraryModel::setDisks(const std::vector<DiskEntry> &disks)
{
    beginResetModel();
    m_disks = disks;
    m_collapsed.clear();
    m_selD = m_selP = m_selV = -1;
    rebuildRows();
    endResetModel();
    emit selectionChanged();
}

void LibraryModel::rebuildRows()
{
    m_rows.clear();
    for (int d = 0; d < int(m_disks.size()); ++d) {
        m_rows.push_back({0, d, -1, -1});
        if (m_collapsed.contains(key(d, -1)))
            continue;
        const auto &parts = m_disks[size_t(d)].partitions;
        const bool flat = parts.size() == 1;
        for (int p = 0; p < int(parts.size()); ++p) {
            if (!flat) {
                m_rows.push_back({1, d, p, -1});
                if (m_collapsed.contains(key(d, p)))
                    continue;
            }
            for (int v = 0; v < int(parts[size_t(p)].volumes.size()); ++v)
                m_rows.push_back({2, d, p, v});
        }
    }
}

const VolumeEntry *LibraryModel::selectedVolume(const PartitionEntry **part, const DiskEntry **disk) const
{
    if (m_selD < 0 || m_selD >= int(m_disks.size()))
        return nullptr;
    const DiskEntry &d = m_disks[size_t(m_selD)];
    if (m_selP < 0 || m_selP >= int(d.partitions.size()))
        return nullptr;
    const PartitionEntry &p = d.partitions[size_t(m_selP)];
    if (m_selV < 0 || m_selV >= int(p.volumes.size()))
        return nullptr;
    if (part)
        *part = &p;
    if (disk)
        *disk = &d;
    return &p.volumes[size_t(m_selV)];
}

bool LibraryModel::selectFirstVolume()
{
    // prefer the first volume that actually contains files
    int fallback = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].kind != 2)
            continue;
        if (fallback < 0)
            fallback = i;
        const Row &r = m_rows[i];
        if (!m_disks[size_t(r.d)].partitions[size_t(r.p)].volumes[size_t(r.v)].files.empty()) {
            activate(i);
            return true;
        }
    }
    if (fallback >= 0)
        activate(fallback);
    return fallback >= 0;
}

void LibraryModel::activate(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    const Row r = m_rows[row];
    if (r.kind != 2) {
        toggle(row);
        return;
    }
    if (r.d == m_selD && r.p == m_selP && r.v == m_selV)
        return;
    m_selD = r.d;
    m_selP = r.p;
    m_selV = r.v;
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1), {SelectedRole});
    emit selectionChanged();
}

void LibraryModel::toggle(int row)
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].kind == 2)
        return;
    const Row r = m_rows[row];
    const quint64 k = key(r.d, r.kind == 0 ? -1 : r.p);
    beginResetModel();
    if (!m_collapsed.remove(k))
        m_collapsed.insert(k);
    rebuildRows();
    endResetModel();
}
