// WavReader.cpp
#include "WavReader.h"
// #include <filesystem>
#include <iostream>

#define DR_WAV_IMPLEMENTATION
#include "../../external/dr_wav.h"

using namespace std;

//------------ Load WAV file ------------

bool WavReader::load(const filesystem::path &path)
{

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
    drwav wav;
    if (!drwav_init_file(&wav, path.string().c_str(), nullptr))
    {
        cerr << "Error: Failed to load WAV file." << endl;
        return false;
    }
    // DELETE LATER ========================================
    if (wav.channels != 1)
    {
        std::cout << "Error: Only mono WAV files are supported.\n";
        drwav_uninit(&wav);
        return false;
    }
    // =====================================================

    // Variables
    const drwav_uint64 totalFrames = wav.totalPCMFrameCount;
    sampleRate = wav.sampleRate;
    channels = wav.channels;

    // Resize the audioSamples vector to hold all the audio samples
    audioSamples.resize(totalFrames * wav.channels);
    // Read the audio samples into the vector
    drwav_uint64 framesRead = drwav_read_pcm_frames_f32(&wav, totalFrames, audioSamples.data());

    drwav_uninit(&wav);

    // Check if all frames were read successfully
    if (framesRead != totalFrames)
    {
        std::cerr << "Error: Failed to read all samples.\n";
        return false;
    }

    cout << "\n--- WavReader ---" << endl;
    cout << "Sample Rate (frames/second): " << sampleRate << endl;
    cout << "Channels: " << channels << endl;
    cout << "First 10 samples of the first channel: ";
    for (int i = 0; i < 10; i++)
    {
        std::cout << i << ": " << audioSamples[i] << "\n";
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