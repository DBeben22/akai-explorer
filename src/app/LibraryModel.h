// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "AkaiSession.h"

#include <QAbstractListModel>
#include <QSet>

/** Flattened, collapsible tree: image -> partition -> volume (sidebar). */
class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { LabelRole = Qt::UserRole + 1, DetailRole, DepthRole, KindRole, ExpandedRole, ExpandableRole,
                 SelectedRole, CountRole, DiskIndexRole, FloppyRole };

    explicit LibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDisks(const std::vector<akx::DiskEntry> &disks);
    const std::vector<akx::DiskEntry> &disks() const { return m_disks; }

    /** Currently selected volume, or nullptr. */
    const akx::VolumeEntry *selectedVolume(const akx::PartitionEntry **part = nullptr,
                                           const akx::DiskEntry **disk = nullptr) const;
    bool selectFirstVolume();

    Q_INVOKABLE void activate(int row); // click: select a volume or toggle a branch
    Q_INVOKABLE void toggle(int row);

signals:
    void selectionChanged();

private:
    struct Row { int kind; int d; int p; int v; }; // kind: 0 image, 1 partition, 2 volume
    void rebuildRows();
    static quint64 key(int d, int p) { return (quint64(quint32(d)) << 32) | quint32(p + 1); }

    std::vector<akx::DiskEntry> m_disks;
    QVector<Row> m_rows;
    QSet<quint64> m_collapsed;
    int m_selD = -1, m_selP = -1, m_selV = -1;
};
