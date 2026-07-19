#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "PianoKey.h"

namespace
{
using namespace juce;

constexpr auto lifecycleFlag = "--synth-lifecycle-test";
constexpr auto invalidDeviceName = "ModelD lifecycle test - deliberately nonexistent output";

struct LifecycleOptions
{
    enum class Mode
    {
        normal,
        invalid,
        noDevice
    };

    Mode mode = Mode::normal;
    File report;
    File screenshot;
    bool enabled = false;
    String parseError;

    String modeName() const
    {
        switch (mode)
        {
            case Mode::normal:   return "normal";
            case Mode::invalid:  return "invalid";
            case Mode::noDevice: return "no-device";
        }

        jassertfalse;
        return {};
    }
};

static LifecycleOptions parseLifecycleOptions()
{
    LifecycleOptions result;

   #if SYNTH_ENABLE_STANDALONE_LIFECYCLE_TESTS
    const auto arguments = JUCEApplicationBase::getCommandLineParameterArray();
    if (! arguments.contains (lifecycleFlag))
        return result;

    result.enabled = true;
    if (arguments.size() != 6
        || arguments[0] != lifecycleFlag
        || arguments[2] != "--report"
        || arguments[4] != "--screenshot")
    {
        result.parseError = "Expected --synth-lifecycle-test <normal|invalid|no-device> --report <absolute-path> --screenshot <absolute-path>";
        return result;
    }

    if (arguments[1] == "normal")
        result.mode = LifecycleOptions::Mode::normal;
    else if (arguments[1] == "invalid")
        result.mode = LifecycleOptions::Mode::invalid;
    else if (arguments[1] == "no-device")
        result.mode = LifecycleOptions::Mode::noDevice;
    else
        result.parseError = "Unsupported standalone lifecycle mode: " + arguments[1];

    result.report = File (arguments[3]);
    result.screenshot = File (arguments[5]);
    if (! File::isAbsolutePath (result.report.getFullPathName())
        || ! File::isAbsolutePath (result.screenshot.getFullPathName()))
        result.parseError = "Lifecycle report and screenshot paths must be absolute";
   #endif

    return result;
}

static int countEnabledMidiInputs (AudioDeviceManager& manager)
{
    int count = 0;
    for (const auto& device : MidiInput::getAvailableDevices())
        if (manager.isMidiInputDeviceEnabled (device.identifier))
            ++count;

    return count;
}

template <typename Visitor>
static void visitComponents (Component& parent, Visitor&& visitor)
{
    visitor (parent);
    for (auto* child : parent.getChildren())
        visitComponents (*child, visitor);
}

static AudioProcessorEditor* findActiveEditor (StandaloneFilterWindow& window)
{
    if (auto* processor = window.getAudioProcessor())
        return processor->getActiveEditor();

    return nullptr;
}

class LifecycleStandaloneWindow final : public StandaloneFilterWindow
{
public:
    LifecycleStandaloneWindow (const String& title,
                               Colour background,
                               std::unique_ptr<StandalonePluginHolder> holder)
        : StandaloneFilterWindow (title, background, std::move (holder))
    {
        statusLabel.setJustificationType (Justification::centredLeft);
        statusLabel.setColour (Label::textColourId,
                               getLookAndFeel().findColour (DocumentWindow::textColourId));
        statusLabel.setInterceptsMouseClicks (false, false);
        Component::addAndMakeVisible (&statusLabel);
        setStatus ("Audio: initialising | MIDI inputs enabled: 0");
    }

    void resized() override
    {
        StandaloneFilterWindow::resized();
        statusLabel.setBounds (76, 2, jmax (0, getWidth() - 84), jmax (0, getTitleBarHeight() - 4));
    }

    void setStatus (const String& status)
    {
        statusLabel.setText (status, dontSendNotification);
        statusLabel.setTooltip (status);
    }

    String getStatus() const
    {
        return statusLabel.getText();
    }

private:
    Label statusLabel;
};

class ModelDStandaloneApplication final : public JUCEApplication,
                                          private Timer
{
public:
    ModelDStandaloneApplication()
    {
        PropertiesFile::Options options;
        options.applicationName = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = {};
       #endif
        applicationProperties.setStorageParameters (options);
    }

    const String getApplicationName() override { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const String&) override {}

    void initialise (const String&) override
    {
        lifecycle = parseLifecycleOptions();
        applicationStartMs = Time::getMillisecondCounterHiRes();

        if (lifecycle.enabled && lifecycle.parseError.isNotEmpty())
        {
            failBeforeWindow (lifecycle.parseError);
            return;
        }

        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            failBeforeWindow ("No display is available for the standalone editor");
            return;
        }

        Array<StandalonePluginHolder::PluginInOuts> initialChannels;
        initialChannels.add ({ 0, 0 });
        auto holder = std::make_unique<StandalonePluginHolder> (
            applicationProperties.getUserSettings(), false, String{}, nullptr, initialChannels, false);

        mainWindow = std::make_unique<LifecycleStandaloneWindow> (
            getApplicationName(),
            LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
            std::move (holder));
        mainWindow->setVisible (true);
        mainWindow->toFront (true);

        firstVisibleMs = Time::getMillisecondCounterHiRes() - applicationStartMs;
        startTimer (50);
    }

    void shutdown() override
    {
        stopTimer();
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();
        mainWindow = nullptr;
        applicationProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []
            {
                if (auto* app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    enum class TimerPhase
    {
        configureDevice,
        captureEvidence
    };

    void timerCallback() override
    {
        stopTimer();
        if (mainWindow == nullptr)
        {
            finishLifecycleTest ("Standalone window disappeared before device configuration");
            return;
        }

        if (timerPhase == TimerPhase::configureDevice)
        {
            configureDeviceAfterWindowIsVisible();
            timerPhase = TimerPhase::captureEvidence;
            startTimer (150);
            return;
        }

        finishLifecycleTest ({});
    }

    void configureDeviceAfterWindowIsVisible()
    {
        deviceConfigurationStartMs = Time::getMillisecondCounterHiRes() - applicationStartMs;
        auto& holder = *mainWindow->pluginHolder;
        auto& deviceManager = holder.deviceManager;
        holder.channelConfiguration.clearQuick();
        holder.channelConfiguration.add ({ 0, 2 });

        if (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::noDevice)
        {
            for (const auto& device : MidiInput::getAvailableDevices())
                deviceManager.setMidiInputDeviceEnabled (device.identifier, false);
            deviceOpenError.clear();
        }
        else if (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::invalid)
        {
            AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName.clear();
            setup.outputDeviceName = invalidDeviceName;
            deviceOpenError = deviceManager.initialise (0, 2, nullptr, false, {}, &setup);
        }
        else
        {
            std::unique_ptr<XmlElement> savedState;
            if (holder.settings != nullptr)
                savedState = holder.settings->getXmlValue ("audioSetup");
            deviceOpenError = deviceManager.initialise (0, 2, savedState.get(), true);
        }

        updateStatusText();
    }

    void updateStatusText()
    {
        auto& manager = mainWindow->pluginHolder->deviceManager;
        const auto midiCount = countEnabledMidiInputs (manager);

        if (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::noDevice)
        {
            mainWindow->setStatus ("Audio: disabled (no-device test) | MIDI inputs enabled: 0");
            return;
        }

        if (const auto* device = manager.getCurrentAudioDevice())
        {
            mainWindow->setStatus ("Audio: " + device->getName()
                                   + " | MIDI inputs enabled: " + String (midiCount));
            return;
        }

        auto reason = deviceOpenError.isNotEmpty() ? deviceOpenError : String ("no output device available");
        mainWindow->setStatus ("Audio unavailable: " + reason
                               + " | MIDI inputs enabled: " + String (midiCount));
    }

    void failBeforeWindow (const String& error)
    {
        if (! lifecycle.enabled)
        {
            Logger::writeToLog (error);
            setApplicationReturnValue (1);
            quit();
            return;
        }

        failureBeforeWindow = error;
        finishLifecycleTest (error);
    }

    void finishLifecycleTest (const String& preconditionFailure)
    {
        if (! lifecycle.enabled)
            return;

        StringArray failures;
        if (preconditionFailure.isNotEmpty())
            failures.add (preconditionFailure);

        auto assertions = DynamicObject::Ptr (new DynamicObject());
        const auto recordAssertion = [&] (const Identifier& name, bool passed, const String& failure)
        {
            assertions->setProperty (name, passed);
            if (! passed)
                failures.add (failure);
        };

        const auto hasWindow = mainWindow != nullptr;
        const auto windowBounds = hasWindow ? mainWindow->getBounds() : Rectangle<int>{};
        recordAssertion ("top_level_visible", hasWindow && mainWindow->isVisible(), "Top-level window is not visible");
        recordAssertion ("top_level_showing", hasWindow && mainWindow->isShowing(), "Top-level window is not showing");
        recordAssertion ("native_peer_present", hasWindow && mainWindow->getPeer() != nullptr, "Top-level window has no native peer");
        recordAssertion ("window_bounds_nonempty", ! windowBounds.isEmpty(), "Top-level window bounds are empty");
        recordAssertion ("visible_before_device_configuration",
                         firstVisibleMs >= 0.0 && deviceConfigurationStartMs > firstVisibleMs,
                         "Device configuration did not begin after first visibility");

        auto* editor = hasWindow ? findActiveEditor (*mainWindow) : nullptr;
        const auto editorBounds = editor != nullptr ? editor->getBounds() : Rectangle<int>{};
        recordAssertion ("custom_editor_visible", editor != nullptr && editor->isVisible(), "Custom editor is not visible");
        recordAssertion ("custom_editor_showing", editor != nullptr && editor->isShowing(), "Custom editor is not showing");
        recordAssertion ("custom_editor_bounds_nonempty", ! editorBounds.isEmpty(), "Custom editor bounds are empty");

        Array<PianoKey*> visiblePianoKeys;
        if (editor != nullptr)
        {
            visitComponents (*editor, [&] (Component& component)
            {
                if (auto* key = dynamic_cast<PianoKey*> (&component); key != nullptr && key->isShowing())
                    visiblePianoKeys.add (key);
            });
        }
        assertions->setProperty ("visible_piano_key_count", visiblePianoKeys.size());
        if (visiblePianoKeys.isEmpty())
            failures.add ("No visible PianoKey components were found");

        bool noteRoundTrip = false;
        bool keyReleased = false;
        if (auto* key = visiblePianoKeys.getFirst())
        {
            key->triggerNoteOn();
            const auto pressed = key->isCurrentlyPressed();
            key->triggerNoteOff();
            keyReleased = ! key->isCurrentlyPressed();
            noteRoundTrip = pressed && keyReleased;
        }
        recordAssertion ("piano_note_round_trip", noteRoundTrip, "Visible piano key note-on/note-off round trip failed");
        recordAssertion ("piano_key_released", keyReleased, "Visible piano key did not return to released state");

        const auto status = hasWindow ? mainWindow->getStatus() : String{};
        recordAssertion ("status_nonempty", status.isNotEmpty(), "Standalone device status text is empty");
        auto* manager = hasWindow ? &mainWindow->pluginHolder->deviceManager : nullptr;
        const auto midiCount = manager != nullptr ? countEnabledMidiInputs (*manager) : -1;
        const auto* currentDevice = manager != nullptr ? manager->getCurrentAudioDevice() : nullptr;
        const auto stateFragment = "MIDI inputs enabled: " + String (midiCount);
        recordAssertion ("status_matches_device_state",
                         status.contains (stateFragment)
                             && ((currentDevice != nullptr && status.contains (currentDevice->getName()))
                                 || (currentDevice == nullptr && (status.containsIgnoreCase ("unavailable")
                                                                  || status.containsIgnoreCase ("disabled")))),
                         "Standalone status text does not match the resulting audio/MIDI state");

        const auto invalidMode = lifecycle.mode == LifecycleOptions::Mode::invalid;
        const auto noDeviceMode = lifecycle.mode == LifecycleOptions::Mode::noDevice;
        recordAssertion ("invalid_error_nonempty", ! invalidMode || deviceOpenError.isNotEmpty(), "Invalid mode did not record a device-open error");
        recordAssertion ("required_current_device_absent", (! invalidMode && ! noDeviceMode) || currentDevice == nullptr,
                         "Invalid/no-device mode unexpectedly opened an audio device");
        recordAssertion ("no_device_midi_inputs_disabled", ! noDeviceMode || midiCount == 0,
                         "No-device mode left one or more MIDI inputs enabled");

        bool pngValid = false;
        bool pngDimensionsMatch = false;
        String screenshotError;
        if (hasWindow && ! windowBounds.isEmpty())
        {
            const auto image = mainWindow->createComponentSnapshot (mainWindow->getLocalBounds(), true);
            lifecycle.screenshot.getParentDirectory().createDirectory();
            if (auto stream = lifecycle.screenshot.createOutputStream())
            {
                PNGImageFormat png;
                if (png.writeImageToStream (image, *stream))
                {
                    stream->flush();
                    const auto decoded = ImageFileFormat::loadFrom (lifecycle.screenshot);
                    pngValid = decoded.isValid() && lifecycle.screenshot.getSize() > 0;
                    pngDimensionsMatch = pngValid
                        && decoded.getWidth() == windowBounds.getWidth()
                        && decoded.getHeight() == windowBounds.getHeight();
                }
            }
        }
        if (! pngValid)
            screenshotError = "PNG screenshot is missing or invalid";
        recordAssertion ("png_valid_nonempty", pngValid, screenshotError);
        recordAssertion ("png_dimensions_match", pngDimensionsMatch, "PNG dimensions do not match the reported window dimensions");

        auto report = DynamicObject::Ptr (new DynamicObject());
        report->setProperty ("schema_version", 1);
        report->setProperty ("tool_version", JucePlugin_VersionString);
        report->setProperty ("mode", lifecycle.modeName());
        report->setProperty ("status", failures.isEmpty() ? "pass" : "fail");
        report->setProperty ("first_visible_ms", firstVisibleMs);
        report->setProperty ("device_configuration_start_ms", deviceConfigurationStartMs);

        auto window = DynamicObject::Ptr (new DynamicObject());
        window->setProperty ("width", windowBounds.getWidth());
        window->setProperty ("height", windowBounds.getHeight());
        report->setProperty ("window", var (window.get()));

        auto device = DynamicObject::Ptr (new DynamicObject());
        device->setProperty ("current_device", currentDevice != nullptr ? currentDevice->getName() : String{});
        device->setProperty ("open_error", deviceOpenError);
        device->setProperty ("enabled_midi_input_count", midiCount);
        device->setProperty ("status_text", status);
        report->setProperty ("device", var (device.get()));
        report->setProperty ("assertions", var (assertions.get()));

        Array<var> failureValues;
        for (const auto& failure : failures)
            failureValues.add (failure);
        report->setProperty ("failures", failureValues);

        lifecycle.report.getParentDirectory().createDirectory();
        const auto reportText = JSON::toString (var (report.get()), true) + newLine;
        const auto reportWritten = lifecycle.report.replaceWithText (reportText, false, false, "\n");
        if (! reportWritten)
        {
            Logger::writeToLog ("Unable to write lifecycle diagnostic report: " + lifecycle.report.getFullPathName());
            failures.add ("Diagnostic report could not be written");
        }

        setApplicationReturnValue (failures.isEmpty() ? 0 : 1);
        quit();
    }

    ApplicationProperties applicationProperties;
    std::unique_ptr<LifecycleStandaloneWindow> mainWindow;
    LifecycleOptions lifecycle;
    TimerPhase timerPhase = TimerPhase::configureDevice;
    double applicationStartMs = 0.0;
    double firstVisibleMs = -1.0;
    double deviceConfigurationStartMs = -1.0;
    String deviceOpenError;
    String failureBeforeWindow;
};
}

JUCE_CREATE_APPLICATION_DEFINE (ModelDStandaloneApplication)
