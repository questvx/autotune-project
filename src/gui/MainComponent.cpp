#include "MainComponent.h"

#include "dsp/pitchCorrection/AutoPitchCorrector.h"
#include "scale/Scale.h"
#include "wavReader/WavReader.h"
#include "../../external/dr_wav.h"

#include <array>
#include <filesystem>
#include <vector>

namespace
{
bool writeFloatWav(
    const std::filesystem::path& path,
    unsigned int sampleRate,
    const std::vector<float>& samples)
{
    if (samples.empty())
        return false;

    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav{};
    if (!drwav_init_file_write_sequential_pcm_frames(
            &wav,
            path.string().c_str(),
            &format,
            static_cast<drwav_uint64>(samples.size()),
            nullptr))
    {
        return false;
    }

    const drwav_uint64 framesWritten = drwav_write_pcm_frames(
        &wav,
        static_cast<drwav_uint64>(samples.size()),
        samples.data());
    const drwav_result uninitResult = drwav_uninit(&wav);
    return framesWritten == samples.size() &&
        uninitResult == DRWAV_SUCCESS;
}
}

MainComponent::MainComponent()
{
    titleLabel.setText("AutoTune", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    titleLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xfff3f4f6));

    subtitleLabel.setText(
        "Offline pitch correction for mono WAV files",
        juce::dontSendNotification);
    subtitleLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffaeb9ca));

    inputLabel.setText("No input selected", juce::dontSendNotification);
    outputLabel.setText("No output selected", juce::dontSendNotification);
    formatLabel.setText(
        "Input currently supports mono WAV files.",
        juce::dontSendNotification);
    statusLabel.setText("Choose a file and correction settings.", juce::dontSendNotification);

    for (juce::Label* label :
         {&inputLabel, &outputLabel, &rootNoteLabel, &scaleTypeLabel,
          &retuneSpeedLabel, &formatLabel, &statusLabel})
    {
        label->setColour(
            juce::Label::textColourId,
            juce::Colour(0xffd5deeb));
    }

    rootNoteLabel.setText("Root note", juce::dontSendNotification);
    scaleTypeLabel.setText("Scale", juce::dontSendNotification);
    retuneSpeedLabel.setText("Retune speed", juce::dontSendNotification);

    static constexpr std::array<const char*, 12> noteNames{
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < static_cast<int>(noteNames.size()); ++i)
        rootNoteBox.addItem(noteNames[static_cast<size_t>(i)], i + 1);
    rootNoteBox.setSelectedId(1);

    scaleTypeBox.addItem("Major", 1);
    scaleTypeBox.addItem("Minor", 2);
    scaleTypeBox.setSelectedId(1);

    retuneSpeedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    retuneSpeedSlider.setTextBoxStyle(
        juce::Slider::TextBoxRight,
        false,
        72,
        24);
    retuneSpeedSlider.setRange(0.01, 1.0, 0.01);
    retuneSpeedSlider.setValue(1.0);
    retuneSpeedSlider.setNumDecimalPlacesToDisplay(2);

    processButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xff2878d0));
    processButton.setColour(
        juce::TextButton::textColourOffId,
        juce::Colour(0xffffffff));

    inputButton.onClick = [this] { chooseInputFile(); };
    outputButton.onClick = [this] { chooseOutputFile(); };
    processButton.onClick = [this] { startProcessing(); };

    const std::array<juce::Component*, 15> components{
        &titleLabel,
        &subtitleLabel,
        &inputLabel,
        &outputLabel,
        &rootNoteLabel,
        &scaleTypeLabel,
        &retuneSpeedLabel,
        &formatLabel,
        &statusLabel,
        &inputButton,
        &outputButton,
        &processButton,
        &rootNoteBox,
        &scaleTypeBox,
        &retuneSpeedSlider};
    for (juce::Component* component : components)
    {
        addAndMakeVisible(component);
    }

    setSize(700, 500);
    updateProcessButton();
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff111827));
    graphics.setColour(juce::Colour(0xff1d293b));
    graphics.fillRoundedRectangle(
        getLocalBounds().toFloat().reduced(12.0f),
        18.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(30);
    titleLabel.setBounds(area.removeFromTop(40));
    subtitleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(14);

    auto inputRow = area.removeFromTop(38);
    inputButton.setBounds(inputRow.removeFromRight(145));
    inputLabel.setBounds(inputRow.reduced(0, 4));
    area.removeFromTop(10);

    auto outputRow = area.removeFromTop(38);
    outputButton.setBounds(outputRow.removeFromRight(145));
    outputLabel.setBounds(outputRow.reduced(0, 4));
    area.removeFromTop(20);

    auto settingsRow = area.removeFromTop(74);
    auto rootColumn = settingsRow.removeFromLeft(settingsRow.getWidth() / 2);
    rootNoteLabel.setBounds(rootColumn.removeFromTop(26));
    rootNoteBox.setBounds(rootColumn.removeFromTop(38).reduced(0, 2));
    scaleTypeLabel.setBounds(settingsRow.removeFromTop(26));
    scaleTypeBox.setBounds(settingsRow.removeFromTop(38).reduced(0, 2));
    area.removeFromTop(12);

    retuneSpeedLabel.setBounds(area.removeFromTop(26));
    retuneSpeedSlider.setBounds(area.removeFromTop(42));
    area.removeFromTop(10);

    formatLabel.setBounds(area.removeFromTop(24));
    statusLabel.setBounds(area.removeFromTop(30));
    processButton.setBounds(area.removeFromBottom(46));
}

void MainComponent::chooseInputFile()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeFileChooser = std::make_unique<juce::FileChooser>(
        "Choose a mono WAV file",
        juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory),
        "*.wav",
        true,
        false,
        this);

    activeFileChooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const juce::File selected = chooser.getResult();
            if (!selected.existsAsFile())
                return;

            safeThis->inputFile = selected;
            safeThis->outputFile = juce::File();
            safeThis->inputLabel.setText(
                selected.getFileName(),
                juce::dontSendNotification);
            safeThis->outputLabel.setText(
                "No output selected",
                juce::dontSendNotification);
            safeThis->statusLabel.setText(
                "Choose where to save the corrected WAV.",
                juce::dontSendNotification);
            safeThis->updateProcessButton();
        });
}

void MainComponent::chooseOutputFile()
{
    if (!inputFile.existsAsFile())
    {
        statusLabel.setText(
            "Choose an input WAV first.",
            juce::dontSendNotification);
        return;
    }

    const juce::File suggestedOutput = inputFile.getSiblingFile(
        inputFile.getFileNameWithoutExtension() + "_corrected.wav");
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeFileChooser = std::make_unique<juce::FileChooser>(
        "Choose where to save the corrected WAV",
        suggestedOutput,
        "*.wav",
        true,
        false,
        this);

    activeFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            juce::File selected = chooser.getResult();
            if (selected == juce::File())
                return;

            if (selected.getFileExtension().compareIgnoreCase(".wav") != 0)
            {
                selected = selected.getSiblingFile(
                    selected.getFileNameWithoutExtension() + ".wav");
            }

            safeThis->outputFile = selected;
            safeThis->outputLabel.setText(
                selected.getFileName(),
                juce::dontSendNotification);
            safeThis->statusLabel.setText(
                "Ready to process.",
                juce::dontSendNotification);
            safeThis->updateProcessButton();
        });
}

void MainComponent::startProcessing()
{
    if (processing)
        return;

    if (!inputFile.existsAsFile())
    {
        statusLabel.setText(
            "Choose a valid input WAV first.",
            juce::dontSendNotification);
        return;
    }

    if (outputFile == juce::File())
    {
        statusLabel.setText(
            "Choose an output file first.",
            juce::dontSendNotification);
        return;
    }

    if (inputFile == outputFile)
    {
        statusLabel.setText(
            "Input and output files must be different.",
            juce::dontSendNotification);
        return;
    }

    const std::filesystem::path inputPath =
        std::filesystem::path(inputFile.getFullPathName().toStdString());
    const std::filesystem::path outputPath =
        std::filesystem::path(outputFile.getFullPathName().toStdString());
    const juce::String outputName = outputFile.getFileName();
    const int rootMidi = 60 + rootNoteBox.getSelectedId() - 1;
    const ScaleType scaleType = scaleTypeBox.getSelectedId() == 2
        ? ScaleType::Minor
        : ScaleType::Major;
    const float retuneSpeed =
        static_cast<float>(retuneSpeedSlider.getValue());
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    setProcessingState(true);
    statusLabel.setText(
        "Processing audio…",
        juce::dontSendNotification);

    processingPool.addJob(
        [safeThis,
         inputPath,
         outputPath,
         outputName,
         rootMidi,
         scaleType,
         retuneSpeed]
        {
            bool succeeded = false;
            juce::String message;
            WavReader reader;

            if (!reader.load(inputPath))
            {
                message =
                    "Could not read the file. Check that it is a valid mono WAV.";
            }
            else
            {
                const Scale scale(rootMidi, scaleType);
                const AutoPitchCorrectionResult correction =
                    AutoPitchCorrector::process(
                        reader.getSamples(),
                        static_cast<float>(reader.getSampleRate()),
                        scale,
                        2048,
                        512,
                        50.0f,
                        1000.0f,
                        retuneSpeed);

                if (correction.samples.size() != reader.getSamples().size() ||
                    correction.frameAnalysis.empty() ||
                    correction.frameDiagnostics.size() !=
                        correction.frameAnalysis.size())
                {
                    message = "Pitch correction returned invalid output.";
                }
                else if (!writeFloatWav(
                             outputPath,
                             reader.getSampleRate(),
                             correction.samples))
                {
                    message = "Could not write the output WAV file.";
                }
                else
                {
                    succeeded = true;
                    message = "Finished: " + outputName;
                }
            }

            juce::MessageManager::callAsync(
                [safeThis, succeeded, message]
                {
                    if (safeThis != nullptr)
                        safeThis->finishProcessing(succeeded, message);
                });
        });
}

void MainComponent::setProcessingState(bool isProcessing)
{
    processing = isProcessing;
    inputButton.setEnabled(!processing);
    outputButton.setEnabled(!processing);
    rootNoteBox.setEnabled(!processing);
    scaleTypeBox.setEnabled(!processing);
    retuneSpeedSlider.setEnabled(!processing);
    processButton.setButtonText(
        processing ? "Processing..." : "Process and export");
    updateProcessButton();
}

void MainComponent::finishProcessing(
    bool succeeded,
    const juce::String& message)
{
    setProcessingState(false);
    statusLabel.setText(
        message,
        juce::dontSendNotification);
    statusLabel.setColour(
        juce::Label::textColourId,
        succeeded
            ? juce::Colour(0xff74d99f)
            : juce::Colour(0xffff8c8c));
}

void MainComponent::updateProcessButton()
{
    processButton.setEnabled(
        !processing &&
        inputFile.existsAsFile() &&
        outputFile != juce::File());
}
