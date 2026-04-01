#include "gamemusicrenderer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <gme/gme.h>

#include <array>

namespace {
constexpr int kChunkSampleCount = 16384;
constexpr int kChannelCount = 2;
constexpr int kBitsPerSample = 16;
constexpr int kBytesPerSample = kBitsPerSample / 8;
constexpr int kDefaultPlayLengthMsecs = 150000;
constexpr int kDefaultFadeLengthMsecs = 8000;

bool cancellationRequested(const std::shared_ptr<std::atomic_bool> &cancelRequested)
{
    return cancelRequested && cancelRequested->load();
}

void writeLe16(QByteArray &target, quint16 value)
{
    target.append(static_cast<char>(value & 0xff));
    target.append(static_cast<char>((value >> 8) & 0xff));
}

void writeLe32(QByteArray &target, quint32 value)
{
    target.append(static_cast<char>(value & 0xff));
    target.append(static_cast<char>((value >> 8) & 0xff));
    target.append(static_cast<char>((value >> 16) & 0xff));
    target.append(static_cast<char>((value >> 24) & 0xff));
}

QByteArray waveHeader(qsizetype dataSize, int sampleRate)
{
    const quint16 blockAlign = kChannelCount * kBytesPerSample;
    const quint32 byteRate = static_cast<quint32>(sampleRate * blockAlign);
    const quint32 payloadSize = static_cast<quint32>(dataSize);

    QByteArray header;
    header.reserve(44);
    header.append("RIFF", 4);
    writeLe32(header, 36u + payloadSize);
    header.append("WAVE", 4);
    header.append("fmt ", 4);
    writeLe32(header, 16u);
    writeLe16(header, 1u);
    writeLe16(header, static_cast<quint16>(kChannelCount));
    writeLe32(header, static_cast<quint32>(sampleRate));
    writeLe32(header, byteRate);
    writeLe16(header, blockAlign);
    writeLe16(header, static_cast<quint16>(kBitsPerSample));
    header.append("data", 4);
    writeLe32(header, payloadSize);
    return header;
}

bool writeWaveFile(const QString &outputPath, const QByteArray &pcmBytes, int sampleRate, QString *errorString)
{
    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        if (errorString) {
            *errorString = QObject::tr("Could not create the FM render cache directory.");
        }
        return false;
    }

    QSaveFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        if (errorString) {
            *errorString = QObject::tr("Could not open the FM render cache file for writing.");
        }
        return false;
    }

    const QByteArray header = waveHeader(pcmBytes.size(), sampleRate);
    if (outputFile.write(header) != header.size() || outputFile.write(pcmBytes) != pcmBytes.size()) {
        if (errorString) {
            *errorString = QObject::tr("Could not write the rendered FM audio cache file.");
        }
        return false;
    }

    if (!outputFile.commit()) {
        if (errorString) {
            *errorString = QObject::tr("Could not finalize the rendered FM audio cache file.");
        }
        return false;
    }

    return true;
}

QString formatNameForType(gme_type_t type)
{
    if (type == gme_gym_type) {
        return QStringLiteral("GYM");
    }
    if (type == gme_vgm_type || type == gme_vgz_type) {
        return QStringLiteral("VGM");
    }
    return QString();
}

QString systemNameForType(gme_type_t type)
{
    const char *systemName = type ? gme_type_system(type) : nullptr;
    return systemName && systemName[0] ? QString::fromUtf8(systemName) : QString();
}

bool supportedType(gme_type_t type)
{
    return type == gme_gym_type || type == gme_vgm_type || type == gme_vgz_type;
}
} // namespace

GameMusicRenderer::GameMusicRenderer(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<GameMusicRenderResult>::finished, this, [this]() {
        const GameMusicRenderResult result = m_watcher.result();
        setRendering(false);
        m_cancelRequested.reset();

        if (result.serial != m_renderSerial || result.cancelled) {
            return;
        }

        if (!result.errorString.isEmpty()) {
            QFile::remove(m_pendingOutputPath);
            setOutputUrl(QUrl());
            setReady(false);
            setErrorString(result.errorString);
            return;
        }

        setFormatName(result.formatName);
        setSystemName(result.systemName);
        setOutputUrl(QUrl::fromLocalFile(result.outputPath));
        setErrorString(QString());
        setReady(true);
        m_pendingOutputPath.clear();
    });
}

GameMusicRenderer::~GameMusicRenderer()
{
    stopRunningRender();
}

bool GameMusicRenderer::rendering() const
{
    return m_rendering;
}

bool GameMusicRenderer::ready() const
{
    return m_ready;
}

QString GameMusicRenderer::errorString() const
{
    return m_errorString;
}

QString GameMusicRenderer::source() const
{
    return m_source;
}

QUrl GameMusicRenderer::outputUrl() const
{
    return m_outputUrl;
}

QString GameMusicRenderer::formatName() const
{
    return m_formatName;
}

QString GameMusicRenderer::systemName() const
{
    return m_systemName;
}

bool GameMusicRenderer::isGameMusicFile(const QUrl &sourceUrl) const
{
    return !formatNameForPath(resolveSourcePath(sourceUrl)).isEmpty();
}

void GameMusicRenderer::render(const QUrl &sourceUrl)
{
    const QString sourcePath = resolveSourcePath(sourceUrl);
    if (sourcePath.isEmpty()) {
        reset();
        setErrorString(tr("Select a local Sega FM track first."));
        return;
    }

    const QString detectedFormat = formatNameForPath(sourcePath);
    if (detectedFormat.isEmpty()) {
        reset();
        setErrorString(tr("The selected file is not a supported GYM/VGM/VGZ track."));
        return;
    }

    stopRunningRender();

    setRendering(false);
    setReady(false);
    setErrorString(QString());
    setOutputUrl(QUrl());
    setSource(sourcePath);
    setFormatName(detectedFormat);
    setSystemName(systemNameForPath(sourcePath));

    m_pendingOutputPath = ensureOutputPath(sourcePath, detectedFormat);
    const QFileInfo outputInfo(m_pendingOutputPath);
    if (outputInfo.exists() && outputInfo.size() > 44) {
        setOutputUrl(QUrl::fromLocalFile(outputInfo.absoluteFilePath()));
        setReady(true);
        m_pendingOutputPath.clear();
        return;
    }

    QFile::remove(m_pendingOutputPath);

    const quint64 serial = ++m_renderSerial;
    const QString outputPath = m_pendingOutputPath;
    const QString systemName = m_systemName;
    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    setRendering(true);
    m_watcher.setFuture(QtConcurrent::run([serial, sourcePath, outputPath, detectedFormat, systemName, cancelRequested = m_cancelRequested]() {
        return renderToWave(serial, sourcePath, outputPath, detectedFormat, systemName, cancelRequested);
    }));
}

void GameMusicRenderer::cancel()
{
    const bool hadRunningRender = m_watcher.isRunning();
    stopRunningRender();
    if (hadRunningRender && !m_pendingOutputPath.isEmpty()) {
        QFile::remove(m_pendingOutputPath);
    }
    setRendering(false);
    setReady(false);
    setErrorString(QString());
    setOutputUrl(QUrl());
}

void GameMusicRenderer::reset()
{
    cancel();
    m_pendingOutputPath.clear();
    setSource(QString());
    setFormatName(QString());
    setSystemName(QString());
}

GameMusicRenderResult GameMusicRenderer::renderToWave(quint64 serial,
                                                      const QString &sourcePath,
                                                      const QString &outputPath,
                                                      const QString &formatName,
                                                      const QString &systemName,
                                                      std::shared_ptr<std::atomic_bool> cancelRequested)
{
    GameMusicRenderResult result;
    result.serial = serial;
    result.sourcePath = sourcePath;
    result.outputPath = outputPath;
    result.formatName = formatName;
    result.systemName = systemName;

    Music_Emu *emu = nullptr;
    gme_info_t *trackInfo = nullptr;

    const QByteArray sourceBytes = sourcePath.toUtf8();
    const gme_err_t openError = gme_open_file(sourceBytes.constData(), &emu, kSampleRate);
    if (openError != nullptr) {
        result.errorString = QString::fromUtf8(openError);
        return result;
    }

    int maxRenderMsecs = kDefaultPlayLengthMsecs + kDefaultFadeLengthMsecs;
    if (gme_track_info(emu, &trackInfo, 0) == nullptr && trackInfo != nullptr) {
        if (trackInfo->system != nullptr && trackInfo->system[0] != '\0') {
            result.systemName = QString::fromUtf8(trackInfo->system);
        }

        const int playLengthMsecs = trackInfo->play_length > 0 ? trackInfo->play_length : kDefaultPlayLengthMsecs;
        const int fadeLengthMsecs = trackInfo->fade_length > 0 ? trackInfo->fade_length : kDefaultFadeLengthMsecs;
        maxRenderMsecs = playLengthMsecs + fadeLengthMsecs;
        if (trackInfo->play_length > 0) {
            gme_set_fade_msecs(emu, trackInfo->play_length, fadeLengthMsecs);
        }
    }

    gme_set_autoload_playback_limit(emu, 1);

    const gme_err_t startError = gme_start_track(emu, 0);
    if (startError != nullptr) {
        if (trackInfo != nullptr) {
            gme_free_info(trackInfo);
        }
        gme_delete(emu);
        result.errorString = QString::fromUtf8(startError);
        return result;
    }

    QByteArray pcmBytes;
    pcmBytes.reserve((maxRenderMsecs * kSampleRate / 1000) * kChannelCount * kBytesPerSample);
    std::array<short, kChunkSampleCount> sampleBuffer = {};

    while (!gme_track_ended(emu) && gme_tell(emu) < maxRenderMsecs) {
        if (cancellationRequested(cancelRequested)) {
            result.cancelled = true;
            break;
        }

        const gme_err_t playError = gme_play(emu, static_cast<int>(sampleBuffer.size()), sampleBuffer.data());
        if (playError != nullptr) {
            result.errorString = QString::fromUtf8(playError);
            break;
        }

        pcmBytes.append(reinterpret_cast<const char *>(sampleBuffer.data()), static_cast<int>(sampleBuffer.size() * sizeof(short)));
    }

    if (trackInfo != nullptr) {
        gme_free_info(trackInfo);
    }
    gme_delete(emu);

    if (result.cancelled || !result.errorString.isEmpty()) {
        return result;
    }

    if (pcmBytes.isEmpty()) {
        result.errorString = QObject::tr("Game_Music_Emu produced no audio for the selected FM track.");
        return result;
    }

    QString writeError;
    if (!writeWaveFile(outputPath, pcmBytes, kSampleRate, &writeError)) {
        result.errorString = writeError;
    }

    return result;
}

QString GameMusicRenderer::ensureOutputPath(const QString &sourcePath, const QString &formatName) const
{
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::tempPath() + QStringLiteral("/realagi-retro-term");
    }

    QDir cacheDir(cacheRoot);
    cacheDir.mkpath(QStringLiteral("game-music-renders"));
    cacheDir.cd(QStringLiteral("game-music-renders"));

    const QFileInfo sourceInfo(sourcePath);
    const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
            + '|'
            + QByteArray::number(sourceInfo.size())
            + '|'
            + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch())
            + '|'
            + formatName.toUtf8();
    const QString hashedName = QString::fromLatin1(
                QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex().left(16));
    return cacheDir.absoluteFilePath(hashedName + QStringLiteral(".wav"));
}

QString GameMusicRenderer::formatNameForPath(const QString &sourcePath) const
{
    if (sourcePath.isEmpty()) {
        return QString();
    }

    const QByteArray sourceBytes = sourcePath.toUtf8();
    const gme_type_t type = gme_identify_extension(sourceBytes.constData());
    return supportedType(type) ? formatNameForType(type) : QString();
}

QString GameMusicRenderer::resolveSourcePath(const QUrl &sourceUrl) const
{
    if (!sourceUrl.isValid()) {
        return QString();
    }

    return sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : sourceUrl.toString();
}

QString GameMusicRenderer::systemNameForPath(const QString &sourcePath) const
{
    if (sourcePath.isEmpty()) {
        return QString();
    }

    const QByteArray sourceBytes = sourcePath.toUtf8();
    const gme_type_t type = gme_identify_extension(sourceBytes.constData());
    return supportedType(type) ? systemNameForType(type) : QString();
}

void GameMusicRenderer::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    emit errorStringChanged();
}

void GameMusicRenderer::setFormatName(const QString &formatName)
{
    if (m_formatName == formatName) {
        return;
    }

    m_formatName = formatName;
    emit formatNameChanged();
}

void GameMusicRenderer::setOutputUrl(const QUrl &outputUrl)
{
    if (m_outputUrl == outputUrl) {
        return;
    }

    m_outputUrl = outputUrl;
    emit outputUrlChanged();
}

void GameMusicRenderer::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }

    m_ready = ready;
    emit readyChanged();
}

void GameMusicRenderer::setRendering(bool rendering)
{
    if (m_rendering == rendering) {
        return;
    }

    m_rendering = rendering;
    emit renderingChanged();
}

void GameMusicRenderer::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;
    emit sourceChanged();
}

void GameMusicRenderer::setSystemName(const QString &systemName)
{
    if (m_systemName == systemName) {
        return;
    }

    m_systemName = systemName;
    emit systemNameChanged();
}

void GameMusicRenderer::stopRunningRender()
{
    if (!m_watcher.isRunning()) {
        return;
    }

    if (m_cancelRequested) {
        m_cancelRequested->store(true);
    }

    m_watcher.waitForFinished();
    m_cancelRequested.reset();
}
