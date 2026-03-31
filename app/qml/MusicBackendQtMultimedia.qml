import QtQuick
import QtMultimedia

QtObject {
    id: backend

    property var timeDriver: null
    readonly property bool hasSource: mediaPlayer.source.toString() !== ""
    readonly property bool isPlaying: mediaPlayer.playbackState === MediaPlayer.PlayingState
    readonly property bool isPaused: mediaPlayer.playbackState === MediaPlayer.PausedState
    readonly property real positionSeconds: mediaPlayer.position / 1000.0
    readonly property real durationSeconds: mediaPlayer.duration / 1000.0
    readonly property real volume: audioOutput.volume
    readonly property real elapsedTime: timeDriver ? timeDriver.time : positionSeconds
    readonly property string trackName: displayName(mediaPlayer.source)
    readonly property string statusText: currentStatusText()
    readonly property real visualizerLevel: computedVisualizerLevel()
    readonly property real visualizerPulse: computedVisualizerPulse()
    readonly property real visualizerSweep: computedVisualizerSweep()

    function clamp01(value) {
        return Math.max(0, Math.min(1, value))
    }

    function twoDigits(value) {
        return (value < 10 ? "0" : "") + value
    }

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

    function currentStatusText() {
        if (mediaPlayer.errorString) {
            return mediaPlayer.errorString
        }
        if (isPlaying) {
            return qsTr("Playing")
        }
        if (isPaused) {
            return qsTr("Paused")
        }
        if (hasSource) {
            return qsTr("Ready")
        }
        return qsTr("Idle")
    }

    function computedVisualizerLevel() {
        if (!isPlaying) {
            return 0
        }

        var t = positionSeconds
        var phase = 0.35 + volume * 0.65
        var signal = 0.40
                + 0.20 * Math.sin(t * 1.73)
                + 0.16 * Math.sin(t * 3.47 + 0.31)
                + 0.10 * Math.sin(t * 7.91 + 1.37)
                + 0.06 * Math.sin(elapsedTime * 12.2)
        return clamp01(signal * phase)
    }

    function computedVisualizerPulse() {
        if (!isPlaying) {
            return 0
        }

        var t = positionSeconds
        return clamp01(0.5 + 0.5 * Math.sin(t * 8.0 + Math.sin(t * 0.9) * 2.0))
    }

    function computedVisualizerSweep() {
        if (!isPlaying) {
            return 0
        }

        var t = positionSeconds
        return clamp01(0.5 + 0.5 * Math.sin(t * 0.45))
    }

    function openSource(selectedFile) {
        if (!selectedFile) {
            return
        }

        mediaPlayer.source = selectedFile
        mediaPlayer.play()
    }

    function togglePlayback() {
        if (!hasSource) {
            return
        }

        if (isPlaying) {
            mediaPlayer.pause()
        } else {
            mediaPlayer.play()
        }
    }

    function stopPlayback() {
        mediaPlayer.stop()
    }

    function setVolume(value) {
        audioOutput.volume = clamp01(value)
    }

    function formatDuration(seconds) {
        if (!isFinite(seconds) || seconds < 0) {
            return "00:00"
        }

        var totalSeconds = Math.floor(seconds)
        var minutes = Math.floor(totalSeconds / 60)
        var remainder = totalSeconds % 60
        return twoDigits(minutes) + ":" + twoDigits(remainder)
    }

    AudioOutput {
        id: audioOutput
        volume: 0.7
    }

    MediaPlayer {
        id: mediaPlayer
        audioOutput: audioOutput
    }
}
