import QtQuick

QtObject {
    id: appRoot

    property ApplicationSettings appSettings: ApplicationSettings { }

    property TimeManager timeManager: TimeManager {
        enableTimer: iosWindow.visible
    }

    property MusicPlayer musicPlayer: MusicPlayer {
        timeDriver: timeManager
    }

    property IosWindow iosWindow: IosWindow {
        visible: false
        player: appRoot.musicPlayer
    }

    Connections {
        target: appSettings

        function onInitializedSettings() {
            iosWindow.show()
            iosWindow.requestActivate()
        }
    }
}
