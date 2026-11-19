// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import AkaiExplorer
import AkaiExplorer.Native

// Round icon-only button with a tooltip that names the action.
Button {
    id: control
    property string iconName
    property string tip
    property bool active: false       // toggled-on state
    property color iconColor: active ? Theme.accent : (hovered ? Theme.text : Theme.muted)
    property int iconSize: 18

    implicitWidth: 34
    implicitHeight: 34
    padding: 0
    opacity: enabled ? 1 : 0.35
    activeFocusOnTab: true
    Accessible.name: tip

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            width: control.iconSize; height: control.iconSize
            name: control.iconName
            color: control.iconColor
        }
    }
    background: Rectangle {
        radius: width / 2
        color: control.down ? Theme.line : (control.hovered ? Theme.hover : "transparent")
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
    ToolTip.visible: hovered && tip
    ToolTip.delay: 600
    ToolTip.text: tip
}
