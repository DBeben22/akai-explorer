// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AkaiExplorer
import AkaiExplorer.Native

// Library column: the opened images with their partitions and volumes.
Rectangle {
    id: root
    signal openRequested()
    color: Theme.sidebar

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // wordmark
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            Row {
                x: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                Row { // logo: four bars, one lit
                    spacing: 3
                    anchors.verticalCenter: parent.verticalCenter
                    Repeater {
                        model: [10, 20, 14, 7]
                        Rectangle {
                            width: 4; height: modelData; radius: 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: index === 1 ? Theme.accent : Theme.text
                        }
                    }
                }
                Text {
                    text: "AKAI Explorer"
                    color: Theme.text
                    font.family: Theme.family
                    font.pixelSize: Theme.title
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            Text {
                x: 20
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Disk images")
                color: Theme.muted
                font.family: Theme.family
                font.pixelSize: Theme.small + 1
                font.weight: Font.DemiBold
            }
            IconButton {
                anchors.right: parent.right; anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                iconName: "plus"
                tip: qsTr("Open image (Ctrl+O)")
                onClicked: root.openRequested()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: app.library
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                id: scroll
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: Theme.line; visible: scroll.size < 1.0 }
            }

            delegate: Item {
                id: row
                required property int index
                required property string label
                required property string detail
                required property int depth
                required property string kind
                required property bool expanded
                required property bool expandable
                required property bool selected
                required property int count
                required property int diskIndex
                required property bool floppy

                width: list.width
                height: kind === "image" ? 52 : 36

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    radius: 6
                    color: row.selected ? Theme.accentTint : (area.containsMouse ? Theme.hover : "transparent")
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: app.library.activate(row.index)
                }
                Icon {
                    id: chevron
                    visible: row.expandable
                    x: 12 + row.depth * 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14; height: 14
                    name: row.expanded ? "chevron-down" : "chevron-right"
                    color: Theme.faint
                }
                Icon {
                    id: glyph
                    x: 30 + row.depth * 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18; height: 18
                    name: row.kind === "image" ? (row.floppy ? "floppy" : "disk") : row.kind === "partition" ? "partition" : "volume"
                    color: row.selected ? Theme.accent : (row.kind === "image" ? Theme.text : Theme.muted)
                }
                Column {
                    anchors.left: glyph.right; anchors.leftMargin: 10
                    anchors.right: trailing.left; anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        width: parent.width
                        text: row.label
                        elide: Text.ElideMiddle
                        color: row.selected ? Theme.accent : Theme.text
                        font.family: Theme.family
                        font.pixelSize: Theme.body
                        font.weight: row.kind === "image" || row.selected ? Font.DemiBold : Font.Normal
                    }
                    Text {
                        visible: row.kind === "image"
                        width: parent.width
                        text: row.detail
                        elide: Text.ElideRight
                        color: Theme.faint
                        font.family: Theme.family
                        font.pixelSize: Theme.small
                    }
                }
                Item {
                    id: trailing
                    anchors.right: parent.right; anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: row.kind === "image" ? 28 : countText.implicitWidth
                    height: 28
                    Text {
                        id: countText
                        visible: row.kind === "volume"
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        text: row.count
                        color: row.selected ? Theme.accent : Theme.faint
                        font.family: Theme.family
                        font.pixelSize: Theme.small
                    }
                    IconButton {
                        visible: row.kind === "image" && (area.containsMouse || hovered || visualFocus)
                        anchors.centerIn: parent
                        implicitWidth: 28; implicitHeight: 28
                        iconName: "close"
                        iconSize: 14
                        tip: qsTr("Close this image")
                        onClicked: app.closeImage(row.diskIndex)
                    }
                }
            }

            Text {
                visible: list.count === 0
                x: 20; y: 6
                width: parent.width - 40
                wrapMode: Text.Wrap
                text: qsTr("No image open yet.")
                color: Theme.faint
                font.family: Theme.family
                font.pixelSize: Theme.small + 1
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.margins: 20
            wrapMode: Text.Wrap
            text: qsTr("Version %1. Reads disks with akaiutil by K. M. Indlekofer.").arg(app.version)
            color: Theme.faint
            font.family: Theme.family
            font.pixelSize: Theme.small - 1
            lineHeight: 1.3
        }
    }
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }
}
