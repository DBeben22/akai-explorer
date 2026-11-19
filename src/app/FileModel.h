// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "AkaiSession.h"

#include <QAbstractListModel>
#include <QSet>

/** Files of the selected volume, with text/kind filter and multi-selection. */
class FileModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterChanged)
    Q_PROPERTY(QString kindFilter READ kindFilter WRITE setKindFilter NOTIFY filterChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedAudioCount READ selectedAudioCount NOTIFY selectionChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(int audioCount READ audioCount NOTIFY countChanged)
    Q_PROPERTY(int otherCount READ otherCount NOTIFY countChanged)
    Q_PROPERTY(int playingPosition READ playingPosition NOTIFY playingChanged)
public:
    enum Roles { TitleRole = Qt::UserRole + 1, NameRole, KindRole, TypeRole, RateRole, DurationRole, SizeRole,
                 AudioRole, SelectedRole, PositionRole, StereoRole, PlayingRole, BitsRole };

    explicit FileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setVolume(const akx::VolumeEntry *volume);
    const akx::VolumeEntry &volume() const { return m_volume; }

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);
    QString kindFilter() const { return m_kindFilter; }
    void setKindFilter(const QString &kind); // "all" | "audio" | "other"

    int count() const { return m_rows.size(); }
    int totalCount() const { return int(m_volume.files.size()); }
    int audioCount() const { return m_audioCount; }
    int otherCount() const { return totalCount() - m_audioCount; }
    int selectedCount() const { return m_selected.size(); }
    int selectedAudioCount() const;
    int playingPosition() const { return m_playing; }
    void setPlayingPosition(int position);

    std::vector<int> selectedPositions() const;

    /** position (index into the volume's file list) shown at `row`, -1 if none */
    Q_INVOKABLE int positionAt(int row) const;
    Q_INVOKABLE int rowOfPosition(int position) const;
    /** mouse selection: plain click, Ctrl-click (toggle) and Shift-click (range) */
    Q_INVOKABLE void select(int row, bool toggle, bool range);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();

    static bool isAudio(const akx::FileEntry &f)
    {
        return f.kind == akx::FileKind::Sample || f.kind == akx::FileKind::Take;
    }

signals:
    void filterChanged();
    void selectionChanged();
    void countChanged();
    void playingChanged();

private:
    void refilter();
    void notifyRows(const QVector<int> &roles);

    akx::VolumeEntry m_volume;
    QVector<int> m_rows; // positions into m_volume.files
    QSet<int> m_selected; // positions
    QString m_filterText;
    QString m_kindFilter = QStringLiteral("all");
    int m_anchorRow = -1;
    int m_audioCount = 0;
    int m_playing = -1;
};
