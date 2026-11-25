#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

class AutoTuneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(
        juce::Graphics& graphics,
        int x,
        int y,
        int width,
        int height,
        float sliderPosition,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider) override;
};

class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override = default;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void chooseInputFile();
    void chooseOutputFile();
    void updateWaveformPreview(const juce::File& file);
    void startProcessing();
    void setProcessingState(bool isProcessing);
    void finishProcessing(
        bool succeeded,
        const juce::String& message,
        const juce::String& detectedNote,
        const juce::String& detectedFrequency);
    void updateProcessButton();
    void updateRetuneSpeedValue();

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label rootNoteLabel;
    juce::Label scaleTypeLabel;
    juce::Label retuneSpeedLabel;
    juce::Label formatLabel;
    juce::Label statusLabel;
    juce::Label pitchDisplayLabel;
    juce::Label pitchFrequencyLabel;
    juce::Label inputTypeLabel;
    juce::Label transposeLabel;
    juce::Label lowLatencyLabel;
    juce::Label flexTuneLabel;
    juce::Label naturalVibratoLabel;
    juce::Label humanizeLabel;
    juce::Label retuneSpeedValueLabel;
    juce::Label flexTuneValueLabel;
    juce::Label naturalVibratoValueLabel;
    juce::Label humanizeValueLabel;

    juce::TextButton inputButton{"Choose input WAV"};
    juce::TextButton outputButton{"Choose output"};
    juce::TextButton processButton{"Process and export"};
    juce::TextButton settingsButton{"⚙"};
    juce::ComboBox rootNoteBox;
    juce::ComboBox scaleTypeBox;
    juce::ComboBox inputTypeBox;
    juce::ToggleButton lowLatencyToggle{"Low latency"};

    AutoTuneLookAndFeel knobLookAndFeel;
    juce::Slider retuneSpeedSlider;
    juce::Slider flexTuneSlider;
    juce::Slider naturalVibratoSlider;
    juce::Slider humanizeSlider;
    juce::Slider transposeSlider;

    std::unique_ptr<juce::FileChooser> activeFileChooser;
    juce::File inputFile;
    juce::File outputFile;
    juce::ThreadPool processingPool{1};
    juce::Rectangle<int> waveformDisplayArea;
    std::vector<float> waveformPeaks;
    bool processing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
