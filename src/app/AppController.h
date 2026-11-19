// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Exporter.h"
#include "FileModel.h"
#include "LibraryModel.h"
#include "Player.h"

#include <QObject>
#include <QStringList>
#include <QUrl>

#include <atomic>

/** Glue between the QML interface and the core library. Lives in the GUI thread. */
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *library READ library CONSTANT)
    Q_PROPERTY(QObject *files READ files CONSTANT)
    Q_PROPERTY(bool hasImages READ hasImages NOTIFY imagesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool hasVolume READ hasVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString volumeName READ volumeName NOTIFY volumeChanged)
    Q_PROPERTY(QString volumeDetail READ volumeDetail NOTIFY volumeChanged)
    Q_PROPERTY(QString volumeLength READ volumeLength NOTIFY volumeChanged)
    Q_PROPERTY(bool mergeStereo READ mergeStereo WRITE setMergeStereo NOTIFY optionsChanged)
    Q_PROPERTY(bool includeRaw READ includeRaw WRITE setIncludeRaw NOTIFY optionsChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportChanged)
    Q_PROPERTY(qreal exportProgress READ exportProgress NOTIFY exportChanged)
    Q_PROPERTY(QString exportLabel READ exportLabel NOTIFY exportChanged)
    Q_PROPERTY(QUrl lastOpenFolder READ lastOpenFolder NOTIFY foldersChanged)
    Q_PROPERTY(QUrl lastExportFolder READ lastExportFolder NOTIFY foldersChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
public:
    explicit AppController(Player *player, QObject *parent = nullptr);
    ~AppController() override;

    QObject *library() { return &m_library; }
    QObject *files() { return &m_files; }
    bool hasImages() const { return !m_library.disks().empty(); }
    bool busy() const { return m_busy; }
    bool hasVolume() const { return m_library.selectedVolume() != nullptr; }
    QString volumeName() const;
    QString volumeDetail() const;
    QString volumeLength() const;
    bool mergeStereo() const { return m_options.mergeStereo; }
    void setMergeStereo(bool on);
    bool includeRaw() const { return m_options.includeRaw; }
    void setIncludeRaw(bool on);
    bool exporting() const { return m_exporting; }
    qreal exportProgress() const { return m_exportTotal ? qreal(m_exportDone) / m_exportTotal : 0.0; }
    QString exportLabel() const { return m_exportLabel; }
    QUrl lastOpenFolder() const;
    QUrl lastExportFolder() const;
    QString version() const { return QStringLiteral(AKX_VERSION); }

    void openPaths(const QStringList &paths, bool replace);

    Q_INVOKABLE void openImages(const QList<QUrl> &urls);       // add to the library
    Q_INVOKABLE void closeImage(int diskIndex);
    Q_INVOKABLE void playPosition(int position);                // position in the volume's file list
    Q_INVOKABLE void playRelative(int delta);                   // next / previous audio file in the list
    Q_INVOKABLE void exportSelected(const QUrl &folder);
    Q_INVOKABLE void exportVolume(const QUrl &folder);
    Q_INVOKABLE void exportEverything(const QUrl &folder);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE bool isImageFile(const QUrl &url) const;

signals:
    void imagesChanged();
    void busyChanged();
    void volumeChanged();
    void optionsChanged();
    void exportChanged();
    void foldersChanged();
    void toast(const QString &message, bool isError);

private:
    struct PlannedJob { akx::ExportJob job; std::string dir; };
    void onVolumeSelected();
    void startExport(std::vector<PlannedJob> jobs, const QString &folder);
    void setBusy(bool on);

    Player *m_player;
    LibraryModel m_library;
    FileModel m_files;
    QStringList m_paths;
    akx::ExportOptions m_options;
    bool m_busy = false;
    bool m_exporting = false;
    int m_exportDone = 0, m_exportTotal = 0;
    QString m_exportLabel;
    std::atomic<bool> m_cancel{false};
    int m_openSerial = 0;
    int m_playSerial = 0;
};
