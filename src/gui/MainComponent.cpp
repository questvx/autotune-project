#include "MainComponent.h"

#include "dsp/pitchCorrection/AutoPitchCorrector.h"
#include "scale/Scale.h"
#include "wavReader/WavReader.h"
#include "../../external/dr_wav.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

namespace
{
std::vector<float> calculateWaveformPeaks(
    const std::vector<float>& samples)
{
    constexpr size_t barCount = 72;
    std::vector<float> peaks(barCount, 0.0f);
    if (samples.empty())
        return peaks;

    for (size_t bar = 0; bar < barCount; ++bar)
    {
        const size_t start = bar * samples.size() / barCount;
        const size_t end = (bar + 1) * samples.size() / barCount;
        for (size_t i = start; i < end; ++i)
            peaks[bar] = std::max(peaks[bar], std::abs(samples[i]));
    }

    return peaks;
}

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

void AutoTuneLookAndFeel::drawRotarySlider(
    juce::Graphics& graphics,
    int x,
    int y,
    int width,
    int height,
    float sliderPosition,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    const float radius = static_cast<float>(std::min(width, height)) * 0.39f;
    const float centreX = static_cast<float>(x) +
        static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) +
        static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle +
        juce::jlimit(0.0f, 1.0f, sliderPosition) *
            (rotaryEndAngle - rotaryStartAngle);
    const juce::PathStrokeType arcStroke(
        4.0f,
        juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(
        centreX,
        centreY,
        radius,
        radius,
        0.0f,
        rotaryStartAngle,
        rotaryEndAngle,
        true);
    graphics.setColour(juce::Colour(0xff29364a));
    graphics.strokePath(backgroundArc, arcStroke);

    juce::Path valueArc;
    valueArc.addCentredArc(
        centreX,
        centreY,
        radius,
        radius,
        0.0f,
        rotaryStartAngle,
        angle,
        true);
    graphics.setColour(
        slider.isEnabled()
            ? slider.findColour(juce::Slider::rotarySliderFillColourId)
            : juce::Colour(0xff596579));
    graphics.strokePath(valueArc, arcStroke);

    const float dialRadius = radius * 0.76f;
    graphics.setColour(juce::Colour(0xff152033));
    graphics.fillEllipse(
        centreX - dialRadius,
        centreY - dialRadius,
        dialRadius * 2.0f,
        dialRadius * 2.0f);

    graphics.setColour(juce::Colour(0xff9aa9be));
    graphics.drawEllipse(
        centreX - dialRadius,
        centreY - dialRadius,
        dialRadius * 2.0f,
        dialRadius * 2.0f,
        1.0f);

    const float pointerLength = dialRadius * 0.66f;
    graphics.setColour(juce::Colour(0xffe6edf7));
    graphics.drawLine(
        centreX,
        centreY,
        centreX + std::sin(angle) * pointerLength,
        centreY - std::cos(angle) * pointerLength,
        2.0f);
    graphics.fillEllipse(centreX - 2.5f, centreY - 2.5f, 5.0f, 5.0f);
}

MainComponent::MainComponent()
{
    titleLabel.setText("AutoTune", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    titleLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xfff3f4f6));

    subtitleLabel.setText(
        "PITCH CORRECTION  /  MONO WAV",
        juce::dontSendNotification);
    subtitleLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xff8292aa));

    inputLabel.setText("No input selected", juce::dontSendNotification);
    outputLabel.setText("No output selected", juce::dontSendNotification);
    formatLabel.setText("MONO WAV  |  OFFLINE", juce::dontSendNotification);
    statusLabel.setText("Choose a file to begin.", juce::dontSendNotification);

    for (juce::Label* label :
         {&inputLabel, &outputLabel, &rootNoteLabel, &scaleTypeLabel,
          &retuneSpeedLabel, &formatLabel, &statusLabel,
          &inputTypeLabel, &transposeLabel,
          &lowLatencyLabel, &flexTuneLabel, &naturalVibratoLabel,
          &humanizeLabel, &retuneSpeedValueLabel, &flexTuneValueLabel,
          &naturalVibratoValueLabel, &humanizeValueLabel})
    {
        label->setColour(
            juce::Label::textColourId,
            juce::Colour(0xffc7d2e2));
    }

    rootNoteLabel.setText("Root note", juce::dontSendNotification);
    scaleTypeLabel.setText("Scale", juce::dontSendNotification);
    retuneSpeedLabel.setText("Retune speed", juce::dontSendNotification);
    flexTuneLabel.setText("Flex Tune", juce::dontSendNotification);
    naturalVibratoLabel.setText(
        "Natural Vibrato",
        juce::dontSendNotification);
    humanizeLabel.setText("Humanize", juce::dontSendNotification);
    inputTypeLabel.setText("Input Type  |  WIP", juce::dontSendNotification);
    transposeLabel.setText("Transpose  |  WIP", juce::dontSendNotification);
    lowLatencyLabel.setText("WIP", juce::dontSendNotification);
    formatLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xff8493a8));
    statusLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffaebbd0));
    lowLatencyLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xff8190a6));

    static constexpr std::array<const char*, 12> noteNames{
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < static_cast<int>(noteNames.size()); ++i)
        rootNoteBox.addItem(noteNames[static_cast<size_t>(i)], i + 1);
    rootNoteBox.setSelectedId(1);

    scaleTypeBox.addItem("Major", 1);
    scaleTypeBox.addItem("Minor", 2);
    scaleTypeBox.setSelectedId(1);

    inputTypeBox.addItem("Low Male", 1);
    inputTypeBox.addItem("High Male", 2);
    inputTypeBox.addItem("Low Female", 3);
    inputTypeBox.addItem("High Female", 4);
    inputTypeBox.setSelectedId(1);
    inputTypeBox.setEnabled(false);

    transposeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    transposeSlider.setTextBoxStyle(
        juce::Slider::TextBoxRight,
        false,
        54,
        22);
    transposeSlider.setRange(-12, 12, 1);
    transposeSlider.setValue(0);
    transposeSlider.setTextValueSuffix(" st");
    transposeSlider.setEnabled(false);
    lowLatencyToggle.setEnabled(false);

    auto configureRotary = [this](juce::Slider& slider, juce::Colour accent)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(
            juce::MathConstants<float>::pi * 1.2f,
            juce::MathConstants<float>::pi * 2.8f,
            true);
        slider.setColour(
            juce::Slider::rotarySliderFillColourId,
            accent);
        slider.setLookAndFeel(&knobLookAndFeel);
    };

    configureRotary(retuneSpeedSlider, juce::Colour(0xff4b8cff));
    retuneSpeedSlider.setRange(1.0, 100.0, 1.0);
    retuneSpeedSlider.setValue(100.0);
    retuneSpeedSlider.onValueChange = [this] { updateRetuneSpeedValue(); };

    configureRotary(flexTuneSlider, juce::Colour(0xff27d6a5));
    flexTuneSlider.setRange(0.0, 100.0, 1.0);
    flexTuneSlider.setValue(50.0);
    flexTuneSlider.setEnabled(false);

    configureRotary(naturalVibratoSlider, juce::Colour(0xff9278ff));
    naturalVibratoSlider.setRange(0.0, 100.0, 1.0);
    naturalVibratoSlider.setValue(30.0);
    naturalVibratoSlider.setEnabled(false);

    configureRotary(humanizeSlider, juce::Colour(0xffffbd48));
    humanizeSlider.setRange(0.0, 100.0, 1.0);
    humanizeSlider.setValue(20.0);
    humanizeSlider.setEnabled(false);

    updateRetuneSpeedValue();
    flexTuneValueLabel.setText("50%  |  WIP", juce::dontSendNotification);
    naturalVibratoValueLabel.setText(
        "30%  |  WIP",
        juce::dontSendNotification);
    humanizeValueLabel.setText("20%  |  WIP", juce::dontSendNotification);
    for (juce::Label* valueLabel :
         {&retuneSpeedValueLabel, &flexTuneValueLabel,
          &naturalVibratoValueLabel, &humanizeValueLabel})
    {
        valueLabel->setJustificationType(juce::Justification::centred);
    }

    processButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xff3979e8));
    processButton.setColour(
        juce::TextButton::textColourOffId,
        juce::Colour(0xffffffff));
    inputButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xff202c40));
    outputButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xff202c40));
    settingsButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xff182336));
    settingsButton.setTooltip("Settings are work in progress");

    inputButton.onClick = [this] { chooseInputFile(); };
    outputButton.onClick = [this] { chooseOutputFile(); };
    processButton.onClick = [this] { startProcessing(); };
    settingsButton.onClick = [this]
    {
        statusLabel.setText(
            "Additional settings are work in progress.",
            juce::dontSendNotification);
    };

    const std::array<juce::Component*, 32> components{
        &titleLabel,
        &subtitleLabel,
        &inputLabel,
        &outputLabel,
        &rootNoteLabel,
        &scaleTypeLabel,
        &retuneSpeedLabel,
        &formatLabel,
        &statusLabel,
        &inputTypeLabel,
        &transposeLabel,
        &lowLatencyLabel,
        &flexTuneLabel,
        &naturalVibratoLabel,
        &humanizeLabel,
        &retuneSpeedValueLabel,
        &flexTuneValueLabel,
        &naturalVibratoValueLabel,
        &humanizeValueLabel,
        &inputButton,
        &outputButton,
        &processButton,
        &settingsButton,
        &rootNoteBox,
        &scaleTypeBox,
        &inputTypeBox,
        &lowLatencyToggle,
        &retuneSpeedSlider,
        &flexTuneSlider,
        &naturalVibratoSlider,
        &humanizeSlider,
        &transposeSlider};
    for (juce::Component* component : components)
        addAndMakeVisible(component);

    setSize(980, 660);
    updateProcessButton();
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff0a1020));

    auto panels = getLocalBounds().reduced(22);
    panels.removeFromTop(56);
    auto topPanel = panels.removeFromTop(328);
    auto settingsPanel = topPanel.removeFromRight(248);
    topPanel.removeFromRight(12);
    panels.removeFromTop(14);
    const auto knobPanel = panels;

    graphics.setColour(juce::Colour(0xff111a2a));
    graphics.fillRoundedRectangle(topPanel.toFloat(), 16.0f);
    graphics.setColour(juce::Colour(0xff101a2b));
    graphics.fillRoundedRectangle(settingsPanel.toFloat(), 16.0f);
    graphics.setColour(juce::Colour(0xff111a2a));
    graphics.fillRoundedRectangle(knobPanel.toFloat(), 16.0f);
    graphics.setColour(juce::Colour(0xff25334a));
    graphics.drawRoundedRectangle(topPanel.toFloat(), 16.0f, 1.0f);
    graphics.drawRoundedRectangle(settingsPanel.toFloat(), 16.0f, 1.0f);
    graphics.drawRoundedRectangle(knobPanel.toFloat(), 16.0f, 1.0f);

    if (waveformDisplayArea.isEmpty())
        return;

    const float centreY =
        static_cast<float>(waveformDisplayArea.getCentreY());
    const float centreX =
        static_cast<float>(waveformDisplayArea.getCentreX());
    const float maxHeight =
        static_cast<float>(waveformDisplayArea.getHeight()) * 0.42f;

    graphics.setColour(juce::Colour(0xff26364e));
    graphics.drawLine(
        static_cast<float>(waveformDisplayArea.getX() + 8),
        centreY,
        static_cast<float>(waveformDisplayArea.getRight() - 8),
        centreY,
        1.0f);

    for (size_t i = 0; i < waveformPeaks.size(); ++i)
    {
        const float fraction = waveformPeaks.size() > 1
            ? static_cast<float>(i) /
                static_cast<float>(waveformPeaks.size() - 1)
            : 0.5f;
        const float x = static_cast<float>(waveformDisplayArea.getX() + 8) +
            fraction *
                static_cast<float>(waveformDisplayArea.getWidth() - 16);
        if (std::abs(x - centreX) < 82.0f)
            continue;

        const float amplitude = juce::jmax(
            3.0f,
            waveformPeaks[i] * maxHeight);
        graphics.setColour(
            juce::Colour(0xff4b82f1).withAlpha(juce::jlimit(
                0.35f,
                1.0f,
                0.35f + 0.65f * waveformPeaks[i])));
        graphics.drawLine(
            x,
            centreY - amplitude,
            x,
            centreY + amplitude,
            2.0f);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(22);
    auto header = area.removeFromTop(44);
    settingsButton.setBounds(header.removeFromRight(38).reduced(0, 2));
    titleLabel.setBounds(header.removeFromLeft(145));
    subtitleLabel.setBounds(header);
    area.removeFromTop(12);

    auto topPanel = area.removeFromTop(328);
    auto settingsArea = topPanel.removeFromRight(248).reduced(16, 14);
    topPanel.removeFromRight(12);
    auto mainArea = topPanel.reduced(18, 14);

    auto inputRow = mainArea.removeFromTop(34);
    inputButton.setBounds(inputRow.removeFromLeft(126).reduced(0, 2));
    inputLabel.setBounds(inputRow.reduced(8, 3));
    mainArea.removeFromTop(4);

    waveformDisplayArea = mainArea.removeFromTop(136).reduced(0, 4);
    mainArea.removeFromTop(4);

    auto outputRow = mainArea.removeFromTop(32);
    outputButton.setBounds(outputRow.removeFromRight(126).reduced(0, 2));
    outputLabel.setBounds(outputRow.reduced(8, 3));
    mainArea.removeFromTop(4);
    processButton.setBounds(mainArea.removeFromTop(36));
    mainArea.removeFromTop(4);
    formatLabel.setBounds(mainArea.removeFromTop(18));
    statusLabel.setBounds(mainArea.removeFromTop(24));

    rootNoteLabel.setBounds(settingsArea.removeFromTop(20));
    rootNoteBox.setBounds(settingsArea.removeFromTop(30));
    settingsArea.removeFromTop(7);
    scaleTypeLabel.setBounds(settingsArea.removeFromTop(20));
    scaleTypeBox.setBounds(settingsArea.removeFromTop(30));
    settingsArea.removeFromTop(7);
    inputTypeLabel.setBounds(settingsArea.removeFromTop(20));
    inputTypeBox.setBounds(settingsArea.removeFromTop(30));
    settingsArea.removeFromTop(7);
    transposeLabel.setBounds(settingsArea.removeFromTop(20));
    transposeSlider.setBounds(settingsArea.removeFromTop(34));
    settingsArea.removeFromTop(5);
    auto latencyRow = settingsArea.removeFromTop(28);
    lowLatencyToggle.setBounds(latencyRow.removeFromLeft(138));
    lowLatencyLabel.setBounds(latencyRow);

    area.removeFromTop(14);
    auto knobArea = area.reduced(14, 10);
    std::array<juce::Label*, 4> knobLabels{
        &retuneSpeedLabel,
        &flexTuneLabel,
        &naturalVibratoLabel,
        &humanizeLabel};
    std::array<juce::Slider*, 4> knobs{
        &retuneSpeedSlider,
        &flexTuneSlider,
        &naturalVibratoSlider,
        &humanizeSlider};
    std::array<juce::Label*, 4> knobValues{
        &retuneSpeedValueLabel,
        &flexTuneValueLabel,
        &naturalVibratoValueLabel,
        &humanizeValueLabel};

    const int columnWidth = knobArea.getWidth() / 4;
    for (size_t i = 0; i < knobLabels.size(); ++i)
    {
        auto column = i + 1 == knobLabels.size()
            ? knobArea
            : knobArea.removeFromLeft(columnWidth);
        knobLabels[i]->setBounds(column.removeFromTop(24));
        knobs[i]->setBounds(column.removeFromTop(122).reduced(22, 0));
        knobValues[i]->setBounds(column.removeFromTop(22));
    }
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
            safeThis->waveformPeaks.clear();
            safeThis->repaint();
            safeThis->updateProcessButton();
            safeThis->updateWaveformPreview(selected);
        });
}

void MainComponent::updateWaveformPreview(const juce::File& file)
{
    const std::filesystem::path path(file.getFullPathName().toStdString());
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    processingPool.addJob([safeThis, path]
    {
        WavReader reader;
        const bool loaded = reader.load(path);
        std::vector<float> peaks;
        if (loaded)
            peaks = calculateWaveformPeaks(reader.getSamples());

        juce::MessageManager::callAsync(
            [safeThis, loaded, peaks = std::move(peaks)]() mutable
            {
                if (safeThis == nullptr)
                    return;

                safeThis->waveformPeaks = std::move(peaks);
                safeThis->repaint();
                if (!loaded && !safeThis->processing)
                {
                    safeThis->statusLabel.setText(
                        "Preview unavailable. Mono WAV is required.",
                        juce::dontSendNotification);
                }
            });
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
        static_cast<float>(retuneSpeedSlider.getValue() / 100.0);
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    setProcessingState(true);
    statusLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffaebbd0));
    statusLabel.setText(
        "Processing audio...",
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

void MainComponent::updateRetuneSpeedValue()
{
    retuneSpeedValueLabel.setText(
        juce::String(
            static_cast<int>(retuneSpeedSlider.getValue())) + "%",
        juce::dontSendNotification);
}
