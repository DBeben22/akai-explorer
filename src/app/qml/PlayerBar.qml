// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AkaiExplorer
import AkaiExplorer.Native

// Transport bar. The waveform doubles as the seek control.
Rectangle {
    id: root
    color: Theme.raised
    implicitHeight: 96

    Rectangle { width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20; anchors.rightMargin: 20
        spacing: 16

        // play / pause
        Button {
            id: playButton
            Layout.preferredWidth: 48; Layout.preferredHeight: 48
            enabled: player.loaded
            opacity: enabled ? 1 : 0.35
            activeFocusOnTab: true
            Accessible.name: player.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: player.toggle()
            background: Rectangle {
                radius: 24
                color: playButton.down ? Qt.darker(Theme.text, 1.2) : Theme.text
                scale: playButton.hovered && playButton.enabled ? 1.05 : 1
                Behavior on scale { NumberAnimation { duration: 90 } }
                Rectangle {
                    visible: playButton.visualFocus
                    anchors.fill: parent; anchors.margins: -4; radius: width / 2
                    color: "transparent"; border.width: 2; border.color: Theme.accent
                }
            }
            contentItem: Item {
                Icon {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: player.playing ? 0 : 1
                    width: 24; height: 24
                    name: player.playing ? "pause" : "play"
                    color: Theme.sidebar
                }
            }
            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: player.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
        }

        // what is loaded
        ColumnLayout {
            Layout.preferredWidth: Math.min(300, root.width * 0.26)
            Layout.maximumWidth: Math.min(300, root.width * 0.26)
            spacing: 3
            Text {
                Layout.fillWidth: true
                text: player.loaded ? player.title : (player.loading ? qsTr("Loading") : qsTr("Nothing loaded"))
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: player.loaded ? Theme.text : Theme.muted
                font.family: Theme.family
                font.pixelSize: Theme.title - 1
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: player.loaded ? player.detail : qsTr("Double-click a sample to listen to it.")
                elide: Text.ElideRight
                color: Theme.muted
                font.family: Theme.family
                font.pixelSize: Theme.small
            }
            Text {
                Layout.fillWidth: true
                visible: player.loaded
                text: player.facts.join(", ")
                elide: Text.ElideRight
                color: Theme.faint
                font.family: Theme.family
                font.pixelSize: Theme.small
            }
        }

        Text {
            Layout.preferredWidth: 62
            horizontalAlignment: Text.AlignRight
            text: player.loaded ? player.positionText : ""
            color: Theme.text
            font.family: Theme.family
            font.pixelSize: Theme.small + 1
        }

        // waveform / seek
        Item {
            id: waveBox
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            activeFocusOnTab: player.loaded
            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Playback position")
            Keys.onLeftPressed: player.seek(player.progress - 0.02)
            Keys.onRightPressed: player.seek(player.progress + 0.02)

            Waveform {
                id: wave
                anchors.fill: parent
                source: player      // the context object; "source" avoids shadowing its name
                progress: player.progress
                color: Theme.wave
                playedColor: Theme.accent
                loopColor: Qt.rgba(1, 1, 1, 0.07)
                opacity: player.loaded ? 1 : 0.5
            }
            // loop markers from the sample header
            Repeater {
                model: player.hasLoop ? [player.loopStart, player.loopEnd] : []
                Rectangle {
                    x: Math.round(modelData * waveBox.width)
                    width: 1; height: waveBox.height
                    color: Theme.muted; opacity: 0.6
                }
            }
            Rectangle { // playhead
                visible: player.loaded
                x: Math.round(player.progress * (waveBox.width - width))
                width: 2; height: parent.height
                radius: 1
                color: Theme.playhead
            }
            Rectangle { // hover position
                visible: seekArea.containsMouse && player.loaded && !seekArea.pressed
                x: seekArea.mouseX; width: 1; height: parent.height
                color: Theme.text; opacity: 0.35
            }
            MouseArea {
                id: seekArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: player.loaded
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                function seekTo(x) { player.seek(Math.max(0, Math.min(1, x / width))) }
                onPressed: (mouse) => { waveBox.forceActiveFocus(); seekTo(mouse.x) }
                onPositionChanged: (mouse) => { if (pressed) seekTo(mouse.x) }
            }
            Rectangle {
                visible: waveBox.activeFocus
                anchors.fill: parent; anchors.margins: -4
                color: "transparent"; radius: 4
                border.width: 1; border.color: Theme.accent
            }
        }

        Text {
            Layout.preferredWidth: 62
            text: player.loaded ? player.durationText : ""
            color: Theme.muted
            font.family: Theme.family
            font.pixelSize: Theme.small + 1
        }

        IconButton {
            iconName: "loop"
            active: player.looping
            tip: player.looping ? qsTr("Looping is on (L)") : (player.hasLoop ? qsTr("Repeat the sample's loop (L)") : qsTr("Repeat the sample (L)"))
            onClicked: player.looping = !player.looping
        }

        // volume
        RowLayout {
            spacing: 8
            Icon { width: 18; height: 18; name: "speaker"; color: Theme.muted }
            Item {
                id: vol
                Layout.preferredWidth: 96
                Layout.preferredHeight: 24
                activeFocusOnTab: true
                Accessible.role: Accessible.Slider
                Accessible.name: qsTr("Volume")
                Keys.onLeftPressed: player.volume = player.volume - 0.05
                Keys.onRightPressed: player.volume = player.volume + 0.05
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width; height: 4; radius: 2
                    color: Theme.line
                    Rectangle {
                        width: parent.width * player.volume; height: parent.height; radius: 2
                        color: volArea.containsMouse || volArea.pressed || vol.activeFocus ? Theme.accent : Theme.text
                    }
                }
                Rectangle {
                    visible: volArea.containsMouse || volArea.pressed || vol.activeFocus
                    x: (parent.width - width) * player.volume
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12; height: 12; radius: 6
                    color: Theme.text
                }
                MouseArea {
                    id: volArea
                    anchors.fill: parent
                    hoverEnabled: true
                    function setTo(x) { player.volume = Math.max(0, Math.min(1, x / width)) }
                    onPressed: (mouse) => { vol.forceActiveFocus(); setTo(mouse.x) }
                    onPositionChanged: (mouse) => { if (pressed) setTo(mouse.x) }
                    onWheel: (wheel) => player.volume = player.volume + (wheel.angleDelta.y > 0 ? 0.05 : -0.05)
                }
            }
        }
    }
}
