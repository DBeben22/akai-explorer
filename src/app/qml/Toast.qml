// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import AkaiExplorer

// Transient message at the bottom of the content area.
Rectangle {
    id: root
    property bool isError: false

    function show(message, error) {
        label.text = message
        isError = error
        opacity = 1
        hideTimer.interval = error ? 9000 : 5000
        hideTimer.restart()
    }

    width: Math.min(parent.width - 48, Math.max(260, label.implicitWidth + 76))
    height: label.height + 24
    radius: Theme.radius
    color: Theme.raised
    border.width: 1
    border.color: isError ? Theme.danger : Theme.line
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 160 } }

    Timer { id: hideTimer; onTriggered: root.opacity = 0 }

    Rectangle {
        id: dot
        width: 8; height: 8; radius: 4
        x: 16; y: 20
        color: root.isError ? Theme.danger : Theme.accent
    }
    Text {
        id: label
        x: 34; y: 12
        width: Math.min(implicitWidth, root.parent.width - 48 - 76)
        wrapMode: Text.Wrap
        color: Theme.text
        font.family: Theme.family
        font.pixelSize: Theme.body
        lineHeight: 1.25
    }
    IconButton {
        anchors.right: parent.right; anchors.rightMargin: 4
        y: 5
        iconName: "close"
        tip: qsTr("Dismiss")
        iconSize: 14
        implicitWidth: 28; implicitHeight: 28
        onClicked: root.opacity = 0
    }
}
