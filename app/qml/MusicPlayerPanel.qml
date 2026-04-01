import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: panel

    property QtObject player: null
    signal dismissed()

    width: Math.min(360, parent ? parent.width - 36 : 360)
    padding: 12

    background: Rectangle {
        radius: 10
        color: "#de000000"
        border.width: 1
        border.color: appSettings.fontColor
    }

    function openFileDialog() {
        audioFileDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Music Visualizer")
                color: appSettings.fontColor
                font.bold: true
                Layout.fillWidth: true
            }

            ToolButton {
                text: "\u00d7"
                onClicked: panel.dismissed()
            }
        }

        Label {
            Layout.fillWidth: true
            text: player ? player.trackName : qsTr("No audio loaded")
            color: appSettings.fontColor
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: player
                  ? player.statusText + "  " + player.formatDuration(player.positionSeconds) + " / " + player.formatDuration(player.durationSeconds)
                  : qsTr("Idle")
            color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.72)
            elide: Text.ElideRight
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: player && player.durationSeconds > 0 ? player.durationSeconds : 1
            value: player ? player.positionSeconds : 0
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Open")
                onClicked: panel.openFileDialog()
            }

            Button {
                enabled: player && player.hasSource
                text: player && player.isPlaying ? qsTr("Pause") : qsTr("Play")
                onClicked: if (player) player.togglePlayback()
            }

            Button {
                enabled: player && player.hasSource
                text: qsTr("Stop")
                onClicked: if (player) player.stopPlayback()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Volume")
                color: appSettings.fontColor
            }

            Slider {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: player ? player.volume : 0
                onMoved: if (player) player.setVolume(value)
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("MP3 playback now shares waveform-driven visuals with a FluidSynth MIDI render path. FM backends still need a dedicated engine.")
            color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.6)
            visible: player !== null
        }
    }

    FileDialog {
        id: audioFileDialog
        title: qsTr("Open Audio File")
        nameFilters: [
            qsTr("Audio files (*.mp3 *.wav *.ogg *.flac *.m4a *.mid *.midi)"),
            qsTr("All files (*)")
        ]
        onAccepted: if (player) player.openSource(selectedFile)
    }
}
