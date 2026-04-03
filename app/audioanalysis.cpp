#include "audioanalysis.h"

#include <QtMath>

#include <algorithm>
#include <cstring>

AudioAnalysis::AudioAnalysis(QObject *parent)
    : QObject(parent)
{
#ifndef Q_OS_IOS
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_pcmBytes += m_process.readAllStandardOutput();
    });

    connect(&m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        setAnalyzing(false);

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            const QString ffmpegError = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
            setErrorString(ffmpegError.isEmpty()
                           ? tr("ffmpeg could not analyze the selected track.")
                           : ffmpegError);
            setReady(false);
            return;
        }

        finalizeAnalysis();
    });

    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError processError) {
        if (processError == QProcess::Crashed && !m_analyzing) {
            return;
        }

        setAnalyzing(false);
        setReady(false);
        setErrorString(tr("Unable to start ffmpeg for waveform analysis."));
    });
#endif
}

bool AudioAnalysis::analyzing() const
{
    return m_analyzing;
}

bool AudioAnalysis::ready() const
{
    return m_ready;
}

QString AudioAnalysis::errorString() const
{
    return m_errorString;
}

QString AudioAnalysis::source() const
{
    return m_source;
}

int AudioAnalysis::sampleRate() const
{
    return kSampleRate;
}

void AudioAnalysis::analyze(const QUrl &sourceUrl)
{
    const QString localPath = sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : sourceUrl.toString();
    if (localPath.isEmpty()) {
        return;
    }

    stopRunningProcess();
    clearAnalysis();

    if (m_source != localPath) {
        m_source = localPath;
        emit sourceChanged();
    }

#ifdef Q_OS_IOS
    setErrorString(tr("Waveform analysis is unavailable on iOS builds."));
    setReady(false);
    setAnalyzing(false);
    return;
#else
    setErrorString(QString());
    setReady(false);
    setAnalyzing(true);

    QStringList arguments;
    arguments << "-v" << "error"
              << "-nostdin"
              << "-i" << localPath
              << "-vn"
              << "-ac" << "1"
              << "-ar" << QString::number(kSampleRate)
              << "-f" << "f32le"
              << "pipe:1";

    m_process.start(QStringLiteral("ffmpeg"), arguments);
    if (!m_process.waitForStarted(1000)) {
        setAnalyzing(false);
        setErrorString(tr("Unable to start ffmpeg for waveform analysis."));
    }
#endif
}

void AudioAnalysis::reset()
{
    stopRunningProcess();
    clearAnalysis();
    setAnalyzing(false);
    setReady(false);
    setErrorString(QString());

    if (!m_source.isEmpty()) {
        m_source.clear();
        emit sourceChanged();
    }
}

qreal AudioAnalysis::levelAt(qreal seconds) const
{
    const int index = indexForSeconds(seconds);
    return index >= 0 ? m_levels[index] : 0.0;
}

qreal AudioAnalysis::pulseAt(qreal seconds) const
{
    const int index = indexForSeconds(seconds);
    return index >= 0 ? m_pulses[index] : 0.0;
}

qreal AudioAnalysis::sweepAt(qreal seconds) const
{
    const int index = indexForSeconds(seconds);
    return index >= 0 ? m_sweeps[index] : 0.0;
}

int AudioAnalysis::indexForSeconds(qreal seconds) const
{
    if (!m_ready || m_levels.isEmpty() || seconds < 0) {
        return -1;
    }

    const int index = qFloor(seconds * kSampleRate);
    return qBound(0, index, static_cast<int>(m_levels.size()) - 1);
}

void AudioAnalysis::clearAnalysis()
{
    m_pcmBytes.clear();
    m_levels.clear();
    m_pulses.clear();
    m_sweeps.clear();
}

void AudioAnalysis::finalizeAnalysis()
{
    const int floatCount = m_pcmBytes.size() / static_cast<int>(sizeof(float));
    if (floatCount <= 0) {
        setErrorString(tr("ffmpeg produced no waveform data for the selected track."));
        setReady(false);
        return;
    }

    QVector<qreal> envelope;
    envelope.reserve(floatCount);

    qreal smoothed = 0.0;
    qreal peak = 0.0;

    const char *raw = m_pcmBytes.constData();
    for (int index = 0; index < floatCount; ++index) {
        float sample = 0.0f;
        std::memcpy(&sample, raw + index * static_cast<int>(sizeof(float)), sizeof(float));

        const qreal magnitude = qAbs(static_cast<qreal>(sample));
        smoothed = smoothed * 0.78 + magnitude * 0.22;
        envelope.push_back(smoothed);
        peak = qMax(peak, smoothed);
    }

    if (peak < 0.0001) {
        peak = 1.0;
    }

    m_levels.resize(envelope.size());
    m_pulses.resize(envelope.size());
    m_sweeps.resize(envelope.size());

    QVector<qreal> normalized;
    normalized.reserve(envelope.size());
    for (qreal value : std::as_const(envelope)) {
        normalized.push_back(qBound<qreal>(0.0, value / peak, 1.0));
    }

    QVector<qreal> prefix(normalized.size() + 1, 0.0);
    for (int index = 0; index < normalized.size(); ++index) {
        prefix[index + 1] = prefix[index] + normalized[index];
    }

    qreal pulsePeak = 0.0001;
    for (int index = 0; index < normalized.size(); ++index) {
        const qreal previous = index > 0 ? normalized[index - 1] : normalized[index];
        const qreal delta = qMax<qreal>(0.0, normalized[index] - previous * 0.90);
        m_levels[index] = normalized[index];
        m_pulses[index] = delta;
        pulsePeak = qMax(pulsePeak, delta);
    }

    for (qreal &pulse : m_pulses) {
        pulse = qBound<qreal>(0.0, pulse / pulsePeak, 1.0);
    }

    const int sweepWindow = qMax(2, kSampleRate / 3);
    for (int index = 0; index < normalized.size(); ++index) {
        const int begin = qMax(0, index - sweepWindow);
        const int end = qMin(normalized.size(), index + sweepWindow);
        const qreal average = (prefix[end] - prefix[begin]) / qMax(1, end - begin);
        m_sweeps[index] = qBound<qreal>(0.0, average, 1.0);
    }

    setErrorString(QString());
    setReady(true);
}

void AudioAnalysis::setAnalyzing(bool analyzingValue)
{
    if (m_analyzing == analyzingValue) {
        return;
    }

    m_analyzing = analyzingValue;
    emit analyzingChanged();
}

void AudioAnalysis::setReady(bool readyValue)
{
    if (m_ready == readyValue) {
        return;
    }

    m_ready = readyValue;
    emit readyChanged();
}

void AudioAnalysis::setErrorString(const QString &errorStringValue)
{
    if (m_errorString == errorStringValue) {
        return;
    }

    m_errorString = errorStringValue;
    emit errorStringChanged();
}

void AudioAnalysis::stopRunningProcess()
{
#ifndef Q_OS_IOS
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }

    m_process.kill();
    m_process.waitForFinished(1000);
#endif
}
