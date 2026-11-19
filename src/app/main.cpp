/*
 * AKAI Explorer - file-explorer-like GUI for AKAI S900/S1000/S3000 disk images.
 * Built on akaiutil by Klaus Michael Indlekofer.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "AppController.h"
#include "IconItem.h"
#include "Player.h"
#include "WaveformItem.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("akai-explorer"));
    QCoreApplication::setApplicationName(QStringLiteral("AKAI Explorer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(AKX_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Basic")); // unstyled base, the look comes from our own QML

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Browse AKAI S900/S1000/S3000 disk images, preview samples and export them to WAV."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("images"), QStringLiteral("Disk image files to open."), QStringLiteral("[image...]"));
    // for documentation and automated UI checks: render one frame to a PNG and quit
    QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Save a screenshot to <file> and quit."), QStringLiteral("file"));
    QCommandLineOption shotPlayOpt(QStringLiteral("screenshot-load"), QStringLiteral("Load the n-th file (from 0) into the player before the screenshot."), QStringLiteral("n"));
    // for automated checks of the audio path: play the n-th file and log the playhead
    QCommandLineOption playTestOpt(QStringLiteral("selftest-play"), QStringLiteral("Play the n-th file (from 0), print the playhead position and quit."), QStringLiteral("n"));
    parser.addOption(playTestOpt);
    // for automated checks of the export path: export every open image to <dir> and quit
    QCommandLineOption exportTestOpt(QStringLiteral("selftest-export"), QStringLiteral("Export everything to <dir> and quit."), QStringLiteral("dir"));
    parser.addOption(exportTestOpt);
    parser.addOption(shotOpt);
    parser.addOption(shotPlayOpt);
    parser.process(app);

    qmlRegisterType<WaveformItem>("AkaiExplorer.Native", 1, 0, "Waveform");
    qmlRegisterType<IconItem>("AkaiExplorer.Native", 1, 0, "Icon");
    qmlRegisterUncreatableType<Player>("AkaiExplorer.Native", 1, 0, "Player", QStringLiteral("provided by the application"));

    Player player;
    AppController controller(&player);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("player"), &player);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/AkaiExplorer/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (!parser.positionalArguments().isEmpty())
        controller.openPaths(parser.positionalArguments(), true);

    if (parser.isSet(shotOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const QString file = parser.value(shotOpt);
        const int loadAt = parser.isSet(shotPlayOpt) ? parser.value(shotPlayOpt).toInt() : -1;
        QTimer::singleShot(900, &app, [&controller, loadAt] {
            if (loadAt >= 0)
                controller.playPosition(loadAt);
        });
        QTimer::singleShot(1800, &app, [&player, window, file, loadAt] {
            if (loadAt >= 0) {
                player.pause();
                player.seek(0.36);
            }
            QTimer::singleShot(300, qApp, [window, file] {
                const bool ok = window && window->grabWindow().save(file);
                QCoreApplication::exit(ok ? 0 : 1);
            });
        });
    }
    if (parser.isSet(exportTestOpt)) {
        const QUrl dir = QUrl::fromLocalFile(parser.value(exportTestOpt));
        QObject::connect(&controller, &AppController::toast, &app, [](const QString &m, bool isError) {
            fprintf(stderr, "%s%s\n", isError ? "ERROR: " : "", qPrintable(m));
        });
        QTimer::singleShot(900, &app, [&controller, dir] {
            controller.exportEverything(dir);
            QObject::connect(&controller, &AppController::exportChanged, qApp, [&controller] {
                if (!controller.exporting())
                    QTimer::singleShot(100, qApp, [] { QCoreApplication::quit(); });
            });
        });
    }
    if (parser.isSet(playTestOpt)) {
        const int n = parser.value(playTestOpt).toInt();
        QTimer::singleShot(900, &app, [&controller, n] { controller.playPosition(n); });
        auto *log = new QTimer(&app);
        auto *ticks = new int(0);
        QObject::connect(log, &QTimer::timeout, &app, [&player, ticks] {
            fprintf(stderr, "t=%4d ms  loaded=%d playing=%d position=%s / %s\n", *ticks * 100, player.loaded(), player.playing(),
                    qPrintable(player.positionText()), qPrintable(player.durationText()));
            if (++*ticks > 12 && !player.playing())
                QCoreApplication::exit(player.loaded() ? 0 : 1);
            if (*ticks > 80)
                QCoreApplication::exit(2);
        });
        log->start(100);
    }
    return app.exec();
}
