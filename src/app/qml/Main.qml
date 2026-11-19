// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AkaiExplorer
import AkaiExplorer.Native

ApplicationWindow {
    id: window
    width: 1200
    height: 760
    minimumWidth: 860
    minimumHeight: 540
    visible: true
    title: app.hasVolume ? app.volumeName + " - AKAI Explorer" : "AKAI Explorer"
    color: Theme.base

    property string exportTarget: "volume"

    function chooseImages() { openDialog.currentFolder = app.lastOpenFolder; openDialog.open() }
    function chooseExportFolder(what) {
        exportTarget = what
        folderDialog.currentFolder = app.lastExportFolder
        folderDialog.open()
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open AKAI disk image")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Disk images (*.img *.iso *.bin *.hda *.ima *.dsk)"), qsTr("All files (*)")]
        onAccepted: app.openImages(selectedFiles)
    }
    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a folder for the exported files")
        onAccepted: {
            if (window.exportTarget === "selected")
                app.exportSelected(selectedFolder)
            else if (window.exportTarget === "everything")
                app.exportEverything(selectedFolder)
            else
                app.exportVolume(selectedFolder)
        }
    }

    Shortcut { sequence: StandardKey.Open; onActivated: window.chooseImages() }
    Shortcut { sequence: "Ctrl+E"; enabled: app.hasVolume && !app.exporting; onActivated: window.chooseExportFolder(app.files.selectedCount > 0 ? "selected" : "volume") }
    Shortcut { sequence: StandardKey.Find; enabled: app.hasVolume; onActivated: header.searchField.forceActiveFocus() }
    Shortcut { sequence: StandardKey.SelectAll; enabled: app.hasVolume && !header.searchField.activeFocus; onActivated: app.files.selectAll() }
    Shortcut { sequence: "Space"; enabled: player.loaded && !header.searchField.activeFocus; onActivated: player.toggle() }
    Shortcut { sequence: "L"; enabled: !header.searchField.activeFocus; onActivated: player.looping = !player.looping }
    Shortcut { sequence: "Ctrl+Right"; enabled: app.hasVolume; onActivated: app.playRelative(1) }
    Shortcut { sequence: "Ctrl+Left"; enabled: app.hasVolume; onActivated: app.playRelative(-1) }

    Connections {
        target: app
        function onToast(message, isError) { toast.show(message, isError) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.preferredWidth: 272
                Layout.fillHeight: true
                onOpenRequested: window.chooseImages()
            }

            Item {
                id: content
                Layout.fillWidth: true
                Layout.fillHeight: true

                EmptyState {
                    anchors.fill: parent
                    visible: !app.hasImages && !app.busy
                    dropActive: drop.containsDrag
                    onOpenRequested: window.chooseImages()
                }

                ColumnLayout {
                    anchors.fill: parent
                    visible: app.hasImages
                    spacing: 0
                    VolumeHeader {
                        id: header
                        Layout.fillWidth: true
                        onExportRequested: (what) => window.chooseExportFolder(what)
                    }
                    FileList {
                        id: fileList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }

                // reading a large image takes a moment
                Text {
                    anchors.centerIn: parent
                    visible: app.busy && !app.hasImages
                    text: qsTr("Reading disk image")
                    color: Theme.muted
                    font.family: Theme.family
                    font.pixelSize: Theme.title
                }

                // drop highlight while images are already open
                Rectangle {
                    anchors.fill: parent; anchors.margins: 10
                    visible: drop.containsDrag && app.hasImages
                    radius: 12
                    color: Qt.rgba(0.07, 0.09, 0.12, 0.92)
                    border.width: 2; border.color: Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Drop to add to the library")
                        color: Theme.text
                        font.family: Theme.family; font.pixelSize: 22; font.weight: Font.DemiBold
                    }
                }

                // export progress
                Rectangle {
                    id: exportCard
                    visible: app.exporting
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 18
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width - 48, 520)
                    height: 64
                    radius: Theme.radius
                    color: Theme.raised
                    border.width: 1; border.color: Theme.line
                    Text {
                        x: 16; y: 12
                        width: parent.width - 120
                        text: app.exportLabel
                        elide: Text.ElideMiddle
                        color: Theme.text
                        font.family: Theme.family; font.pixelSize: Theme.body
                    }
                    Rectangle {
                        x: 16; y: 42; width: parent.width - 32; height: 4; radius: 2
                        color: Theme.line
                        Rectangle { width: parent.width * app.exportProgress; height: parent.height; radius: 2; color: Theme.accent }
                    }
                    PillButton {
                        anchors.right: parent.right; anchors.rightMargin: 10
                        y: 6
                        implicitHeight: 28
                        text: qsTr("Cancel")
                        onClicked: app.cancelExport()
                    }
                }

                Toast {
                    id: toast
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: app.exporting ? 94 : 18
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        PlayerBar {
            Layout.fillWidth: true
        }
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (event) => { if (event.hasUrls) app.openImages(event.urls) }
    }
}
