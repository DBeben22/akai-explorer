// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AkaiExplorer
import AkaiExplorer.Native

// Shown while no disk image is open: says what the app does and how to start.
Item {
    id: root
    property bool dropActive: false
    signal openRequested()

    Rectangle {
        anchors.fill: parent
        anchors.margins: 28
        radius: 16
        color: root.dropActive ? Theme.accentTint : "transparent"
        border.width: 2
        border.color: root.dropActive ? Theme.accent : Theme.line
        Behavior on color { ColorAnimation { duration: 120 } }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 520)
            spacing: 0

            // the app's signature element, as a still: a waveform with its playhead
            Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 236
                Layout.preferredHeight: 72
                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    Repeater {
                        model: [10, 22, 40, 64, 48, 70, 52, 34, 56, 38, 26, 44, 30, 18, 26, 14, 20, 10, 14, 8, 10, 6, 8, 4, 6, 4]
                        Rectangle {
                            width: 5; height: modelData; radius: 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: index < 9 ? Theme.accent : Theme.wave
                        }
                    }
                }
                Rectangle { x: 80; width: 2; height: parent.height; color: Theme.playhead }
            }
            Text {
                Layout.topMargin: 28
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.dropActive ? qsTr("Drop to open") : qsTr("Open an AKAI disk image")
                color: Theme.text
                font.family: Theme.family
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }
            Text {
                Layout.topMargin: 10
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Browse floppy, hard disk and CD-ROM images from S900, S950, S1000, S1100 and S3000 series samplers. Listen to the samples and save them as WAV files.")
                color: Theme.muted
                font.family: Theme.family
                font.pixelSize: Theme.body
                lineHeight: 1.35
            }
            PillButton {
                Layout.topMargin: 26
                Layout.alignment: Qt.AlignHCenter
                primary: true
                iconName: "folder"
                text: qsTr("Choose image")
                onClicked: root.openRequested()
            }
            Text {
                Layout.topMargin: 14
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("or drop .img and .iso files anywhere in this window. Images are opened read-only.")
                color: Theme.faint
                font.family: Theme.family
                font.pixelSize: Theme.small + 1
            }
        }
    }
}
