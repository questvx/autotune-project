#pragma once

#include <JuceHeader.h>

#include <memory>

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
    void startProcessing();
    void setProcessingState(bool isProcessing);
    void finishProcessing(bool succeeded, const juce::String& message);
    void updateProcessButton();

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label rootNoteLabel;
    juce::Label scaleTypeLabel;
    juce::Label retuneSpeedLabel;
    juce::Label formatLabel;
    juce::Label statusLabel;

    juce::TextButton inputButton{"Choose input WAV"};
    juce::TextButton outputButton{"Choose output"};
    juce::TextButton processButton{"Process and export"};
    juce::ComboBox rootNoteBox;
    juce::ComboBox scaleTypeBox;
    juce::Slider retuneSpeedSlider;

    std::unique_ptr<juce::FileChooser> activeFileChooser;
    juce::File inputFile;
    juce::File outputFile;
    juce::ThreadPool processingPool{1};
    bool processing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
