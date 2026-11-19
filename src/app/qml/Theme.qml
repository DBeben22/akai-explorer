// SPDX-License-Identifier: GPL-2.0-or-later
pragma Singleton
import QtQuick

// Design tokens. The palette borrows from the hardware this app is about:
// the dark chassis of a rack sampler, the blue of its backlit LCD and the
// amber of its LEDs (used only for the playhead).
QtObject {
    readonly property color base: "#12151C"       // main background
    readonly property color sidebar: "#0C0E13"    // library column
    readonly property color raised: "#181C25"     // player bar, popups
    readonly property color hover: "#1B202B"
    readonly property color line: "#262C3A"       // hairlines
    readonly property color text: "#E8EBF2"
    readonly property color muted: "#8D94A6"
    readonly property color faint: "#5A6173"
    readonly property color accent: "#58B4FF"     // LCD blue
    readonly property color accentInk: "#06131F"  // text on accent
    readonly property color accentTint: "#1B2C40" // selected row
    readonly property color wave: "#3A4254"       // unplayed waveform
    readonly property color playhead: "#FFB454"   // LED amber
    readonly property color danger: "#FF7A70"

    // first installed family wins; every Linux desktop has at least one of them
    readonly property string family: {
        var wanted = ["Inter", "Ubuntu", "Cantarell", "Noto Sans", "DejaVu Sans"]
        var have = Qt.fontFamilies()
        for (var i = 0; i < wanted.length; ++i)
            if (have.indexOf(wanted[i]) >= 0)
                return wanted[i]
        return Qt.application.font.family
    }

    readonly property int small: 12
    readonly property int body: 14
    readonly property int title: 17
    readonly property int display: 34

    readonly property int radius: 8
    readonly property int rowHeight: 44
}
