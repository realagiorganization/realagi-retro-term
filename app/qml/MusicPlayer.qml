import QtQuick

QtObject {
    id: musicPlayer

    property var timeDriver: null
    property QtObject backend: null
    property string backendError: ""
    readonly property bool backendAvailable: backend !== null
    readonly property bool hasSource: backend ? backend.hasSource : false
    readonly property bool isPlaying: backend ? backend.isPlaying : false
    readonly property bool isPaused: backend ? backend.isPaused : false
    readonly property real positionSeconds: backend ? backend.positionSeconds : 0
    readonly property real durationSeconds: backend ? backend.durationSeconds : 0
    readonly property real volume: backend ? backend.volume : 0.7
    readonly property string trackName: backend ? backend.trackName : qsTr("No audio loaded")
    readonly property string statusText: backend ? backend.statusText : backendError
    readonly property real visualizerLevel: backend ? backend.visualizerLevel : 0
    readonly property real visualizerPulse: backend ? backend.visualizerPulse : 0
    readonly property real visualizerSweep: backend ? backend.visualizerSweep : 0

    function initializeBackend() {
        var component = Qt.createComponent("MusicBackendQtMultimedia.qml")
        if (component.status === Component.Ready) {
            backend = component.createObject(musicPlayer, { "timeDriver": timeDriver })
            backendError = backend ? "" : qsTr("Could not create multimedia backend.")
            return
        }

        backendError = component.errorString()
    }

    function openSource(selectedFile) {
        if (backend) {
            backend.openSource(selectedFile)
        }
    }

    function togglePlayback() {
        if (backend) {
            backend.togglePlayback()
        }
    }

    function stopPlayback() {
        if (backend) {
            backend.stopPlayback()
        }
    }

    function setVolume(value) {
        if (backend) {
            backend.setVolume(value)
        }
    }

    function formatDuration(seconds) {
        if (backend) {
            return backend.formatDuration(seconds)
        }
        return "00:00"
    }

    Component.onCompleted: initializeBackend()
}
