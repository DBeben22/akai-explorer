// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AkaiExplorer
import AkaiExplorer.Native

// Title block of the selected volume with the export actions and the list filter.
Item {
    id: root
    signal exportRequested(string what)   // "selected" | "volume" | "everything"
    property alias searchField: search
    implicitHeight: column.implicitHeight + 20

    ColumnLayout {
        id: column
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 28; anchors.rightMargin: 28
        y: 22
        spacing: 0

        Text {
            Layout.fillWidth: true
            text: app.volumeDetail
            elide: Text.ElideRight
            color: Theme.muted
            font.family: Theme.family
            font.pixelSize: Theme.small + 1
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 2
            text: app.volumeName
            elide: Text.ElideRight
            color: Theme.text
            font.family: Theme.family
            font.pixelSize: Theme.display
            font.weight: Font.Bold
            font.letterSpacing: -0.6
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 2
            text: {
                var parts = []
                parts.push(app.files.totalCount === 1 ? qsTr("1 file") : qsTr("%1 files").arg(app.files.totalCount))
                if (app.volumeLength)
                    parts.push(app.volumeLength)
                return parts.join(", ")
            }
            color: Theme.muted
            font.family: Theme.family
            font.pixelSize: Theme.body
        }

        RowLayout {
            Layout.topMargin: 18
            Layout.fillWidth: true
            spacing: 10

            PillButton {
                primary: true
                iconName: "export"
                enabled: !app.exporting && app.files.totalCount > 0
                text: app.files.selectedCount > 1 ? qsTr("Export %1 selected").arg(app.files.selectedCount)
                                                  : app.files.selectedCount === 1 ? qsTr("Export selected")
                                                                                  : qsTr("Export volume")
                onClicked: root.exportRequested(app.files.selectedCount > 0 ? "selected" : "volume")
            }
            PillButton {
                id: optionsButton
                text: qsTr("Export options")
                onClicked: options.opened ? options.close() : options.open()

                Popup {
                    id: options
                    y: parent.height + 8
                    width: 340
                    padding: 16
                    background: Rectangle { color: Theme.raised; radius: Theme.radius; border.width: 1; border.color: Theme.line }
                    contentItem: ColumnLayout {
                        spacing: 14
                        OptionSwitch {
                            title: qsTr("Join stereo pairs")
                            hint: qsTr("Samples named -L and -R become one stereo WAV, and are previewed together.")
                            checked: app.mergeStereo
                            onToggled: app.mergeStereo = checked
                        }
                        OptionSwitch {
                            title: qsTr("Also export programs and other files")
                            hint: qsTr("Files without audio are saved unconverted, as akaiutil's \"get\" does.")
                            checked: app.includeRaw
                            onToggled: app.includeRaw = checked
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                        PillButton {
                            Layout.fillWidth: true
                            enabled: !app.exporting
                            text: qsTr("Export every open image")
                            onClicked: { options.close(); root.exportRequested("everything") }
                        }
                    }
                }
            }
            Item { Layout.fillWidth: true }

            Chip { text: qsTr("All"); count: app.files.totalCount; selected: app.files.kindFilter === "all"; onClicked: app.files.kindFilter = "all" }
            Chip { text: qsTr("Audio"); count: app.files.audioCount; selected: app.files.kindFilter === "audio"; onClicked: app.files.kindFilter = "audio" }
            Chip { visible: app.files.otherCount > 0; text: qsTr("Other"); count: app.files.otherCount; selected: app.files.kindFilter === "other"; onClicked: app.files.kindFilter = "other" }

            TextField {
                id: search
                Layout.preferredWidth: 200
                implicitHeight: 34
                leftPadding: 34
                rightPadding: 12
                placeholderText: qsTr("Find in volume")
                placeholderTextColor: Theme.faint
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: Theme.accentInk
                font.family: Theme.family
                font.pixelSize: Theme.body
                verticalAlignment: TextInput.AlignVCenter
                onTextChanged: app.files.filterText = text
                Keys.onEscapePressed: { text = ""; focus = false }
                Connections { // the model clears its filter when another volume is opened
                    target: app.files
                    function onFilterChanged() { if (app.files.filterText !== search.text) search.text = app.files.filterText }
                }
                background: Rectangle {
                    radius: height / 2
                    color: Theme.raised
                    border.width: search.activeFocus ? 2 : 0
                    border.color: Theme.accent
                    Icon { x: 11; anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; name: "search"; color: Theme.muted }
                }
            }
        }
    }

    component OptionSwitch: RowLayout {
        id: opt
        property string title
        property string hint
        property bool checked
        signal toggled()
        Layout.fillWidth: true
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text { Layout.fillWidth: true; text: opt.title; color: Theme.text; font.family: Theme.family; font.pixelSize: Theme.body; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
            Text { Layout.fillWidth: true; text: opt.hint; color: Theme.muted; font.family: Theme.family; font.pixelSize: Theme.small; wrapMode: Text.Wrap; lineHeight: 1.25 }
        }
        Rectangle {
            Layout.alignment: Qt.AlignTop
            width: 40; height: 22; radius: 11
            color: opt.checked ? Theme.accent : Theme.line
            activeFocusOnTab: true
            border.width: activeFocus ? 2 : 0
            border.color: Theme.text
            Accessible.role: Accessible.CheckBox
            Accessible.name: opt.title
            Keys.onSpacePressed: { opt.checked = !opt.checked; opt.toggled() }
            Rectangle {
                width: 16; height: 16; radius: 8; y: 3
                x: opt.checked ? 21 : 3
                color: opt.checked ? Theme.accentInk : Theme.text
                Behavior on x { NumberAnimation { duration: 110 } }
            }
            MouseArea { anchors.fill: parent; onClicked: { opt.checked = !opt.checked; opt.toggled() } }
        }
    }
}
