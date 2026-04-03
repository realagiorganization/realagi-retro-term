import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: iosWindow

    property QtObject player: null
    property bool showMusicPanel: true

    width: 1024
    height: 768
    title: qsTr("realagi-retro-term")
    color: "#050505"

    Component.onCompleted: visible = true

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#090909" }
            GradientStop { position: 1.0; color: "#020202" }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true

        ColumnLayout {
            width: Math.max(320, iosWindow.width - 40)
            spacing: 18

            Rectangle {
                Layout.fillWidth: true
                radius: 14
                color: "#15000000"
                border.width: 1
                border.color: appSettings.fontColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("realagi-retro-term")
                        color: appSettings.fontColor
                        font.pixelSize: 34
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("iOS preview build")
                        color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.72)
                        font.pixelSize: 18
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("The CRT shell and music stack are available here, but the embedded terminal is disabled on iOS until the PTY backend is replaced with an iOS-safe alternative.")
                        color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.88)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 14
                color: "#11000000"
                border.width: 1
                border.color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.42)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Port Status")
                        color: appSettings.fontColor
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("This build keeps the shared QML settings, shaders, audio analysis, Qt Multimedia playback, MIDI renderer, and YMFM renderer alive. The desktop-only terminal path is intentionally fenced off for now.")
                        color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.82)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            text: qsTr("Open Audio")
                            onClicked: {
                                iosWindow.showMusicPanel = true
                                musicPanel.openFileDialog()
                            }
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
                }
            }

            MusicPlayerPanel {
                id: musicPanel
                Layout.alignment: Qt.AlignHCenter
                player: iosWindow.player
                visible: iosWindow.showMusicPanel

                onDismissed: iosWindow.showMusicPanel = false
            }

            Label {
                Layout.fillWidth: true
                visible: !iosWindow.showMusicPanel
                wrapMode: Text.WordWrap
                text: qsTr("The music panel is hidden. Use Open Audio to bring it back.")
                color: Qt.rgba(appSettings.fontColor.r, appSettings.fontColor.g, appSettings.fontColor.b, 0.64)
            }
        }
    }

    onClosing: appSettings.close()
}
