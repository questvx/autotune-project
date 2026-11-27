// WavReader.cpp
#include "WavReader.h"
#include <algorithm>
#include <iostream>
#include <limits>

#define DR_WAV_IMPLEMENTATION
#include "../../external/dr_wav.h"

using namespace std;

//------------ Load WAV file ------------

bool WavReader::load(const filesystem::path &path)
{
    audioSamples.clear();
    sampleRate = 0;
    channels = 0;

    // Print the current working directory for debugging purposes
    // cout << "Current path(wav reader): "
    //      << filesystem::current_path()
    //      << "\n";

    // Check if the file exists
    if (!filesystem::exists(path))
    {
        cerr << "Error: File does not exist." << endl;
        return false;
    }

    // Load the WAV file using dr_wav
    drwav wav{};
    if (!drwav_init_file(&wav, path.string().c_str(), nullptr))
    {
        cerr << "Error: Failed to load WAV file." << endl;
        return false;
    }
    if (wav.channels != 1 && wav.channels != 2)
    {
        std::cerr << "Error: Only mono and stereo WAV files are supported.\n";
        drwav_uninit(&wav);
        return false;
    }

    // Variables
    const drwav_uint64 totalFrames = wav.totalPCMFrameCount;
    if (totalFrames >
        static_cast<drwav_uint64>(
            std::numeric_limits<size_t>::max() / wav.channels))
    {
        std::cerr << "Error: WAV dimensions exceed supported memory size.\n";
        drwav_uninit(&wav);
        return false;
    }
    const unsigned int loadedSampleRate = wav.sampleRate;
    const unsigned int loadedChannels = wav.channels;

    // Resize the audioSamples vector to hold all the audio samples
    audioSamples.resize(
        static_cast<size_t>(totalFrames) * wav.channels);
    // Read the audio samples into the vector
    const drwav_uint64 framesRead = drwav_read_pcm_frames_f32(
        &wav,
        totalFrames,
        audioSamples.data());

    drwav_uninit(&wav);

    // Check if all frames were read successfully
    if (framesRead != totalFrames)
    {
        std::cerr << "Error: Failed to read all samples.\n";
        audioSamples.clear();
        return false;
    }

    sampleRate = loadedSampleRate;
    channels = loadedChannels;

    cout << "\n--- WavReader ---" << endl;
    cout << "Sample Rate (frames/second): " << sampleRate << endl;
    cout << "Channels: " << channels << endl;
    cout << "First 10 samples of the first channel: ";
    const size_t framesToPrint = std::min<size_t>(10, totalFrames);
    for (size_t i = 0; i < framesToPrint; i++)
    {
        std::cout << i << ": "
                  << audioSamples[i * channels] << "\n";
    }


    return true;
}

//------------ Getters ------------
unsigned int WavReader::getSampleRate() const
{
    return sampleRate;
}

unsigned int WavReader::getChannels() const
{
    return channels;
}

const std::vector<float> &WavReader::getSamples() const
{
    return audioSamples;
}