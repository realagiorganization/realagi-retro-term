/*******************************************************************************
* Copyright (c) 2013-2021 "Filippo Scognamiglio"
* https://github.com/realagiorganization/realagi-retro-term
*
* This file is part of realagi-retro-term.
*
* realagi-retro-term is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Dialogs

import "menus"

ApplicationWindow {
    id: terminalWindow

    width: 1024
    height: 768

    // Show the window once it is ready.
    Component.onCompleted: {
        visible = true
    }

    minimumWidth: 320
    minimumHeight: 240

    visible: false

    property bool fullscreen: false
    property bool showMusicPanel: false
    onFullscreenChanged: visibility = (fullscreen ? Window.FullScreen : Window.Windowed)

    menuBar: WindowMenu { }

    property real normalizedWindowScale: 1024 / ((0.5 * width + 0.5 * height))

    color: "#00000000"

    title: terminalTabs.currentTitle

    Action {
        id: fullscreenAction
        text: qsTr("Fullscreen")
        enabled: !appSettings.isMacOS
        shortcut: StandardKey.FullScreen
        onTriggered: fullscreen = !fullscreen
        checkable: true
        checked: fullscreen
    }
    Action {
        id: newWindowAction
        text: qsTr("New Window")
        shortcut: appSettings.isMacOS ? "Meta+N" : "Ctrl+Shift+N"
        onTriggered: appRoot.createWindow()
    }
    Action {
        id: quitAction
        text: qsTr("Quit")
        shortcut: appSettings.isMacOS ? StandardKey.Close : "Ctrl+Shift+Q"
        onTriggered: terminalWindow.close()
    }
    Action {
        id: showsettingsAction
        text: qsTr("Settings")
        onTriggered: {
            settingsWindow.show()
            settingsWindow.requestActivate()
            settingsWindow.raise()
        }
    }
    Action {
        id: copyAction
        text: qsTr("Copy")
        shortcut: appSettings.isMacOS ? StandardKey.Copy : "Ctrl+Shift+C"
    }
    Action {
        id: pasteAction
        text: qsTr("Paste")
        shortcut: appSettings.isMacOS ? StandardKey.Paste : "Ctrl+Shift+V"
    }
    Action {
        id: zoomIn
        text: qsTr("Zoom In")
        shortcut: StandardKey.ZoomIn
        onTriggered: appSettings.incrementScaling()
    }
    Action {
        id: zoomOut
        text: qsTr("Zoom Out")
        shortcut: StandardKey.ZoomOut
        onTriggered: appSettings.decrementScaling()
    }
    Action {
        id: showAboutAction
        text: qsTr("About")
        onTriggered: {
            aboutDialog.show()
            aboutDialog.requestActivate()
            aboutDialog.raise()
        }
    }
    Action {
        id: newTabAction
        text: qsTr("New Tab")
        shortcut: appSettings.isMacOS ? "Meta+T" : "Ctrl+Shift+T"
        onTriggered: terminalTabs.addTab()
    }
    Action {
        id: closeTabAction
        text: qsTr("Close Tab")
        shortcut: appSettings.isMacOS ? "Meta+W" : "Ctrl+Shift+W"
        onTriggered: terminalTabs.closeTab(terminalTabs.currentIndex)
    }
    Action {
        id: openAudioAction
        text: qsTr("Open Audio")
        shortcut: appSettings.isMacOS ? "Meta+Alt+O" : "Ctrl+Alt+O"
        onTriggered: {
            showMusicPanel = true
            musicPlayerPanel.openFileDialog()
        }
    }
    Action {
        id: togglePlaybackAction
        text: appRoot.musicPlayer.isPlaying ? qsTr("Pause Audio") : qsTr("Play Audio")
        shortcut: appSettings.isMacOS ? "Meta+Alt+P" : "Ctrl+Alt+P"
        onTriggered: appRoot.musicPlayer.togglePlayback()
    }
    Action {
        id: stopPlaybackAction
        text: qsTr("Stop Audio")
        shortcut: appSettings.isMacOS ? "Meta+Alt+S" : "Ctrl+Alt+S"
        onTriggered: appRoot.musicPlayer.stopPlayback()
    }
    Action {
        id: toggleMusicPanelAction
        text: showMusicPanel ? qsTr("Hide Music Player") : qsTr("Show Music Player")
        shortcut: appSettings.isMacOS ? "Meta+Alt+M" : "Ctrl+Alt+M"
        onTriggered: showMusicPanel = !showMusicPanel
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+1" : "Alt+1"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 0) terminalTabs.currentIndex = 0
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+2" : "Alt+2"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 1) terminalTabs.currentIndex = 1
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+3" : "Alt+3"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 2) terminalTabs.currentIndex = 2
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+4" : "Alt+4"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 3) terminalTabs.currentIndex = 3
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+5" : "Alt+5"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 4) terminalTabs.currentIndex = 4
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+6" : "Alt+6"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 5) terminalTabs.currentIndex = 5
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+7" : "Alt+7"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 6) terminalTabs.currentIndex = 6
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+8" : "Alt+8"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 7) terminalTabs.currentIndex = 7
    }
    Shortcut {
        sequence: appSettings.isMacOS ? "Meta+9" : "Alt+9"
        context: Qt.WindowShortcut
        onActivated: if (terminalTabs.count > 8) terminalTabs.currentIndex = 8
    }
    TerminalTabs {
        id: terminalTabs
        width: parent.width
        height: (parent.height + Math.abs(y))
    }
    Loader {
        anchors.centerIn: parent
        active: appSettings.showTerminalSize
        sourceComponent: SizeOverlay {
            z: 3
            terminalSize: terminalTabs.terminalSize
        }
    }
    MusicPlayerPanel {
        id: musicPlayerPanel
        z: 4
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 18
        visible: showMusicPanel
        player: appRoot.musicPlayer
        onDismissed: showMusicPanel = false
    }
    onClosing: {
        appRoot.closeWindow(terminalWindow)
    }
}
