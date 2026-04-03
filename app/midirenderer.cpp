#include "midirenderer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

MidiRenderer::MidiRenderer(QObject *parent)
    : QObject(parent)
{
#ifndef Q_OS_IOS
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_errorBytes += m_process.readAllStandardError();
    });

    connect(&m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        setRendering(false);

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QFile::remove(m_pendingOutputPath);
            const QString renderError = QString::fromUtf8(m_errorBytes).trimmed();
            setErrorString(renderError.isEmpty()
                           ? tr("FluidSynth could not render the selected MIDI file.")
                           : renderError);
            setReady(false);
            return;
        }

        const QFileInfo outputInfo(m_pendingOutputPath);
        if (!outputInfo.exists() || outputInfo.size() <= 0) {
            setErrorString(tr("FluidSynth finished without producing rendered audio."));
            setReady(false);
            return;
        }

        setOutputUrl(QUrl::fromLocalFile(outputInfo.absoluteFilePath()));
        setErrorString(QString());
        setReady(true);
    });

    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError processError) {
        if (processError == QProcess::Crashed && !m_rendering) {
            return;
        }

        QFile::remove(m_pendingOutputPath);
        setRendering(false);
        setReady(false);
        setErrorString(tr("Unable to start FluidSynth for MIDI rendering."));
    });
#endif
}

bool MidiRenderer::rendering() const
{
    return m_rendering;
}

bool MidiRenderer::ready() const
{
    return m_ready;
}

QString MidiRenderer::errorString() const
{
    return m_errorString;
}

QString MidiRenderer::source() const
{
    return m_source;
}

QUrl MidiRenderer::outputUrl() const
{
    return m_outputUrl;
}

QString MidiRenderer::soundFontPath() const
{
    return m_soundFontPath;
}

bool MidiRenderer::isMidiFile(const QUrl &sourceUrl) const
{
    const QFileInfo sourceInfo(resolveSourcePath(sourceUrl));
    const QString extension = sourceInfo.suffix().toLower();
    return extension == QStringLiteral("mid") || extension == QStringLiteral("midi");
}

void MidiRenderer::render(const QUrl &sourceUrl)
{
    const QString sourcePath = resolveSourcePath(sourceUrl);
    if (sourcePath.isEmpty()) {
        reset();
        setErrorString(tr("Select a local MIDI file first."));
        return;
    }

    if (!isMidiFile(sourceUrl)) {
        reset();
        setErrorString(tr("The selected file is not a MIDI file."));
        return;
    }

#ifdef Q_OS_IOS
    stopRunningProcess();
    m_errorBytes.clear();
    setSource(sourcePath);
    setSoundFontPath(QString());
    setOutputUrl(QUrl());
    setReady(false);
    setRendering(false);
    setErrorString(tr("MIDI rendering is unavailable on iOS builds."));
    return;
#else
    const QString detectedSoundFont = detectSoundFont();
    if (QStandardPaths::findExecutable(QStringLiteral("fluidsynth")).isEmpty()
        || detectedSoundFont.isEmpty()) {
        reset();
        setErrorString(tr("FluidSynth or a GM soundfont is not available on this system."));
        return;
    }

    stopRunningProcess();
    m_errorBytes.clear();
    setRendering(false);
    setReady(false);
    setErrorString(QString());
    setOutputUrl(QUrl());
    setSource(sourcePath);
    setSoundFontPath(detectedSoundFont);

    m_pendingOutputPath = ensureOutputPath(sourcePath);
    const QFileInfo outputInfo(m_pendingOutputPath);
    if (outputInfo.exists() && outputInfo.size() > 0) {
        setOutputUrl(QUrl::fromLocalFile(outputInfo.absoluteFilePath()));
        setReady(true);
        return;
    }

    QFile::remove(m_pendingOutputPath);

    QStringList arguments;
    arguments << "-q"
              << "-n"
              << "-i"
              << "-F" << m_pendingOutputPath
              << "-T" << "wav"
              << "-r" << "44100"
              << detectedSoundFont
              << sourcePath;

    setRendering(true);
    m_process.start(QStringLiteral("fluidsynth"), arguments);
    if (!m_process.waitForStarted(1000)) {
        setRendering(false);
        setErrorString(tr("Unable to start FluidSynth for MIDI rendering."));
    }
#endif
}

void MidiRenderer::cancel()
{
    stopRunningProcess();
    m_errorBytes.clear();
    QFile::remove(m_pendingOutputPath);
    setRendering(false);
    setReady(false);
    setErrorString(QString());
    setOutputUrl(QUrl());
}

void MidiRenderer::reset()
{
    cancel();
    setSource(QString());
    setSoundFontPath(QString());
}

QString MidiRenderer::detectSoundFont() const
{
    const QString configuredPath = qEnvironmentVariable("REALAGI_RETRO_TERM_SOUNDFONT");
    if (!configuredPath.isEmpty() && QFileInfo::exists(configuredPath)) {
        return configuredPath;
    }

    const QStringList candidatePaths = {
        QStringLiteral("/usr/share/sounds/sf2/default-GM.sf2"),
        QStringLiteral("/usr/share/sounds/sf2/FluidR3_GM.sf2"),
        QStringLiteral("/usr/share/sounds/sf2/TimGM6mb.sf2"),
        QStringLiteral("/usr/share/sounds/sf3/default-GM.sf3")
    };

    for (const QString &candidatePath : candidatePaths) {
        if (QFileInfo::exists(candidatePath)) {
            return candidatePath;
        }
    }

    return QString();
}

QString MidiRenderer::ensureOutputPath(const QString &sourcePath) const
{
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::tempPath() + QStringLiteral("/realagi-retro-term");
    }

    QDir cacheDir(cacheRoot);
    cacheDir.mkpath(QStringLiteral("midi-renders"));
    cacheDir.cd(QStringLiteral("midi-renders"));

    const QFileInfo sourceInfo(sourcePath);
    const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
            + '|'
            + QByteArray::number(sourceInfo.size())
            + '|'
            + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch())
            + '|'
            + m_soundFontPath.toUtf8();
    const QString hashedName = QString::fromLatin1(
                QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex().left(16));
    return cacheDir.absoluteFilePath(hashedName + QStringLiteral(".wav"));
}

QString MidiRenderer::resolveSourcePath(const QUrl &sourceUrl) const
{
    if (!sourceUrl.isValid()) {
        return QString();
    }

    return sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : sourceUrl.toString();
}

void MidiRenderer::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    emit errorStringChanged();
}

void MidiRenderer::setOutputUrl(const QUrl &outputUrl)
{
    if (m_outputUrl == outputUrl) {
        return;
    }

    m_outputUrl = outputUrl;
    emit outputUrlChanged();
}

void MidiRenderer::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }

    m_ready = ready;
    emit readyChanged();
}

void MidiRenderer::setRendering(bool rendering)
{
    if (m_rendering == rendering) {
        return;
    }

    m_rendering = rendering;
    emit renderingChanged();
}

void MidiRenderer::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;
    emit sourceChanged();
}

void MidiRenderer::setSoundFontPath(const QString &soundFontPath)
{
    if (m_soundFontPath == soundFontPath) {
        return;
    }

    m_soundFontPath = soundFontPath;
    emit soundFontPathChanged();
}

void MidiRenderer::stopRunningProcess()
{
#ifndef Q_OS_IOS
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }

    m_process.kill();
    m_process.waitForFinished(1000);
#endif
}
