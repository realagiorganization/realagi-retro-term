#include "ymfmrenderer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <array>
#include <vector>

#include "em_inflate.h"
#include "ymfm_opn.h"
#include "../third_party/ymfm/examples/vgmrender/vgmrender.h"

namespace {
using EmulatedTime = qint64;
constexpr EmulatedTime kTimeScale = 0x100000000ll;
constexpr int kOutputSampleRate = 44100;
constexpr int kYm2612Clock = 7670454;
constexpr int kGymTickRate = 60;
constexpr int kWaveChannels = 2;
constexpr int kWaveBitsPerSample = 16;
constexpr int kWaveBytesPerSample = kWaveBitsPerSample / 8;
constexpr int kWaveScale = 26000;
constexpr int kGymFrameSamples = kOutputSampleRate / kGymTickRate;
constexpr int kGymHeaderSize = 0x1AC;

struct GymFileData
{
    QByteArray bytes;
    quint32 dataOffset = 0;
    quint32 loopFrame = 0;
};

struct GymScanInfo
{
    quint32 totalFrames = 0;
    quint32 loopOffset = 0;
};

class GymYm2612Interface : public ymfm::ymfm_interface
{
};

class GymYm2612Synth final : public GymYm2612Interface
{
public:
    GymYm2612Synth()
        : m_chip(*this),
          m_step(kTimeScale / m_chip.sample_rate(kYm2612Clock))
    {
        m_chip.reset();
    }

    void writeRegister(quint8 port, quint8 reg, quint8 value)
    {
        m_chip.write((port << 1) | 0, reg);
        m_chip.write((port << 1) | 1, value);
    }

    void renderSample(EmulatedTime outputStart, int32_t *left, int32_t *right)
    {
        for (; m_pos <= outputStart; m_pos += m_step) {
            m_chip.generate(&m_output);
        }

        *left += m_output.data[0];
        *right += m_output.data[1 % ymfm::ym2612::OUTPUTS];
    }

private:
    ymfm::ym2612 m_chip;
    EmulatedTime m_step = 1;
    EmulatedTime m_pos = 0;
    ymfm::ym2612::output_data m_output = {};
};

bool cancellationRequested(const std::shared_ptr<std::atomic_bool> &cancelRequested)
{
    return cancelRequested && cancelRequested->load();
}

QString renderErrorForCode(int errorCode)
{
    switch (errorCode) {
    case 1:
        return QObject::tr("Internal YMFM render arguments were invalid.");
    case 2:
        return QObject::tr("Could not open the selected VGM/VGZ file.");
    case 3:
        return QObject::tr("Could not read the selected VGM/VGZ file.");
    case 4:
        return QObject::tr("The selected file is not a valid VGM/VGZ track.");
    case 5:
        return QObject::tr("No supported Yamaha FM chips were found in the selected track.");
    case 6:
    case 7:
        return QObject::tr("YMFM could not write the rendered FM audio cache file.");
    default:
        return QObject::tr("YMFM could not render the selected FM track.");
    }
}

quint32 readLe32(const char *data)
{
    return static_cast<quint32>(static_cast<unsigned char>(data[0]))
            | (static_cast<quint32>(static_cast<unsigned char>(data[1])) << 8)
            | (static_cast<quint32>(static_cast<unsigned char>(data[2])) << 16)
            | (static_cast<quint32>(static_cast<unsigned char>(data[3])) << 24);
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
    const quint16 blockAlign = kWaveChannels * kWaveBytesPerSample;
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
    writeLe16(header, static_cast<quint16>(kWaveChannels));
    writeLe32(header, static_cast<quint32>(sampleRate));
    writeLe32(header, byteRate);
    writeLe16(header, blockAlign);
    writeLe16(header, static_cast<quint16>(kWaveBitsPerSample));
    header.append("data", 4);
    writeLe32(header, payloadSize);
    return header;
}

bool writeWaveFile(const QString &outputPath,
                   const std::vector<int32_t> &waveData,
                   int sampleRate,
                   QString *errorString)
{
    if (waveData.empty()) {
        if (errorString) {
            *errorString = QObject::tr("YMFM produced no audio samples.");
        }
        return false;
    }

    qint64 maxScale = 0;
    for (int32_t sample : waveData) {
        maxScale = std::max(maxScale, std::llabs(static_cast<long long>(sample)));
    }
    if (maxScale <= 0) {
        maxScale = 1;
    }

    QByteArray payload;
    payload.reserve(static_cast<int>(waveData.size() * kWaveBytesPerSample));
    for (int32_t sample : waveData) {
        const qint64 scaled = static_cast<qint64>(sample) * kWaveScale / maxScale;
        writeLe16(payload, static_cast<quint16>(static_cast<qint16>(scaled)));
    }

    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        if (errorString) {
            *errorString = QObject::tr("Could not create the YMFM render cache directory.");
        }
        return false;
    }

    QSaveFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        if (errorString) {
            *errorString = QObject::tr("Could not open the YMFM render cache file for writing.");
        }
        return false;
    }

    const QByteArray header = waveHeader(payload.size(), sampleRate);
    if (outputFile.write(header) != header.size() || outputFile.write(payload) != payload.size()) {
        if (errorString) {
            *errorString = QObject::tr("Could not write the YMFM render cache file.");
        }
        return false;
    }

    if (!outputFile.commit()) {
        if (errorString) {
            *errorString = QObject::tr("Could not finalize the YMFM render cache file.");
        }
        return false;
    }

    return true;
}

bool looksLikeRawGym(const QByteArray &data)
{
    qsizetype filePos = 0;
    while (filePos < data.size() && data[filePos] == '\0') {
        ++filePos;
    }
    if (filePos >= data.size()) {
        return false;
    }

    bool expectPsgDataByte = false;
    while (filePos < data.size()) {
        const quint8 command = static_cast<quint8>(data[filePos++]);
        switch (command) {
        case 0x00:
            expectPsgDataByte = false;
            break;
        case 0x01:
        case 0x02: {
            if (filePos + 2 > data.size()) {
                return false;
            }
            const quint8 reg = static_cast<quint8>(data[filePos]);
            if (reg >= 0xB8) {
                return false;
            }
            if (command == 0x01 && reg < 0x21) {
                return false;
            }
            if (command == 0x02 && reg < 0x30) {
                return false;
            }
            filePos += 2;
            break;
        }
        case 0x03: {
            if (filePos >= data.size()) {
                return false;
            }
            const quint8 value = static_cast<quint8>(data[filePos]);
            if (value & 0x80) {
                expectPsgDataByte = ((value & 0x10) == 0x00 && value < 0xE0);
            } else if (expectPsgDataByte && value < 0x40) {
                expectPsgDataByte = false;
            } else {
                return false;
            }
            ++filePos;
            break;
        }
        default:
            return false;
        }
    }

    return true;
}

bool loadGymFile(const QString &sourcePath, GymFileData *gymFile, QString *errorString)
{
    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = QObject::tr("Could not open the selected GYM file.");
        }
        return false;
    }

    QByteArray data = sourceFile.readAll();
    if (data.isEmpty()) {
        if (errorString) {
            *errorString = QObject::tr("The selected GYM file is empty.");
        }
        return false;
    }

    const bool hasHeader = data.size() >= kGymHeaderSize && std::memcmp(data.constData(), "GYMX", 4) == 0;
    quint32 dataOffset = 0;
    quint32 loopFrame = 0;

    if (hasHeader) {
        dataOffset = kGymHeaderSize;
        loopFrame = readLe32(data.constData() + 0x1A4);
        const quint32 uncompressedSize = readLe32(data.constData() + 0x1A8);
        if (uncompressedSize > 0) {
            if (data.size() <= dataOffset) {
                if (errorString) {
                    *errorString = QObject::tr("The selected GYMX file is missing compressed track data.");
                }
                return false;
            }

            QByteArray decompressed;
            decompressed.resize(static_cast<int>(uncompressedSize));
            const size_t result = em_inflate(data.constData() + dataOffset,
                                             static_cast<size_t>(data.size() - dataOffset),
                                             reinterpret_cast<unsigned char *>(decompressed.data()),
                                             static_cast<size_t>(decompressed.size()));
            if (result == static_cast<size_t>(-1)) {
                if (errorString) {
                    *errorString = QObject::tr("Could not decompress the selected GYMX track.");
                }
                return false;
            }
            decompressed.resize(static_cast<int>(result));
            data = data.left(dataOffset) + decompressed;
        }
    } else if (!looksLikeRawGym(data)) {
        if (errorString) {
            *errorString = QObject::tr("The selected file is not a supported GYM track.");
        }
        return false;
    }

    if (data.size() <= static_cast<int>(dataOffset)) {
        if (errorString) {
            *errorString = QObject::tr("The selected GYM track contains no command data.");
        }
        return false;
    }

    gymFile->bytes = data;
    gymFile->dataOffset = dataOffset;
    gymFile->loopFrame = loopFrame;
    return true;
}

bool scanGymTrack(const GymFileData &gymFile, GymScanInfo *scanInfo, QString *errorString)
{
    quint32 totalFrames = 0;
    quint32 loopOffset = 0;
    qsizetype filePos = gymFile.dataOffset;

    while (filePos < gymFile.bytes.size()) {
        if (totalFrames == gymFile.loopFrame && gymFile.loopFrame != 0) {
            loopOffset = static_cast<quint32>(filePos);
        }

        const quint8 command = static_cast<quint8>(gymFile.bytes[filePos++]);
        switch (command) {
        case 0x00:
            ++totalFrames;
            break;
        case 0x01:
        case 0x02:
            if (filePos + 2 > gymFile.bytes.size()) {
                if (errorString) {
                    *errorString = QObject::tr("The selected GYM track ends mid-command.");
                }
                return false;
            }
            filePos += 2;
            break;
        case 0x03:
            if (filePos + 1 > gymFile.bytes.size()) {
                if (errorString) {
                    *errorString = QObject::tr("The selected GYM track ends mid-command.");
                }
                return false;
            }
            filePos += 1;
            break;
        default:
            if (errorString) {
                *errorString = QObject::tr("The selected GYM track contains an unsupported command 0x%1.")
                        .arg(command, 2, 16, QLatin1Char('0'));
            }
            return false;
        }
    }

    scanInfo->totalFrames = totalFrames;
    scanInfo->loopOffset = loopOffset;
    return true;
}

void writeYm2612Register(GymYm2612Synth &chip,
                         quint8 port,
                         quint8 reg,
                         quint8 value,
                         std::array<quint8, 0x20> &freqCache,
                         std::array<quint8, 2> &latchCache,
                         const QByteArray &trackData,
                         qsizetype nextFilePos)
{
    const quint8 commandByte = static_cast<quint8>(0x01 + port);
    const auto writeRegister = [&chip, port](quint8 registerByte, quint8 registerValue) {
        chip.writeRegister(port, registerByte, registerValue);
    };

    if ((reg & 0xF0) != 0xA0) {
        writeRegister(reg, value);
        return;
    }

    const quint8 cacheReg = static_cast<quint8>((port << 4) | (reg & 0x0F));
    const bool isLatchWrite = (reg & 0x04) != 0;
    const quint8 latchId = static_cast<quint8>((reg & 0x08) >> 3);
    freqCache[cacheReg] = value;

    if (isLatchWrite) {
        bool needPatch = true;
        if (nextFilePos + 1 < trackData.size()
            && static_cast<quint8>(trackData[nextFilePos]) == commandByte
            && static_cast<quint8>(trackData[nextFilePos + 1]) == static_cast<quint8>(reg ^ 0x04)) {
            needPatch = false;
        }

        writeRegister(reg, value);
        latchCache[latchId] = value;
        if (needPatch) {
            writeRegister(static_cast<quint8>(reg ^ 0x04), freqCache[cacheReg ^ 0x04]);
        }
        return;
    }

    if (latchCache[latchId] != freqCache[cacheReg ^ 0x04]) {
        writeRegister(static_cast<quint8>(reg ^ 0x04), freqCache[cacheReg ^ 0x04]);
        latchCache[latchId] = freqCache[cacheReg ^ 0x04];
    }
    writeRegister(reg, value);
}

void renderGymFrame(GymYm2612Synth &chip,
                    const std::vector<quint8> &pcmWrites,
                    std::vector<int32_t> *waveData,
                    EmulatedTime *outputPos,
                    const std::shared_ptr<std::atomic_bool> &cancelRequested)
{
    constexpr EmulatedTime outputStep = kTimeScale / kOutputSampleRate;

    int lastPcmIndex = -1;
    for (int sampleIndex = 0; sampleIndex < kGymFrameSamples; ++sampleIndex) {
        if (cancellationRequested(cancelRequested)) {
            return;
        }

        if (!pcmWrites.empty()) {
            const int pcmIndex = sampleIndex * static_cast<int>(pcmWrites.size()) / kGymFrameSamples;
            if (pcmIndex != lastPcmIndex && pcmIndex >= 0 && pcmIndex < static_cast<int>(pcmWrites.size())) {
                chip.writeRegister(0, 0x2A, pcmWrites[pcmIndex]);
                lastPcmIndex = pcmIndex;
            }
        }

        int32_t left = 0;
        int32_t right = 0;
        chip.renderSample(*outputPos, &left, &right);
        *outputPos += outputStep;
        waveData->push_back(left);
        waveData->push_back(right);
    }
}

YmfmRenderResult renderGymToWave(quint64 serial,
                                 const QString &sourcePath,
                                 const QString &outputPath,
                                 const QString &formatName,
                                 const QString &systemName,
                                 const std::shared_ptr<std::atomic_bool> &cancelRequested)
{
    YmfmRenderResult result;
    result.serial = serial;
    result.sourcePath = sourcePath;
    result.outputPath = outputPath;
    result.formatName = formatName;
    result.systemName = systemName;

    GymFileData gymFile;
    if (!loadGymFile(sourcePath, &gymFile, &result.errorString)) {
        return result;
    }

    GymScanInfo scanInfo;
    if (!scanGymTrack(gymFile, &scanInfo, &result.errorString)) {
        return result;
    }

    GymYm2612Synth chip;

    const quint32 loopFrames = (scanInfo.loopOffset != 0 && scanInfo.totalFrames > gymFile.loopFrame)
            ? (scanInfo.totalFrames - gymFile.loopFrame)
            : 0;

    std::vector<int32_t> waveData;
    waveData.reserve(static_cast<size_t>((scanInfo.totalFrames + loopFrames) * kGymFrameSamples * kWaveChannels));

    std::array<quint8, 0x20> freqCache = {};
    std::array<quint8, 2> latchCache = {};
    std::vector<quint8> pcmWrites;
    pcmWrites.reserve(kOutputSampleRate / 30);

    bool sawPsgWrites = false;
    int loopsRendered = 0;
    EmulatedTime outputPos = 0;
    qsizetype filePos = gymFile.dataOffset;

    while (filePos < gymFile.bytes.size()) {
        if (cancellationRequested(cancelRequested)) {
            result.cancelled = true;
            return result;
        }

        const quint8 command = static_cast<quint8>(gymFile.bytes[filePos++]);
        switch (command) {
        case 0x00:
            renderGymFrame(chip, pcmWrites, &waveData, &outputPos, cancelRequested);
            pcmWrites.clear();
            break;
        case 0x01:
        case 0x02: {
            if (filePos + 2 > gymFile.bytes.size()) {
                result.errorString = QObject::tr("The selected GYM track ends mid-command.");
                return result;
            }

            const quint8 port = static_cast<quint8>(command - 0x01);
            const quint8 reg = static_cast<quint8>(gymFile.bytes[filePos]);
            const quint8 value = static_cast<quint8>(gymFile.bytes[filePos + 1]);
            filePos += 2;

            if (port == 0 && reg == 0x2A) {
                pcmWrites.push_back(value);
            } else {
                writeYm2612Register(chip, port, reg, value, freqCache, latchCache, gymFile.bytes, filePos);
            }
            break;
        }
        case 0x03:
            if (filePos + 1 > gymFile.bytes.size()) {
                result.errorString = QObject::tr("The selected GYM track ends mid-command.");
                return result;
            }
            ++filePos;
            sawPsgWrites = true;
            break;
        default:
            result.errorString = QObject::tr("The selected GYM track contains an unsupported command 0x%1.")
                    .arg(command, 2, 16, QLatin1Char('0'));
            return result;
        }

        if (filePos >= gymFile.bytes.size() && scanInfo.loopOffset != 0 && loopsRendered < 1) {
            ++loopsRendered;
            filePos = scanInfo.loopOffset;
        }
    }

    if (cancellationRequested(cancelRequested)) {
        result.cancelled = true;
        return result;
    }

    if (waveData.empty()) {
        result.errorString = QObject::tr("YMFM produced no audio for the selected GYM track.");
        return result;
    }

    if (sawPsgWrites) {
        result.systemName = QObject::tr("Sega Mega Drive / Genesis (PSG muted)");
    } else {
        result.systemName = QObject::tr("Sega Mega Drive / Genesis");
    }

    QString writeError;
    if (!writeWaveFile(outputPath, waveData, kOutputSampleRate, &writeError)) {
        result.errorString = writeError;
    }

    return result;
}
} // namespace

YmfmRenderer::YmfmRenderer(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<YmfmRenderResult>::finished, this, [this]() {
        const YmfmRenderResult result = m_watcher.result();
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

YmfmRenderer::~YmfmRenderer()
{
    stopRunningRender();
}

bool YmfmRenderer::rendering() const
{
    return m_rendering;
}

bool YmfmRenderer::ready() const
{
    return m_ready;
}

QString YmfmRenderer::errorString() const
{
    return m_errorString;
}

QString YmfmRenderer::source() const
{
    return m_source;
}

QUrl YmfmRenderer::outputUrl() const
{
    return m_outputUrl;
}

QString YmfmRenderer::formatName() const
{
    return m_formatName;
}

QString YmfmRenderer::systemName() const
{
    return m_systemName;
}

bool YmfmRenderer::isYmfmFile(const QUrl &sourceUrl) const
{
    return !formatNameForPath(resolveSourcePath(sourceUrl)).isEmpty();
}

void YmfmRenderer::render(const QUrl &sourceUrl)
{
    const QString sourcePath = resolveSourcePath(sourceUrl);
    if (sourcePath.isEmpty()) {
        reset();
        setErrorString(tr("Select a local VGM/VGZ/GYM track first."));
        return;
    }

    const QString detectedFormat = formatNameForPath(sourcePath);
    if (detectedFormat.isEmpty()) {
        reset();
        setErrorString(tr("The selected file is not a supported VGM/VGZ/GYM track."));
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
    const QString initialSystemName = m_systemName;
    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    setRendering(true);
    m_watcher.setFuture(QtConcurrent::run([serial, sourcePath, outputPath, detectedFormat, initialSystemName, cancelRequested = m_cancelRequested]() {
        return renderToWave(serial, sourcePath, outputPath, detectedFormat, initialSystemName, cancelRequested);
    }));
}

void YmfmRenderer::cancel()
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

void YmfmRenderer::reset()
{
    cancel();
    m_pendingOutputPath.clear();
    setSource(QString());
    setFormatName(QString());
    setSystemName(QString());
}

YmfmRenderResult YmfmRenderer::renderToWave(quint64 serial,
                                            const QString &sourcePath,
                                            const QString &outputPath,
                                            const QString &formatName,
                                            const QString &systemName,
                                            std::shared_ptr<std::atomic_bool> cancelRequested)
{
    if (formatName == QStringLiteral("GYM")) {
        return renderGymToWave(serial, sourcePath, outputPath, formatName, systemName, cancelRequested);
    }

    YmfmRenderResult result;
    result.serial = serial;
    result.sourcePath = sourcePath;
    result.outputPath = outputPath;
    result.formatName = formatName;
    result.systemName = systemName;

    if (cancellationRequested(cancelRequested)) {
        result.cancelled = true;
        return result;
    }

    const QByteArray sourceBytes = sourcePath.toUtf8();
    const QByteArray outputBytes = outputPath.toUtf8();
    const int renderCode = ymfm_vgmrender_file(sourceBytes.constData(),
                                               outputBytes.constData(),
                                               kSampleRate,
                                               cancelRequested ? cancelRequested.get() : nullptr);
    if (renderCode == 8 || cancellationRequested(cancelRequested)) {
        result.cancelled = true;
        return result;
    }

    if (renderCode != 0) {
        result.errorString = renderErrorForCode(renderCode);
        return result;
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists() || outputInfo.size() <= 44) {
        result.errorString = QObject::tr("YMFM finished without producing rendered audio.");
    }

    return result;
}

QString YmfmRenderer::ensureOutputPath(const QString &sourcePath, const QString &formatName) const
{
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::tempPath() + QStringLiteral("/realagi-retro-term");
    }

    QDir cacheDir(cacheRoot);
    cacheDir.mkpath(QStringLiteral("ymfm-renders"));
    cacheDir.cd(QStringLiteral("ymfm-renders"));

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

QString YmfmRenderer::formatNameForPath(const QString &sourcePath) const
{
    if (sourcePath.isEmpty()) {
        return QString();
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString extension = sourceInfo.suffix().toLower();
    if (extension == QStringLiteral("vgm") || extension == QStringLiteral("vgz") || extension == QStringLiteral("gym")) {
        return extension.toUpper();
    }

    return QString();
}

QString YmfmRenderer::resolveSourcePath(const QUrl &sourceUrl) const
{
    if (!sourceUrl.isValid()) {
        return QString();
    }

    return sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : sourceUrl.toString();
}

QString YmfmRenderer::systemNameForPath(const QString &sourcePath) const
{
    const QString formatName = formatNameForPath(sourcePath);
    if (formatName.isEmpty()) {
        return QString();
    }

    if (formatName == QStringLiteral("GYM")) {
        return QStringLiteral("Sega Mega Drive / Genesis");
    }

    return QStringLiteral("Sega/Yamaha FM");
}

void YmfmRenderer::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    emit errorStringChanged();
}

void YmfmRenderer::setFormatName(const QString &formatName)
{
    if (m_formatName == formatName) {
        return;
    }

    m_formatName = formatName;
    emit formatNameChanged();
}

void YmfmRenderer::setOutputUrl(const QUrl &outputUrl)
{
    if (m_outputUrl == outputUrl) {
        return;
    }

    m_outputUrl = outputUrl;
    emit outputUrlChanged();
}

void YmfmRenderer::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }

    m_ready = ready;
    emit readyChanged();
}

void YmfmRenderer::setRendering(bool rendering)
{
    if (m_rendering == rendering) {
        return;
    }

    m_rendering = rendering;
    emit renderingChanged();
}

void YmfmRenderer::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;
    emit sourceChanged();
}

void YmfmRenderer::setSystemName(const QString &systemName)
{
    if (m_systemName == systemName) {
        return;
    }

    m_systemName = systemName;
    emit systemNameChanged();
}

void YmfmRenderer::stopRunningRender()
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
