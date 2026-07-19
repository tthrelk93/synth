#include <JuceHeader.h>
#include <juce_audio_plugin_client/detail/juce_CreatePluginFilter.h>

#include "PianoKey.h"
#include "PresetManager.h"

namespace
{
using namespace juce;

#if SYNTH_ENABLE_STANDALONE_LIFECYCLE_TESTS
constexpr auto lifecycleFlag = "--synth-lifecycle-test";
#endif
constexpr auto invalidDeviceName = "ModelD lifecycle test - deliberately nonexistent output";

struct LifecycleOptions
{
    enum class Mode { normal, invalid, noDevice };

    Mode mode = Mode::normal;
    File report;
    File screenshot;
    File settings;
    File presetDirectory;
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
    if (arguments.size() != 10
        || arguments[0] != lifecycleFlag
        || arguments[2] != "--report"
        || arguments[4] != "--screenshot"
        || arguments[6] != "--settings"
        || arguments[8] != "--preset-dir")
    {
        result.parseError = "Expected --synth-lifecycle-test <normal|invalid|no-device> --report <absolute-path> --screenshot <absolute-path> --settings <absolute-path> --preset-dir <absolute-path>";
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

    if (! File::isAbsolutePath (arguments[3])
        || ! File::isAbsolutePath (arguments[5])
        || ! File::isAbsolutePath (arguments[7])
        || ! File::isAbsolutePath (arguments[9]))
    {
        result.parseError = "Lifecycle report, screenshot, settings, and preset paths must be absolute";
        return result;
    }

    result.report = File (arguments[3]);
    result.screenshot = File (arguments[5]);
    result.settings = File (arguments[7]);
    result.presetDirectory = File (arguments[9]);
   #endif

    return result;
}

static PropertiesFile::Options makeSettingsOptions()
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
    return options;
}

template <typename Visitor>
static void visitComponents (Component& parent, Visitor&& visitor)
{
    visitor (parent);
    for (auto* child : parent.getChildren())
        visitComponents (*child, visitor);
}

class DeferredAudioDeviceManager final : public AudioDeviceManager
{
public:
    void setSequenceRecorder (std::function<int()> recorder)
    {
        sequenceRecorder = std::move (recorder);
    }

    void createAudioDeviceTypes (OwnedArray<AudioIODeviceType>& types) override
    {
        ++audioDeviceDiscoveryCount;
        if (firstAudioDeviceDiscoverySequence == 0 && sequenceRecorder)
            firstAudioDeviceDiscoverySequence = sequenceRecorder();
        AudioDeviceManager::createAudioDeviceTypes (types);
    }

    int audioDeviceDiscoveryCount = 0;
    int firstAudioDeviceDiscoverySequence = 0;

private:
    std::function<int()> sequenceRecorder;
};

class StandaloneActions
{
public:
    virtual ~StandaloneActions() = default;
    virtual void showAudioMidiSettings() = 0;
    virtual void askUserToSaveState() = 0;
    virtual void askUserToLoadState() = 0;
    virtual void resetPluginState() = 0;
    virtual void requestApplicationQuit() = 0;
};

class EditorContent final : public Component,
                            private ComponentListener
{
public:
    explicit EditorContent (AudioProcessor& processor)
        : processor (processor),
          editor (processor.hasEditor() ? processor.createEditorIfNeeded()
                                        : new GenericAudioProcessorEditor (processor))
    {
        if (editor != nullptr)
        {
            editor->addComponentListener (this);
            handleEditorMovedOrResized();
            addAndMakeVisible (*editor);
        }
    }

    ~EditorContent() override
    {
        if (editor != nullptr)
        {
            editor->removeComponentListener (this);
            processor.editorBeingDeleted (editor.get());
        }
        editor.reset();
    }

    void resized() override { handleContentResized(); }

    AudioProcessorEditor* getEditor() const noexcept { return editor.get(); }

    ComponentBoundsConstrainer* getEditorConstrainer() const noexcept
    {
        return editor != nullptr ? editor->getConstrainer() : nullptr;
    }

private:
    void handleContentResized()
    {
        if (editor == nullptr)
            return;

        const auto contentBounds = getLocalBounds();
        const auto newPosition = contentBounds.getTopLeft().toFloat()
                                     .transformedBy (editor->getTransform().inverted())
                                     .roundToInt();
        if (preventResizingEditor)
            editor->setTopLeftPosition (newPosition);
        else
            editor->setBoundsConstrained (
                editor->getLocalArea (this, contentBounds.toFloat())
                    .withPosition (newPosition.toFloat()).toNearestInt());
    }

    void handleEditorMovedOrResized()
    {
        const ScopedValueSetter<bool> scope (preventResizingEditor, true);
        if (editor != nullptr)
        {
            const auto editorArea = getLocalArea (editor.get(), editor->getLocalBounds());
            setSize (editorArea.getWidth(), editorArea.getHeight());
        }
    }

    void componentMovedOrResized (Component&, bool, bool) override
    {
        handleEditorMovedOrResized();
    }

    AudioProcessor& processor;
    std::unique_ptr<AudioProcessorEditor> editor;
    bool preventResizingEditor = false;
};

class StandaloneDecoratorConstrainer final : public BorderedComponentBoundsConstrainer
{
public:
    ComponentBoundsConstrainer* getWrappedConstrainer() const override
    {
        return content != nullptr ? content->getEditorConstrainer() : nullptr;
    }

    BorderSize<int> getAdditionalBorder() const override
    {
        if (window == nullptr)
            return {};

        const auto nativeFrame = [&]() -> BorderSize<int>
        {
            if (auto* peer = window->getPeer())
                if (const auto frame = peer->getFrameSizeIfPresent())
                    return *frame;
            return {};
        }();
        return nativeFrame.addedTo (window->getContentComponentBorder());
    }

    void setWindow (DocumentWindow* windowIn) { window = windowIn; }
    void setEditorContent (EditorContent* contentIn) { content = contentIn; }

private:
    DocumentWindow* window = nullptr;
    EditorContent* content = nullptr;
};

class ProjectStandaloneWindow final : public DocumentWindow
{
public:
    ProjectStandaloneWindow (const String& title,
                             Colour background,
                             StandaloneActions& actionsIn,
                             PropertiesFile& settingsIn)
        : DocumentWindow (title, background,
                          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          actions (actionsIn), settings (settingsIn), optionsButton ("Options")
    {
        decoratorConstrainer.setWindow (this);
        setConstrainer (&decoratorConstrainer);
        setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, false);
        optionsButton.setTriggeredOnMouseDown (true);
        optionsButton.onClick = [safe = SafePointer<ProjectStandaloneWindow> (this)]
        {
            if (safe != nullptr)
                safe->showOptionsMenu();
        };
        Component::addAndMakeVisible (&optionsButton);

        statusLabel.setJustificationType (Justification::centredLeft);
        statusLabel.setColour (Label::textColourId,
                               getLookAndFeel().findColour (DocumentWindow::textColourId));
        statusLabel.setInterceptsMouseClicks (false, false);
        Component::addAndMakeVisible (&statusLabel);
        setStatus ("Audio: initialising | MIDI inputs enabled: 0");
    }

    ~ProjectStandaloneWindow() override
    {
        saveWindowPosition();
        clearProcessorEditor();
    }

    void attachProcessorEditor (AudioProcessor& processor)
    {
        auto content = std::make_unique<EditorContent> (processor);
        editorContent = content.get();
        decoratorConstrainer.setEditorContent (editorContent);
        setContentOwned (content.release(), true);
        if (auto* editor = getEditor())
            setResizable (editor->isResizable(), false);
        restoreWindowPosition();
    }

    void clearProcessorEditor()
    {
        decoratorConstrainer.setEditorContent (nullptr);
        editorContent = nullptr;
        clearContentComponent();
    }

    AudioProcessorEditor* getEditor() const noexcept
    {
        return editorContent != nullptr ? editorContent->getEditor() : nullptr;
    }

    ComponentBoundsConstrainer* getEditorConstrainer() const noexcept
    {
        return editorContent != nullptr ? editorContent->getEditorConstrainer() : nullptr;
    }

    Rectangle<int> getEditorContentBounds() const noexcept
    {
        return editorContent != nullptr ? editorContent->getBounds() : Rectangle<int>{};
    }

    bool isDecoratorConstrainerActive() noexcept
    {
        return getConstrainer() == &decoratorConstrainer;
    }

    void resized() override
    {
        DocumentWindow::resized();
        optionsButton.setBounds (8, 6, 60, jmax (0, getTitleBarHeight() - 8));
        statusLabel.setBounds (76, 2, jmax (0, getWidth() - 84),
                               jmax (0, getTitleBarHeight() - 4));
    }

    void closeButtonPressed() override { actions.requestApplicationQuit(); }

    void setStatus (const String& status)
    {
        statusLabel.setText (status, dontSendNotification);
        statusLabel.setTooltip (status);
    }

    String getStatus() const { return statusLabel.getText(); }
    bool isStatusLabelShowing() const { return statusLabel.isShowing(); }
    Rectangle<int> getStatusLabelBounds() const { return statusLabel.getBounds(); }

    void saveWindowPosition()
    {
        settings.setValue ("windowX", getX());
        settings.setValue ("windowY", getY());
    }

private:
    void restoreWindowPosition()
    {
        const auto& displays = Desktop::getInstance().getDisplays();
        if (displays.displays.isEmpty())
            return;

        constexpr int missing = -100000;
        const auto savedX = settings.getIntValue ("windowX", missing);
        const auto savedY = settings.getIntValue ("windowY", missing);
        Rectangle<int> requested;
        if (savedX != missing && savedY != missing)
            requested = { savedX, savedY, getWidth(), getHeight() };
        else
            requested = getLocalBounds().withCentre (displays.getPrimaryDisplay()->userArea.getCentre());

        const auto limits = displays.getDisplayForRect (requested)->userArea;
        requested.setPosition (jlimit (limits.getX(), jmax (limits.getX(), limits.getRight() - requested.getWidth()), requested.getX()),
                               jlimit (limits.getY(), jmax (limits.getY(), limits.getBottom() - requested.getHeight()), requested.getY()));
        setBoundsConstrained (requested);
    }

    void showOptionsMenu()
    {
        PopupMenu menu;
        menu.addItem (1, TRANS ("Audio/MIDI Settings..."));
        menu.addSeparator();
        menu.addItem (2, TRANS ("Save current state..."));
        menu.addItem (3, TRANS ("Load a saved state..."));
        menu.addSeparator();
        menu.addItem (4, TRANS ("Reset to default state"));
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&optionsButton),
                            [safe = SafePointer<ProjectStandaloneWindow> (this)] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1) safe->actions.showAudioMidiSettings();
                                if (result == 2) safe->actions.askUserToSaveState();
                                if (result == 3) safe->actions.askUserToLoadState();
                                if (result == 4) safe->actions.resetPluginState();
                            });
    }

    StandaloneActions& actions;
    PropertiesFile& settings;
    TextButton optionsButton;
    Label statusLabel;
    EditorContent* editorContent = nullptr;
    StandaloneDecoratorConstrainer decoratorConstrainer;
};

class AudioMidiSettingsComponent final : public Component
{
public:
    AudioMidiSettingsComponent (AudioDeviceManager& manager, AudioProcessor& processor)
        : selector (manager, 0, 0, 0, 2, true, processor.producesMidi(), true, false)
    {
        addAndMakeVisible (selector);
        setSize (500, 550);
    }

    void resized() override { selector.setBounds (getLocalBounds()); }

private:
    AudioDeviceSelectorComponent selector;
};

class ModelDStandaloneApplication final : public JUCEApplication,
                                          private Timer,
                                          private ChangeListener,
                                          private StandaloneActions
{
public:
    const String getApplicationName() override { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const String&) override {}

    void initialise (const String&) override
    {
        lifecycle = parseLifecycleOptions();
        if (lifecycle.enabled && lifecycle.parseError.isNotEmpty())
        {
            writeEarlyFailure (lifecycle.parseError);
            return;
        }

        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            writeEarlyFailure ("No display is available for the standalone editor");
            return;
        }

        const auto options = makeSettingsOptions();
        if (lifecycle.enabled)
        {
            lifecycle.settings.getParentDirectory().createDirectory();
            settings = std::make_unique<PropertiesFile> (lifecycle.settings, options);
        }
        else
        {
            settings = std::make_unique<PropertiesFile> (options);
        }

        if (! settings->isValidFile())
        {
            writeEarlyFailure ("Standalone settings file is invalid: " + settings->getFile().getFullPathName());
            return;
        }

        if (lifecycle.enabled
            && ! PresetManager::setStandaloneLifecycleTestDirectory (lifecycle.presetDirectory))
        {
            writeEarlyFailure ("Standalone preset directory is invalid: "
                               + lifecycle.presetDirectory.getFullPathName());
            return;
        }

        deviceManager.setSequenceRecorder ([this] { return nextSequence(); });
        deviceManager.addChangeListener (this);
        createPlugin();

        mainWindow = std::make_unique<ProjectStandaloneWindow> (
            getApplicationName(),
            LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
            static_cast<StandaloneActions&> (*this), *settings);
        mainWindow->attachProcessorEditor (*processor);
        mainWindow->setVisible (true);
        mainWindow->toFront (false);
        windowVisibleSequence = nextSequence();

        startTimer (50);
    }

    void shutdown() override
    {
        stopTimer();
        if (mainWindow != nullptr)
            mainWindow->saveWindowPosition();
        savePluginState();
        saveAudioDeviceState();
        mainWindow = nullptr;
        stopAudioAndMidiCallbacks();
        player.setProcessor (nullptr);
        processor = nullptr;
        deviceManager.removeChangeListener (this);
        if (settings != nullptr)
            settings->saveIfNeeded();
        settings = nullptr;
    }

    void systemRequestedQuit() override
    {
        savePluginState();
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
    enum class TimerPhase { configureDevice, captureEvidence };

    int nextSequence() { return ++logicalSequence; }

    void createPlugin()
    {
        processor = createPluginFilterOfType (AudioProcessor::wrapperType_Standalone);
        processor->disableNonMainBuses();
        processor->setRateAndBufferSizeDetails (44100.0, 512);
        reloadPluginState();
        player.setProcessor (processor.get());
    }

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
            startTimer (250);
            return;
        }

        finishLifecycleTest ({});
    }

    void configureDeviceAfterWindowIsVisible()
    {
        deviceConfigurationStartSequence = nextSequence();
        if (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::noDevice)
        {
            deviceOpenError.clear();
            enabledMidiInputCount = 0;
            updateStatusText();
            return;
        }

        deviceManager.addAudioCallback (&player);
        deviceManager.addMidiInputDeviceCallback ({}, &player);
        callbacksWired = true;

        if (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::invalid)
        {
            AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName.clear();
            setup.outputDeviceName = invalidDeviceName;
            deviceOpenError = deviceManager.initialise (0, 2, nullptr, false, {}, &setup);
        }
        else
        {
            auto savedState = settings->getXmlValue ("audioSetup");
            deviceOpenError = deviceManager.initialise (0, 2, savedState.get(), true);
        }

        refreshEnabledMidiInputCount();
        updateStatusText();
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &deviceManager)
        {
            if (! (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::noDevice))
                refreshEnabledMidiInputCount();
            updateStatusText();
        }
    }

    void refreshEnabledMidiInputCount()
    {
        midiEnumerationPerformed = true;
        enabledMidiInputCount = 0;
        for (const auto& device : MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled (device.identifier))
                ++enabledMidiInputCount;
    }

    void updateStatusText()
    {
        if (mainWindow == nullptr)
            return;
        if (lifecycle.enabled && lifecycle.mode == LifecycleOptions::Mode::noDevice)
        {
            mainWindow->setStatus ("Audio: disabled (no-device test) | MIDI inputs enabled: 0");
            return;
        }
        if (const auto* device = deviceManager.getCurrentAudioDevice())
        {
            mainWindow->setStatus ("Audio: " + device->getName()
                                   + " | MIDI inputs enabled: " + String (enabledMidiInputCount));
            return;
        }
        const auto reason = deviceOpenError.isNotEmpty() ? deviceOpenError
                                                         : String ("no output device available");
        mainWindow->setStatus ("Audio unavailable: " + reason
                               + " | MIDI inputs enabled: " + String (enabledMidiInputCount));
    }

    void savePluginState()
    {
        if (settings == nullptr || processor == nullptr)
            return;
        MemoryBlock data;
        processor->getStateInformation (data);
        settings->setValue ("filterState", data.toBase64Encoding());
    }

    void reloadPluginState()
    {
        if (settings == nullptr || processor == nullptr)
            return;
        MemoryBlock data;
        if (data.fromBase64Encoding (settings->getValue ("filterState")) && data.getSize() > 0)
            processor->setStateInformation (data.getData(), (int) data.getSize());
    }

    void saveAudioDeviceState()
    {
        if (settings != nullptr && callbacksWired)
            settings->setValue ("audioSetup", deviceManager.createStateXml().get());
    }

    void stopAudioAndMidiCallbacks()
    {
        if (! callbacksWired)
            return;
        deviceManager.removeMidiInputDeviceCallback ({}, &player);
        deviceManager.removeAudioCallback (&player);
        callbacksWired = false;
    }

    File getLastStateFile() const
    {
        auto file = File (settings->getValue ("lastStateFile"));
        return file == File() ? File::getSpecialLocation (File::userDocumentsDirectory) : file;
    }

    void showAudioMidiSettings() override
    {
        if (processor == nullptr)
            return;
        DialogWindow::LaunchOptions options;
        options.content.setOwned (new AudioMidiSettingsComponent (deviceManager, *processor));
        options.dialogTitle = TRANS ("Audio/MIDI Settings");
        options.dialogBackgroundColour = options.content->getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    void askUserToSaveState() override
    {
        stateFileChooser = std::make_unique<FileChooser> (TRANS ("Save current state"), getLastStateFile());
        stateFileChooser->launchAsync (FileBrowserComponent::saveMode
                                           | FileBrowserComponent::canSelectFiles
                                           | FileBrowserComponent::warnAboutOverwriting,
                                       [this] (const FileChooser& chooser)
                                       {
                                           const auto file = chooser.getResult();
                                           if (file == File() || processor == nullptr)
                                               return;
                                           settings->setValue ("lastStateFile", file.getFullPathName());
                                           MemoryBlock data;
                                           processor->getStateInformation (data);
                                           if (! file.replaceWithData (data.getData(), data.getSize()))
                                               showFileError (TRANS ("Couldn't write to the specified file!"));
                                       });
    }

    void askUserToLoadState() override
    {
        stateFileChooser = std::make_unique<FileChooser> (TRANS ("Load a saved state"), getLastStateFile());
        stateFileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                       [this] (const FileChooser& chooser)
                                       {
                                           const auto file = chooser.getResult();
                                           if (file == File() || processor == nullptr)
                                               return;
                                           settings->setValue ("lastStateFile", file.getFullPathName());
                                           MemoryBlock data;
                                           if (file.loadFileAsData (data))
                                               processor->setStateInformation (data.getData(), (int) data.getSize());
                                           else
                                               showFileError (TRANS ("Couldn't read from the specified file!"));
                                       });
    }

    void showFileError (const String& message)
    {
        const auto options = MessageBoxOptions::makeOptionsOk (AlertWindow::WarningIcon,
                                                               TRANS ("Standalone state error"), message);
        messageBox = AlertWindow::showScopedAsync (options, nullptr);
    }

    void resetPluginState() override
    {
        if (mainWindow == nullptr)
            return;
        player.setProcessor (nullptr);
        mainWindow->clearProcessorEditor();
        settings->removeValue ("filterState");
        processor = nullptr;
        createPlugin();
        mainWindow->attachProcessorEditor (*processor);
    }

    void requestApplicationQuit() override { systemRequestedQuit(); }

    void writeEarlyFailure (const String& error)
    {
        Logger::writeToLog (error);
        if (lifecycle.enabled && lifecycle.report != File())
        {
            auto report = DynamicObject::Ptr (new DynamicObject());
            report->setProperty ("schema_version", 1);
            report->setProperty ("tool_version", JucePlugin_VersionString);
            report->setProperty ("mode", lifecycle.modeName());
            report->setProperty ("status", "fail");
            report->setProperty ("failures", Array<var> { error });
            lifecycle.report.getParentDirectory().createDirectory();
            lifecycle.report.replaceWithText (JSON::toString (var (report.get()), true) + newLine,
                                               false, false, "\n");
        }
        setApplicationReturnValue (1);
        quit();
    }

    void finishLifecycleTest (const String& preconditionFailure)
    {
        if (! lifecycle.enabled)
            return;

        StringArray failures;
        if (preconditionFailure.isNotEmpty())
            failures.add (preconditionFailure);
        auto assertions = DynamicObject::Ptr (new DynamicObject());
        const auto assertThat = [&] (const Identifier& name, bool passed, const String& failure)
        {
            assertions->setProperty (name, passed);
            if (! passed)
                failures.add (failure);
        };

        const auto hasWindow = mainWindow != nullptr;
        const auto originalWindowBounds = hasWindow ? mainWindow->getBounds() : Rectangle<int>{};
        assertThat ("top_level_visible", hasWindow && mainWindow->isVisible(), "Top-level window is not visible");
        assertThat ("top_level_showing", hasWindow && mainWindow->isShowing(), "Top-level window is not showing");
        assertThat ("native_peer_present", hasWindow && mainWindow->getPeer() != nullptr, "Top-level window has no native peer");
        assertThat ("window_bounds_nonempty", ! originalWindowBounds.isEmpty(), "Top-level window bounds are empty");
        assertThat ("visible_before_device_configuration",
                    windowVisibleSequence > 0 && deviceConfigurationStartSequence > windowVisibleSequence,
                    "Device configuration did not begin after first visibility");
        assertThat ("device_discovery_after_visibility",
                    deviceManager.firstAudioDeviceDiscoverySequence == 0
                        || deviceManager.firstAudioDeviceDiscoverySequence > windowVisibleSequence,
                    "Audio device discovery began before first visibility");

        auto* editor = hasWindow ? mainWindow->getEditor() : nullptr;
        const auto editorBounds = editor != nullptr ? editor->getBounds() : Rectangle<int>{};
        auto* editorConstrainer = hasWindow ? mainWindow->getEditorConstrainer() : nullptr;
        const auto minimumEditorWidth = editorConstrainer != nullptr ? editorConstrainer->getMinimumWidth() : 0;
        const auto minimumEditorHeight = editorConstrainer != nullptr ? editorConstrainer->getMinimumHeight() : 0;
        const auto maximumEditorWidth = editorConstrainer != nullptr ? editorConstrainer->getMaximumWidth() : 0;
        const auto maximumEditorHeight = editorConstrainer != nullptr ? editorConstrainer->getMaximumHeight() : 0;
        const auto editorConstrainerLimitsValid = editorConstrainer != nullptr
            && minimumEditorWidth > 0 && minimumEditorHeight > 0
            && maximumEditorWidth >= minimumEditorWidth
            && maximumEditorHeight >= minimumEditorHeight
            && editorBounds.getWidth() >= minimumEditorWidth
            && editorBounds.getWidth() <= maximumEditorWidth
            && editorBounds.getHeight() >= minimumEditorHeight
            && editorBounds.getHeight() <= maximumEditorHeight;

        const auto chooseResizeDimension = [] (int current, int minimum, int maximum)
        {
            const auto larger = jmin (maximum, current + 16);
            return larger != current ? larger : jmax (minimum, current - 16);
        };
        const auto requestedEditorWidth = editorConstrainerLimitsValid
            ? chooseResizeDimension (editorBounds.getWidth(), minimumEditorWidth, maximumEditorWidth)
            : editorBounds.getWidth();
        const auto requestedEditorHeight = editorConstrainerLimitsValid
            ? chooseResizeDimension (editorBounds.getHeight(), minimumEditorHeight, maximumEditorHeight)
            : editorBounds.getHeight();

        Rectangle<int> resizedWindowBounds;
        Rectangle<int> resizedContentBounds;
        Rectangle<int> restoredWindowBounds;
        Rectangle<int> restoredContentBounds;
        Rectangle<int> resizedEditorBounds;
        Rectangle<int> restoredEditorBounds;
        bool editorResizePropagatedToWindow = false;
        bool editorResizeRoundTrip = false;
        const auto rectangleSize = [] (const Rectangle<int>& rectangle)
        {
            return Point<int> (rectangle.getWidth(), rectangle.getHeight());
        };
        if (editor != nullptr && hasWindow
            && (requestedEditorWidth != editorBounds.getWidth()
                || requestedEditorHeight != editorBounds.getHeight()))
        {
            editor->setSize (requestedEditorWidth, requestedEditorHeight);
            resizedEditorBounds = editor->getBounds();
            resizedContentBounds = mainWindow->getEditorContentBounds();
            resizedWindowBounds = mainWindow->getBounds();
            editorResizePropagatedToWindow = rectangleSize (resizedEditorBounds)
                                                  == Point<int> (requestedEditorWidth, requestedEditorHeight)
                && rectangleSize (resizedContentBounds) == rectangleSize (resizedEditorBounds)
                && resizedWindowBounds.getWidth() - originalWindowBounds.getWidth()
                       == requestedEditorWidth - editorBounds.getWidth()
                && resizedWindowBounds.getHeight() - originalWindowBounds.getHeight()
                       == requestedEditorHeight - editorBounds.getHeight();

            editor->setSize (editorBounds.getWidth(), editorBounds.getHeight());
            restoredEditorBounds = editor->getBounds();
            restoredContentBounds = mainWindow->getEditorContentBounds();
            restoredWindowBounds = mainWindow->getBounds();
            editorResizeRoundTrip = rectangleSize (restoredEditorBounds) == rectangleSize (editorBounds)
                && rectangleSize (restoredContentBounds) == rectangleSize (editorBounds)
                && rectangleSize (restoredWindowBounds) == rectangleSize (originalWindowBounds);
        }

        assertThat ("custom_editor_visible", editor != nullptr && editor->isVisible(), "Custom editor is not visible");
        assertThat ("custom_editor_showing", editor != nullptr && editor->isShowing(), "Custom editor is not showing");
        assertThat ("custom_editor_bounds_nonempty", ! editorBounds.isEmpty(), "Custom editor bounds are empty");
        assertThat ("editor_constrainer_present", editorConstrainer != nullptr, "Custom editor has no bounds constrainer");
        assertThat ("editor_constrainer_limits_valid", editorConstrainerLimitsValid, "Custom editor constrainer limits are invalid");
        assertThat ("window_decorator_constrainer_active",
                    hasWindow && mainWindow->isDecoratorConstrainerActive(),
                    "Standalone window is not using the decorated editor constrainer");
        assertThat ("editor_resize_propagated_to_window", editorResizePropagatedToWindow,
                    "Editor-driven resize did not propagate to content and window bounds");
        assertThat ("editor_resize_round_trip", editorResizeRoundTrip,
                    "Editor-driven resize did not round-trip to the original bounds");
        assertThat ("status_label_showing", hasWindow && mainWindow->isStatusLabelShowing(), "Status label is not showing");
        assertThat ("status_label_bounds_nonempty", hasWindow && ! mainWindow->getStatusLabelBounds().isEmpty(), "Status label bounds are empty");

        Array<PianoKey*> visiblePianoKeys;
        if (editor != nullptr)
            visitComponents (*editor, [&] (Component& component)
            {
                if (auto* key = dynamic_cast<PianoKey*> (&component); key != nullptr && key->isShowing())
                    visiblePianoKeys.add (key);
            });
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
        assertThat ("piano_note_round_trip", noteRoundTrip, "Visible piano key note round trip failed");
        assertThat ("piano_key_released", keyReleased, "Visible piano key did not return to released state");

        const auto status = hasWindow ? mainWindow->getStatus() : String{};
        const auto* currentDevice = deviceManager.getCurrentAudioDevice();
        assertThat ("status_nonempty", status.isNotEmpty(), "Standalone device status text is empty");
        assertThat ("status_matches_device_state",
                    status.contains ("MIDI inputs enabled: " + String (enabledMidiInputCount))
                        && ((currentDevice != nullptr && status.contains (currentDevice->getName()))
                            || (currentDevice == nullptr && (status.containsIgnoreCase ("unavailable")
                                                             || status.containsIgnoreCase ("disabled")))),
                    "Standalone status text does not match audio/MIDI state");

        const auto invalidMode = lifecycle.mode == LifecycleOptions::Mode::invalid;
        const auto noDeviceMode = lifecycle.mode == LifecycleOptions::Mode::noDevice;
        assertThat ("invalid_error_nonempty", ! invalidMode || deviceOpenError.isNotEmpty(), "Invalid mode did not record a device-open error");
        assertThat ("required_current_device_absent", (! invalidMode && ! noDeviceMode) || currentDevice == nullptr, "Invalid/no-device mode opened an audio device");
        assertThat ("no_device_midi_inputs_disabled", ! noDeviceMode || enabledMidiInputCount == 0, "No-device mode enabled MIDI inputs");
        assertThat ("no_device_discovery_skipped",
                    ! noDeviceMode || (deviceManager.audioDeviceDiscoveryCount == 0
                                       && ! midiEnumerationPerformed && ! callbacksWired),
                    "No-device mode performed audio/MIDI discovery or wired callbacks");

        const auto actualSettingsPath = settings != nullptr ? settings->getFile().getFullPathName() : String{};
        const auto settingsPathMatches = actualSettingsPath == lifecycle.settings.getFullPathName();
        assertThat ("settings_path_matches_request", settingsPathMatches,
                    "Standalone did not use the explicit lifecycle settings path");
        const auto actualPresetDirectory = PresetManager::getLastConstructedPresetDirectory().getFullPathName();
        const auto presetDirectoryMatches = actualPresetDirectory == lifecycle.presetDirectory.getFullPathName();
        assertThat ("preset_directory_matches_request", presetDirectoryMatches,
                    "Standalone editor did not use the explicit lifecycle preset directory");

        bool pngValid = false;
        bool pngDimensionsMatch = false;
        const auto windowBounds = hasWindow ? mainWindow->getBounds() : Rectangle<int>{};
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
        assertThat ("png_valid_nonempty", pngValid, "PNG screenshot is missing or invalid");
        assertThat ("png_dimensions_match", pngDimensionsMatch, "PNG dimensions do not match the window");

        auto report = DynamicObject::Ptr (new DynamicObject());
        report->setProperty ("schema_version", 1);
        report->setProperty ("tool_version", JucePlugin_VersionString);
        report->setProperty ("mode", lifecycle.modeName());
        report->setProperty ("status", failures.isEmpty() ? "pass" : "fail");
        auto sequence = DynamicObject::Ptr (new DynamicObject());
        sequence->setProperty ("window_visible", windowVisibleSequence);
        sequence->setProperty ("device_configuration_start", deviceConfigurationStartSequence);
        sequence->setProperty ("first_audio_device_discovery", deviceManager.firstAudioDeviceDiscoverySequence);
        report->setProperty ("sequence", var (sequence.get()));
        auto window = DynamicObject::Ptr (new DynamicObject());
        window->setProperty ("width", windowBounds.getWidth());
        window->setProperty ("height", windowBounds.getHeight());
        report->setProperty ("window", var (window.get()));
        auto resize = DynamicObject::Ptr (new DynamicObject());
        resize->setProperty ("minimum_editor_width", minimumEditorWidth);
        resize->setProperty ("minimum_editor_height", minimumEditorHeight);
        resize->setProperty ("maximum_editor_width", maximumEditorWidth);
        resize->setProperty ("maximum_editor_height", maximumEditorHeight);
        resize->setProperty ("original_editor_width", editorBounds.getWidth());
        resize->setProperty ("original_editor_height", editorBounds.getHeight());
        resize->setProperty ("requested_editor_width", requestedEditorWidth);
        resize->setProperty ("requested_editor_height", requestedEditorHeight);
        resize->setProperty ("resized_editor_width", resizedEditorBounds.getWidth());
        resize->setProperty ("resized_editor_height", resizedEditorBounds.getHeight());
        resize->setProperty ("resized_content_width", resizedContentBounds.getWidth());
        resize->setProperty ("resized_content_height", resizedContentBounds.getHeight());
        resize->setProperty ("original_window_width", originalWindowBounds.getWidth());
        resize->setProperty ("original_window_height", originalWindowBounds.getHeight());
        resize->setProperty ("resized_window_width", resizedWindowBounds.getWidth());
        resize->setProperty ("resized_window_height", resizedWindowBounds.getHeight());
        resize->setProperty ("restored_editor_width", restoredEditorBounds.getWidth());
        resize->setProperty ("restored_editor_height", restoredEditorBounds.getHeight());
        resize->setProperty ("restored_content_width", restoredContentBounds.getWidth());
        resize->setProperty ("restored_content_height", restoredContentBounds.getHeight());
        resize->setProperty ("restored_window_width", restoredWindowBounds.getWidth());
        resize->setProperty ("restored_window_height", restoredWindowBounds.getHeight());
        report->setProperty ("resize", var (resize.get()));
        auto discovery = DynamicObject::Ptr (new DynamicObject());
        discovery->setProperty ("audio_device_discovery_count", deviceManager.audioDeviceDiscoveryCount);
        discovery->setProperty ("midi_enumeration_performed", midiEnumerationPerformed);
        discovery->setProperty ("callbacks_wired", callbacksWired);
        report->setProperty ("discovery", var (discovery.get()));
        auto device = DynamicObject::Ptr (new DynamicObject());
        device->setProperty ("current_device", currentDevice != nullptr ? currentDevice->getName() : String{});
        device->setProperty ("open_error", deviceOpenError);
        device->setProperty ("enabled_midi_input_count", enabledMidiInputCount);
        device->setProperty ("status_text", status);
        report->setProperty ("device", var (device.get()));
        auto settingsJson = DynamicObject::Ptr (new DynamicObject());
        settingsJson->setProperty ("path", actualSettingsPath);
        settingsJson->setProperty ("matches_requested_path", settingsPathMatches);
        report->setProperty ("settings", var (settingsJson.get()));
        auto presetsJson = DynamicObject::Ptr (new DynamicObject());
        presetsJson->setProperty ("path", actualPresetDirectory);
        presetsJson->setProperty ("matches_requested_path", presetDirectoryMatches);
        report->setProperty ("presets", var (presetsJson.get()));
        report->setProperty ("assertions", var (assertions.get()));
        Array<var> failureValues;
        for (const auto& failure : failures)
            failureValues.add (failure);
        report->setProperty ("failures", failureValues);

        lifecycle.report.getParentDirectory().createDirectory();
        const auto reportWritten = lifecycle.report.replaceWithText (
            JSON::toString (var (report.get()), true) + newLine, false, false, "\n");
        if (! reportWritten)
            failures.add ("Diagnostic report could not be written");

        setApplicationReturnValue (failures.isEmpty() ? 0 : 1);
        quit();
    }

    LifecycleOptions lifecycle;
    std::unique_ptr<PropertiesFile> settings;
    std::unique_ptr<AudioProcessor> processor;
    DeferredAudioDeviceManager deviceManager;
    AudioProcessorPlayer player;
    std::unique_ptr<ProjectStandaloneWindow> mainWindow;
    std::unique_ptr<FileChooser> stateFileChooser;
    ScopedMessageBox messageBox;
    TimerPhase timerPhase = TimerPhase::configureDevice;
    int logicalSequence = 0;
    int windowVisibleSequence = 0;
    int deviceConfigurationStartSequence = 0;
    int enabledMidiInputCount = 0;
    bool midiEnumerationPerformed = false;
    bool callbacksWired = false;
    String deviceOpenError;
};
}

JUCE_CREATE_APPLICATION_DEFINE (ModelDStandaloneApplication)
