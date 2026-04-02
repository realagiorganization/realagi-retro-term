import QtQuick
import RealagiRetroTerm 1.0

QtObject {
    id: musicPlayer

    property var timeDriver: null
    property QtObject backend: null
    property string backendError: ""
    property url selectedSource: ""
    readonly property bool selectedSourceIsMidi: midiRenderer.isMidiFile(selectedSource)
    readonly property bool selectedSourceIsYmfm: ymfmRenderer.isYmfmFile(selectedSource)
    readonly property bool backendAvailable: backend !== null
    readonly property bool hasSource: backend ? backend.hasSource : false
    readonly property bool isPlaying: backend ? backend.isPlaying : false
    readonly property bool isPaused: backend ? backend.isPaused : false
    readonly property real positionSeconds: backend ? backend.positionSeconds : 0
    readonly property real durationSeconds: backend ? backend.durationSeconds : 0
    readonly property real volume: backend ? backend.volume : 0.7
    readonly property string trackName: selectedSource.toString() !== ""
                                         ? displayName(selectedSource)
                                         : (backend ? backend.trackName : displayName(selectedSource))
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

    function normalizedLocalPath(sourceUrl) {
        if (!sourceUrl) {
            return ""
        }

        var rawUrl = sourceUrl.toString()
        if (!rawUrl) {
            return ""
        }

        return rawUrl.indexOf("file://") === 0 ? rawUrl.substring(7) : rawUrl
    }

    function openPlaybackSource(playbackSource) {
        if (!backend || !playbackSource) {
            return
        }

        analysis.analyze(playbackSource)
        backend.openSource(playbackSource)
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
        analysis.reset()

        if (selectedSourceIsMidi) {
            if (backend && backend.clearSource) {
                backend.clearSource()
            }
            ymfmRenderer.reset()
            midiRenderer.render(selectedFile)
            return
        }

        midiRenderer.reset()

        if (selectedSourceIsYmfm) {
            if (backend && backend.clearSource) {
                backend.clearSource()
            }
            ymfmRenderer.render(selectedFile)
            return
        }

        ymfmRenderer.reset()

        if (backend) {
            openPlaybackSource(selectedFile)
        }
    }

    function togglePlayback() {
        if (backend) {
            backend.togglePlayback()
        }
    }

    function stopPlayback() {
        if (midiRenderer.rendering) {
            midiRenderer.cancel()
        }
        if (ymfmRenderer.rendering) {
            ymfmRenderer.cancel()
        }
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

        if (selectedSourceIsMidi) {
            if (midiRenderer.rendering) {
                return qsTr("Rendering MIDI with FluidSynth")
            }
            if (midiRenderer.errorString) {
                return midiRenderer.errorString
            }
        }

        if (selectedSourceIsYmfm) {
            if (ymfmRenderer.rendering) {
                return qsTr("Rendering YMFM track")
            }
            if (ymfmRenderer.errorString) {
                return ymfmRenderer.errorString
            }
        }

        var baseStatus = backend.statusText
        if (selectedSourceIsMidi && midiRenderer.ready) {
            baseStatus += " • " + qsTr("rendered with FluidSynth")
        }
        if (selectedSourceIsYmfm && ymfmRenderer.ready) {
            baseStatus += " • " + qsTr("rendered with ymfm")
            if (ymfmRenderer.systemName) {
                baseStatus += " • " + ymfmRenderer.systemName
            }
        }
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

    property AudioAnalysis analysis: AudioAnalysis {
        id: analysis
    }

    property MidiRenderer midiRenderer: MidiRenderer {
        id: midiRenderer

        onReadyChanged: {
            if (!ready || !musicPlayer.selectedSourceIsMidi) {
                return
            }

            if (source !== musicPlayer.normalizedLocalPath(musicPlayer.selectedSource)) {
                return
            }

            musicPlayer.openPlaybackSource(outputUrl)
        }
    }

    property YmfmRenderer ymfmRenderer: YmfmRenderer {
        id: ymfmRenderer

        onReadyChanged: {
            if (!ready || !musicPlayer.selectedSourceIsYmfm) {
                return
            }

            if (source !== musicPlayer.normalizedLocalPath(musicPlayer.selectedSource)) {
                return
            }

            musicPlayer.openPlaybackSource(outputUrl)
        }
    }

    Component.onCompleted: initializeBackend()
}
