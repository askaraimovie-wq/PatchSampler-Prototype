#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <string>
#include <atomic>

struct PatchNode
{
    int id = 0;
    std::string name;
    std::string type; // Sampler, EQ, Compressor, Saturation, Chorus, Delay, Space3D, Stereo, Limiter
    int gridX = 0;
    int gridY = 0;
    bool loaded = true;
    bool bypassed = false;
    float paramA = 0.5f;
    float paramB = 0.5f;
    float paramC = 0.5f;
};

struct PatchCable
{
    int fromNodeId = 0;
    int toNodeId = 0;
};

struct FrontControl
{
    int id = 0;
    std::string label;
    std::string kind;   // Knob, HFader, VFader, Switch
    std::string style;  // Vintage, Modern
    int gridX = 0;
    int gridY = 0;
    int spanW = 1;
    int spanH = 1;
    float value = 0.5f;
    int linkedNodeId = 1;
    int linkedParamIndex = 0;
};

struct SampleZoneConfig
{
    std::string groupName = "Main Instrument";
    std::string loadedFileName = "Internal Synth Pad (Load WAV)";
    int keyLow = 24;
    int keyHigh = 96;
    int rootNote = 60;
    int velLow = 1;
    int velHigh = 127;
    int roundRobinCount = 3;
    float loopCrossfadeMs = 40.0f;
    float attackMs = 12.0f;
    float decayMs = 220.0f;
    float sustainLevel = 0.85f;
    float releaseMs = 380.0f;
    float pitchSemitones = 0.0f;
};

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiKeyboardStateListener,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void timerCallback() override;

private:
    std::vector<PatchNode> nodes;
    std::vector<PatchCable> cables;
    std::vector<FrontControl> controls;
    SampleZoneConfig samplerConfig;

    bool showGridInRouting = true;
    bool showGridInUI = true;
    int activeTab = 0; // 0: Front Panel, 1: Patcher Grid, 2: Sampler & UI Designer

    // Top Navigation
    juce::TextButton tabFrontBtn   { "FRONT PANEL" };
    juce::TextButton tabGridBtn    { "PATCHER GRID" };
    juce::TextButton tabSamplerBtn { "SAMPLER & UI" };
    juce::TextButton loadWavBtn    { "LOAD WAV" };
    juce::TextButton loadBgBtn     { "LOAD BG" };

    // Designer & Grid Toolbar
    juce::TextButton gridToggleBtn  { "GRID SNAP: ON" };
    juce::TextButton addKnobBtn     { "+ KNOB" };
    juce::TextButton addHFaderBtn   { "+ H-FADER" };
    juce::TextButton addVFaderBtn   { "+ V-FADER" };
    juce::TextButton addSwitchBtn   { "+ SWITCH" };
    juce::TextButton toggleStyleBtn { "STYLE: VINTAGE" };

    // Full FX Module Rack Buttons
    juce::TextButton modEqBtn    { "+ EQ" };
    juce::TextButton modCompBtn  { "+ COMP" };
    juce::TextButton modSatBtn   { "+ SATURATE" };
    juce::TextButton modChorBtn  { "+ CHORUS" };
    juce::TextButton modDlyBtn   { "+ DELAY" };
    juce::TextButton modVerbBtn  { "+ 3D REVERB" };
    juce::TextButton modWideBtn  { "+ STEREO" };
    juce::TextButton modLimBtn   { "+ LIMITER" };

    // On-Screen Piano Keyboard
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    // File Chooser & Custom Background
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Image customBackgroundImage;

    // WAV Sample Buffer (Thread-safe swap via lock)
    juce::AudioFormatManager formatManager;
    juce::CriticalSection sampleLock;
    juce::AudioBuffer<float> wavSampleBuffer;
    double wavSampleRate = 44100.0;
    bool hasWavLoaded = false;

    // Playback Voice State
    std::atomic<bool> noteActive { false };
    std::atomic<int> activeMidiNote { 60 };
    std::atomic<float> activeVelocity { 0.8f };
    double wavPlayhead = 0.0;
    float synthPhase = 0.0f;
    float envGain = 0.0f;
    int roundRobinCounter = 0;

    // Metering
    std::atomic<float> meterLevelL { 0.0f };
    std::atomic<float> meterLevelR { 0.0f };

    // UI Interaction State
    int draggedControlId = -1;
    int draggedNodeId = -1;
    int cableSourceNodeId = -1;
    std::string currentControlStyle = "Vintage";

    // DSP Chain
    double currentSampleRate = 44100.0;
    juce::dsp::Reverb reverbProcessor;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::Compressor<float> compressorProcessor;
    juce::dsp::Chorus<float> chorusProcessor;
    juce::dsp::Limiter<float> limiterProcessor;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowShelf;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highShelf;

    // Simple Stereo Delay Line
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;

    void setupFullDefaultRack();
    void updateDspFromNodes();
    void loadWavFile();
    void loadBackgroundFile();

    void addOrLoadNode (const std::string& type, int gx, int gy);
    void toggleBypassNode (int nodeId);
    void addCable (int fromId, int toId);
    void addFrontControl (const std::string& kind, const std::string& style, int gx, int gy);
    void applyControlToLinkedNode (int controlId, float newValue);

    void drawStudioHeader (juce::Graphics& g, juce::Rectangle<int> headerArea);
    void drawGridBackground (juce::Graphics& g, juce::Rectangle<int> area, int cellW, int cellH);
    void drawFrontPanel (juce::Graphics& g, juce::Rectangle<int> area);
    void drawPatcherGrid (juce::Graphics& g, juce::Rectangle<int> area);
    void drawSamplerAndUiDesigner (juce::Graphics& g, juce::Rectangle<int> area);
    void drawLevelMeter (juce::Graphics& g, juce::Rectangle<int> meterArea);
    void refreshButtonVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
