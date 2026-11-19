// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import AkaiExplorer
import AkaiExplorer.Native

// Text button. primary: filled with the accent colour; otherwise outlined.
Button {
    id: control
    property bool primary: false
    property string iconName: ""

    implicitHeight: 36
    leftPadding: iconName ? 14 : 18
    rightPadding: 18
    font.family: Theme.family
    font.pixelSize: Theme.body
    font.weight: Font.DemiBold
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: true

    contentItem: Row {
        spacing: 8
        Icon {
            visible: control.iconName
            name: control.iconName
            width: 18; height: 18
            anchors.verticalCenter: parent.verticalCenter
            color: control.primary ? Theme.accentInk : Theme.text
        }
        Text {
            text: control.text
            font: control.font
            color: control.primary ? Theme.accentInk : Theme.text
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    background: Rectangle {
        radius: height / 2
        color: control.primary ? (control.down ? Qt.darker(Theme.accent, 1.15) : control.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
                               : (control.down ? Theme.line : control.hovered ? Theme.hover : "transparent")
        border.width: control.primary ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.line
        Rectangle { // keyboard focus ring for the filled variant
            visible: control.primary && control.visualFocus
            anchors.fill: parent; anchors.margins: -3
            radius: height / 2; color: "transparent"
            border.width: 2; border.color: Theme.text
        }
    }
}
