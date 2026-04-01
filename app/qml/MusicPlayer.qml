import QtQuick
import RealagiRetroTerm 1.0

QtObject {
    id: musicPlayer

    property var timeDriver: null
    property QtObject backend: null
    property string backendError: ""
    property url selectedSource: ""
    readonly property bool backendAvailable: backend !== null
    readonly property bool hasSource: backend ? backend.hasSource : false
    readonly property bool isPlaying: backend ? backend.isPlaying : false
    readonly property bool isPaused: backend ? backend.isPaused : false
    readonly property real positionSeconds: backend ? backend.positionSeconds : 0
    readonly property real durationSeconds: backend ? backend.durationSeconds : 0
    readonly property real volume: backend ? backend.volume : 0.7
    readonly property string trackName: backend ? backend.trackName : displayName(selectedSource)
    readonly property string statusText: currentStatusText()
    readonly property real visualizerLevel: analysis.ready ? analysis.levelAt(positionSeconds) : (backend ? backend.visualizerLevel : 0)
    readonly property real visualizerPulse: analysis.ready ? analysis.pulseAt(positionSeconds) : (backend ? backend.visualizerPulse : 0)
    readonly property real visualizerSweep: analysis.ready ? analysis.sweepAt(positionSeconds) : (backend ? backend.visualizerSweep : 0)

    function displayName(sourceUrl) {
        var rawUrl = sourceUrl ? sourceUrl.toString() : ""
        if (!rawUrl) {
            return qsTr("No audio loaded")
        }

        var normalized = rawUrl
        if (normalized.indexOf("file://") === 0) {
            normalized = normalized.substring(7)
        }

        var segments = normalized.split("/")
        var lastSegment = segments.length > 0 ? segments[segments.length - 1] : normalized
        return decodeURIComponent(lastSegment || normalized)
    }

    function initializeBackend() {
        var component = Qt.createComponent("MusicBackendQtMultimedia.qml")
        if (component.status === Component.Ready) {
            backend = component.createObject(musicPlayer, { "timeDriver": timeDriver })
            backendError = backend ? "" : qsTr("Could not create multimedia backend.")
            return
        }

        backendError = qsTr("QtMultimedia playback backend is unavailable on this system.")
    }

    function openSource(selectedFile) {
        if (!selectedFile) {
            return
        }

        selectedSource = selectedFile
        analysis.analyze(selectedFile)

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

    function currentStatusText() {
        if (!backendAvailable) {
            return backendError
        }

        var baseStatus = backend.statusText
        if (analysis.analyzing) {
            return baseStatus + " • " + qsTr("analyzing waveform")
        }
        if (analysis.ready) {
            return baseStatus + " • " + qsTr("waveform ready")
        }
        if (analysis.errorString) {
            return baseStatus + " • " + qsTr("visualizer fallback")
        }
        return baseStatus
    }

    AudioAnalysis {
        id: analysis
    }

    Component.onCompleted: initializeBackend()
}
