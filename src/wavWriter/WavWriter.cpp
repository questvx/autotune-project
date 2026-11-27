#include "WavWriter.h"

#include "../../external/dr_wav.h"

bool WavWriter::writeFloatWav(
    const std::filesystem::path& path,
    unsigned int sampleRate,
    const std::vector<float>& interleavedSamples,
    unsigned int channelCount)
{
    if (interleavedSamples.empty() ||
        sampleRate == 0 ||
        (channelCount != 1 && channelCount != 2) ||
        interleavedSamples.size() % channelCount != 0)
    {
        return false;
    }

    const size_t frameCount = interleavedSamples.size() / channelCount;
    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = static_cast<drwav_uint16>(channelCount);
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav{};
    if (!drwav_init_file_write_sequential_pcm_frames(
            &wav,
            path.string().c_str(),
            &format,
            static_cast<drwav_uint64>(frameCount),
            nullptr))
    {
        return false;
    }

    const drwav_uint64 framesWritten = drwav_write_pcm_frames(
        &wav,
        static_cast<drwav_uint64>(frameCount),
        interleavedSamples.data());
    const drwav_result uninitResult = drwav_uninit(&wav);
    return framesWritten == frameCount &&
        uninitResult == DRWAV_SUCCESS;
}
