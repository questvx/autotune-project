#include "MainComponent.h"

class AutoTuneMainWindow final : public juce::DocumentWindow
{
public:
    AutoTuneMainWindow()
        : juce::DocumentWindow(
              "AutoTune",
              juce::Colour(0xff111827),
              juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        setResizable(true, true);
        centreWithSize(700, 500);
        setVisible(true);
        getConstrainer()->setSizeLimits(600, 440, 1000, 800);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class AutoTuneApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "AutoTune";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<AutoTuneMainWindow>();
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

private:
    std::unique_ptr<AutoTuneMainWindow> mainWindow;
};

START_JUCE_APPLICATION(AutoTuneApplication)
