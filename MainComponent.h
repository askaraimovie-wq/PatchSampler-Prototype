#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <string>

struct PatchNode
{
    int id = 0;
    std::string name;
    std::string type; // Sampler, EQ, Saturation, Space3D, Stereo
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
    std::string groupName = "Sustains";
    int keyLow = 36;
    int keyHigh = 84;
    int rootNote = 60;
    int velLow = 1;
    int velHigh = 127;
    int roundRobinCount = 3;
    float loopCrossfadeMs = 45.0f;
    float attackMs = 15.0f;
    float decayMs = 200.0f;
    float sustainLevel = 0.85f;
    float releaseMs = 350.0f;
    float pitchSemitones = 0.0f;
};

class MainComponent : public juce::AudioAppComponent
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

private:
    std::vector<PatchNode> nodes;
    std::vector<PatchCable> cables;
    std::vector<FrontControl> controls;
    SampleZoneConfig samplerConfig;

    bool showGridInRouting = true;
    bool showGridInUI = true;
    int activeTab = 0; // 0: Front Panel, 1: Patcher Grid, 2: Sampler & UI Edit

    juce::TextButton tabFrontBtn  { "FRONT PANEL" };
    juce::TextButton tabGridBtn   { "PATCHER GRID" };
    juce::TextButton tabEditBtn   { "SAMPLER & UI EDIT" };
    juce::TextButton testNoteBtn  { "TEST TONE" };

    juce::TextButton gridToggleBtn  { "GRID SNAP: ON" };
    juce::TextButton addKnobBtn     { "+ KNOB" };
    juce::TextButton addHFaderBtn   { "+ H-FADER" };
    juce::TextButton addVFaderBtn   { "+ V-FADER" };
    juce::TextButton toggleStyleBtn { "STYLE: VINTAGE" };

    juce::TextButton modSamplerBtn { "+ SAMPLER" };
    juce::TextButton modEqBtn      { "+ EQ" };
    juce::TextButton modSatBtn     { "+ SATURATION" };
    juce::TextButton modVerbBtn    { "+ 3D REVERB" };

    int draggedControlId = -1;
    int draggedNodeId = -1;
    int cableSourceNodeId = -1;
    std::string currentControlStyle = "Vintage";

    double currentSampleRate = 44100.0;
    float phase = 0.0f;
    float currentGain = 0.0f;
    bool testToneActive = false;
    int roundRobinCounter = 0;

    juce::dsp::Reverb reverbProcessor;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowShelf;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highShelf;

    void setupDefaultPrototypeRack();
    void updateDspFromNodes();
    void addOrLoadNode (const std::string& type, int gx, int gy);
    void toggleBypassNode (int nodeId);
    void addCable (int fromId, int toId);
    void addFrontControl (const std::string& kind, const std::string& style, int gx, int gy);
    void applyControlToLinkedNode (int controlId, float newValue);

    void drawGridBackground (juce::Graphics& g, juce::Rectangle<int> area, int cellW, int cellH);
    void drawFrontPanel (juce::Graphics& g, juce::Rectangle<int> area);
    void drawPatcherGrid (juce::Graphics& g, juce::Rectangle<int> area);
    void drawSamplerAndUiEdit (juce::Graphics& g, juce::Rectangle<int> area);
    void refreshButtonVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
