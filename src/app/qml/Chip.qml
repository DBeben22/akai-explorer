// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import AkaiExplorer

// Filter chip: one of a small set of mutually exclusive choices.
Button {
    id: control
    property bool selected: false
    property int count: -1

    implicitHeight: 30
    leftPadding: 13
    rightPadding: 13
    font.family: Theme.family
    font.pixelSize: Theme.small + 1
    activeFocusOnTab: true

    contentItem: Row {
        spacing: 6
        Text {
            text: control.text
            font: control.font
            color: control.selected ? Theme.accentInk : Theme.text
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: control.count >= 0
            text: control.count
            font: control.font
            color: control.selected ? Theme.accentInk : Theme.muted
            opacity: control.selected ? 0.7 : 1
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    background: Rectangle {
        radius: height / 2
        color: control.selected ? Theme.text : (control.hovered ? Theme.hover : Theme.raised)
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
