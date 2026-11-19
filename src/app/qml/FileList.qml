// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AkaiExplorer
import AkaiExplorer.Native

// The files of the selected volume, laid out like a track list.
Item {
    id: root
    property alias view: list

    // column widths shared by header and rows
    readonly property int colLead: 52
    readonly property int colType: 190
    readonly property int colChannel: 72
    readonly property int colRate: 84
    readonly property int colLength: 84
    readonly property int colSize: 84
    readonly property int sidePad: 20
    readonly property bool roomy: width > 820   // hide type and channel columns when narrow

    component HeadCell: Text {
        color: Theme.faint
        font.family: Theme.family
        font.pixelSize: Theme.small
        elide: Text.ElideRight
    }
    component Cell: Text {
        color: Theme.muted
        font.family: Theme.family
        font.pixelSize: Theme.small + 1
        elide: Text.ElideRight
    }

    RowLayout {
        id: header
        x: root.sidePad + 8
        width: parent.width - 2 * (root.sidePad + 8)
        height: 30
        spacing: 0
        Item { Layout.preferredWidth: root.colLead }
        HeadCell { Layout.fillWidth: true; text: qsTr("Name") }
        HeadCell { visible: root.roomy; Layout.preferredWidth: root.colType; text: qsTr("Type") }
        HeadCell { visible: root.roomy; Layout.preferredWidth: root.colChannel; text: qsTr("Channel") }
        HeadCell { Layout.preferredWidth: root.colRate; text: qsTr("Rate"); horizontalAlignment: Text.AlignRight }
        HeadCell { Layout.preferredWidth: root.colLength; text: qsTr("Length"); horizontalAlignment: Text.AlignRight }
        HeadCell { Layout.preferredWidth: root.colSize; text: qsTr("Size"); horizontalAlignment: Text.AlignRight }
    }
    Rectangle { anchors.top: header.bottom; x: root.sidePad; width: parent.width - 2 * root.sidePad; height: 1; color: Theme.line }

    ListView {
        id: list
        anchors.top: header.bottom; anchors.topMargin: 5
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        model: app.files
        boundsBehavior: Flickable.StopAtBounds
        activeFocusOnTab: true
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: false
        currentIndex: -1
        bottomMargin: 12
        ScrollBar.vertical: ScrollBar {
            id: scroll
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle { implicitWidth: 8; radius: 4; color: Theme.line; visible: scroll.size < 1.0 }
        }

        function move(delta, extend) {
            if (count === 0)
                return
            var next = Math.max(0, Math.min(count - 1, currentIndex < 0 ? (delta > 0 ? 0 : count - 1) : currentIndex + delta))
            currentIndex = next
            app.files.select(next, false, extend)
            positionViewAtIndex(next, ListView.Contain)
        }
        Keys.onDownPressed: (event) => move(1, event.modifiers & Qt.ShiftModifier)
        Keys.onUpPressed: (event) => move(-1, event.modifiers & Qt.ShiftModifier)
        Keys.onReturnPressed: if (currentIndex >= 0) app.playPosition(app.files.positionAt(currentIndex))
        Keys.onEnterPressed: if (currentIndex >= 0) app.playPosition(app.files.positionAt(currentIndex))
        Keys.onEscapePressed: app.files.clearSelection()

        Rectangle { // keyboard focus indication for the list as a whole
            visible: list.activeFocus && list.currentIndex < 0 && list.count > 0
            x: root.sidePad; width: list.width - 2 * root.sidePad; height: 2
            color: Theme.accent
        }

        delegate: Item {
            id: row
            required property int index
            required property string title
            required property string name
            required property string kind
            required property string type
            required property string rate
            required property string duration
            required property string size
            required property string stereo
            required property bool audio
            required property bool selected
            required property bool playing
            required property int position

            width: list.width
            height: Theme.rowHeight
            readonly property bool nowPlaying: playing && player.loaded

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: root.sidePad; anchors.rightMargin: root.sidePad
                radius: 6
                color: row.selected ? Theme.accentTint : (area.containsMouse ? Theme.hover : "transparent")
                border.width: list.activeFocus && list.currentIndex === row.index ? 1 : 0
                border.color: Theme.accent
            }
            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                onClicked: (mouse) => {
                    list.forceActiveFocus()
                    list.currentIndex = row.index
                    app.files.select(row.index, mouse.modifiers & Qt.ControlModifier, mouse.modifiers & Qt.ShiftModifier)
                }
                onDoubleClicked: if (row.audio) app.playPosition(row.position)
            }
            RowLayout {
                x: root.sidePad + 8
                width: parent.width - 2 * (root.sidePad + 8)
                height: parent.height
                spacing: 0

                Item { // leading cell: file glyph, play button on hover, level bars while playing
                    Layout.preferredWidth: root.colLead
                    Layout.fillHeight: true
                    Icon {
                        visible: !lead.visible && !bars.visible
                        x: 8; anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 18
                        name: row.audio ? "wave" : "file"
                        color: row.audio ? Theme.muted : Theme.faint
                    }
                    Row {
                        id: bars
                        visible: row.nowPlaying && !area.containsMouse && !lead.hovered
                        x: 9; anchors.verticalCenter: parent.verticalCenter
                        spacing: 3
                        Repeater {
                            model: 4
                            Rectangle {
                                id: bar
                                width: 3; radius: 1.5
                                anchors.bottom: parent.bottom
                                color: Theme.accent
                                height: [8, 16, 11, 6][index]
                                SequentialAnimation on height {
                                    running: bars.visible && player.playing
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 4 + (index * 7) % 13; duration: 260 + index * 40 }
                                    NumberAnimation { to: 16 - (index * 5) % 9; duration: 300 + index * 30 }
                                }
                            }
                        }
                        height: 16
                    }
                    IconButton {
                        id: lead
                        visible: row.audio && (area.containsMouse || hovered || visualFocus)
                        x: 0; anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 34; implicitHeight: 34
                        iconName: row.nowPlaying && player.playing ? "pause" : "play"
                        iconColor: Theme.text
                        tip: row.nowPlaying && player.playing ? qsTr("Pause") : qsTr("Play %1").arg(row.title)
                        activeFocusOnTab: false
                        onClicked: row.nowPlaying ? player.toggle() : app.playPosition(row.position)
                    }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.rightMargin: 16
                    text: row.title
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: row.nowPlaying ? Theme.accent : (row.audio ? Theme.text : Theme.muted)
                    font.family: Theme.family
                    font.pixelSize: Theme.body
                    font.weight: row.nowPlaying ? Font.DemiBold : Font.Normal
                }
                Cell { visible: root.roomy; Layout.preferredWidth: root.colType; text: row.type }
                Cell { visible: root.roomy; Layout.preferredWidth: root.colChannel; text: row.stereo }
                Cell { Layout.preferredWidth: root.colRate; text: row.rate; horizontalAlignment: Text.AlignRight }
                Cell { Layout.preferredWidth: root.colLength; text: row.duration; horizontalAlignment: Text.AlignRight }
                Cell { Layout.preferredWidth: root.colSize; text: row.size; horizontalAlignment: Text.AlignRight }
            }
        }

        // nothing to list: say why
        Column {
            visible: list.count === 0
            anchors.horizontalCenter: parent.horizontalCenter
            y: 56
            spacing: 8
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: app.files.totalCount === 0 ? qsTr("This volume is empty") : qsTr("No file matches the filter")
                color: Theme.text
                font.family: Theme.family; font.pixelSize: Theme.title; font.weight: Font.DemiBold
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: app.files.totalCount === 0 ? qsTr("Pick another volume in the list on the left.")
                                                 : qsTr("Clear the search field or choose \"All\".")
                color: Theme.muted
                font.family: Theme.family; font.pixelSize: Theme.body
            }
        }
    }
}
