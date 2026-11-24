#include "MainComponent.h"
#include <cmath>
#include <algorithm>

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();
    setupFullDefaultRack();

    auto styleBtn = [] (juce::TextButton& b, juce::Colour bg)
    {
        b.setColour (juce::TextButton::buttonColourId, bg);
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    };

    styleBtn (tabFrontBtn,   juce::Colour (0xff1e293b));
    styleBtn (tabGridBtn,    juce::Colour (0xff1e293b));
    styleBtn (tabSamplerBtn, juce::Colour (0xff1e293b));
    styleBtn (loadWavBtn,    juce::Colour (0xff0284c7));
    styleBtn (loadBgBtn,     juce::Colour (0xff475569));

    addAndMakeVisible (tabFrontBtn);
    addAndMakeVisible (tabGridBtn);
    addAndMakeVisible (tabSamplerBtn);
    addAndMakeVisible (loadWavBtn);
    addAndMakeVisible (loadBgBtn);
    addAndMakeVisible (keyboardComponent);

    keyboardState.addListener (this);
    keyboardComponent.setAvailableRange (24, 96);

    addChildComponent (gridToggleBtn);
    addChildComponent (addKnobBtn);
    addChildComponent (addHFaderBtn);
    addChildComponent (addVFaderBtn);
    addChildComponent (addSwitchBtn);
    addChildComponent (toggleStyleBtn);

    addChildComponent (modEqBtn);
    addChildComponent (modCompBtn);
    addChildComponent (modSatBtn);
    addChildComponent (modChorBtn);
    addChildComponent (modDlyBtn);
    addChildComponent (modVerbBtn);
    addChildComponent (modWideBtn);
    addChildComponent (modLimBtn);

    tabFrontBtn.onClick   = [this] { activeTab = 0; refreshButtonVisibility(); repaint(); };
    tabGridBtn.onClick    = [this] { activeTab = 1; refreshButtonVisibility(); repaint(); };
    tabSamplerBtn.onClick = [this] { activeTab = 2; refreshButtonVisibility(); repaint(); };
    loadWavBtn.onClick    = [this] { loadWavFile(); };
    loadBgBtn.onClick     = [this] { loadBackgroundFile(); };

    gridToggleBtn.onClick = [this]
    {
        if (activeTab == 1)
            showGridInRouting = !showGridInRouting;
        else
            showGridInUI = !showGridInUI;
        refreshButtonVisibility();
        repaint();
    };

    addKnobBtn.onClick   = [this] { addFrontControl ("Knob",   currentControlStyle, 4, 0); repaint(); };
    addHFaderBtn.onClick = [this] { addFrontControl ("HFader", currentControlStyle, 4, 1); repaint(); };
    addVFaderBtn.onClick = [this] { addFrontControl ("VFader", currentControlStyle, 6, 0); repaint(); };
    addSwitchBtn.onClick = [this] { addFrontControl ("Switch", currentControlStyle, 5, 0); repaint(); };

    toggleStyleBtn.onClick = [this]
    {
        currentControlStyle = (currentControlStyle == "Vintage") ? "Modern" : "Vintage";
        for (auto& c : controls)
            c.style = currentControlStyle;
        toggleStyleBtn.setButtonText ("STYLE: " + juce::String (currentControlStyle).toUpperCase());
        repaint();
    };

    modEqBtn.onClick   = [this] { addOrLoadNode ("EQ",         0, 1); repaint(); };
    modCompBtn.onClick = [this] { addOrLoadNode ("Compressor", 1, 1); repaint(); };
    modSatBtn.onClick  = [this] { addOrLoadNode ("Saturation", 2, 1); repaint(); };
    modChorBtn.onClick = [this] { addOrLoadNode ("Chorus",     3, 1); repaint(); };
    modDlyBtn.onClick  = [this] { addOrLoadNode ("Delay",      0, 2); repaint(); };
    modVerbBtn.onClick = [this] { addOrLoadNode ("Space3D",    1, 2); repaint(); };
    modWideBtn.onClick = [this] { addOrLoadNode ("Stereo",     2, 2); repaint(); };
    modLimBtn.onClick  = [this] { addOrLoadNode ("Limiter",    3, 2); repaint(); };

    setSize (1120, 680);
    refreshButtonVisibility();
    setAudioChannels (0, 2);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    keyboardState.removeListener (this);
    shutdownAudio();
}

void MainComponent::setupFullDefaultRack()
{
    nodes.clear();
    cables.clear();
    controls.clear();

    nodes.push_back ({ 1, "Kontakt Sampler", "Sampler",    0, 0, true, false, 0.85f, 0.25f, 0.50f });
    nodes.push_back ({ 2, "Studio EQ",       "EQ",         1, 0, true, false, 0.52f, 0.55f, 0.50f });
    nodes.push_back ({ 3, "Glue Comp",       "Compressor", 2, 0, true, false, 0.40f, 0.50f, 0.50f });
    nodes.push_back ({ 4, "Tape Drive",      "Saturation", 3, 0, true, false, 0.30f, 0.65f, 0.80f });
    nodes.push_back ({ 5, "3D Space Verb",   "Space3D",    4, 0, true, false, 0.42f, 0.62f, 0.80f });
    nodes.push_back ({ 6, "Master Limiter",  "Limiter",    5, 0, true, false, 0.85f, 0.50f, 0.50f });

    cables.push_back ({ 1, 2 });
    cables.push_back ({ 2, 3 });
    cables.push_back ({ 3, 4 });
    cables.push_back ({ 4, 5 });
    cables.push_back ({ 5, 6 });

    controls.push_back ({ 101, "DYNAMICS",   "VFader", "Modern",  0, 0, 1, 2, 0.80f, 1, 0 });
    controls.push_back ({ 102, "SPACE 3D",   "Knob",   "Vintage", 1, 0, 1, 1, 0.42f, 5, 0 });
    controls.push_back ({ 103, "TAPE DRIVE", "Knob",   "Vintage", 2, 0, 1, 1, 0.30f, 4, 0 });
    controls.push_back ({ 104, "GLUE COMP",  "Knob",   "Vintage", 3, 0, 1, 1, 0.40f, 3, 0 });
    controls.push_back ({ 105, "ROOM SIZE",  "HFader", "Modern",  1, 1, 2, 1, 0.62f, 5, 1 });
    controls.push_back ({ 106, "VERB ON/OFF","Switch", "Modern",  3, 1, 1, 1, 1.00f, 5, 2 });
}

void MainComponent::loadWavFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select WAV/AIFF Sample for PatchSampler", juce::File {}, "*.wav;*.aiff;*.flac");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (!file.existsAsFile())
            return;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader != nullptr)
        {
            juce::AudioBuffer<float> tempBuf ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&tempBuf, 0, (int) reader->lengthInSamples, 0, true, true);

            const juce::ScopedLock sl (sampleLock);
            wavSampleBuffer = std::move (tempBuf);
            wavSampleRate = reader->sampleRate;
            hasWavLoaded = true;
            samplerConfig.loadedFileName = file.getFileName().toStdString();
            wavPlayhead = 0.0;
        }
        repaint();
    });
}

void MainComponent::loadBackgroundFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Custom Front Panel Background Image", juce::File {}, "*.png;*.jpg;*.jpeg");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            customBackgroundImage = juce::ImageFileFormat::loadFrom (file);
            repaint();
        }
    });
}

void MainComponent::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    const int rr = std::max (1, samplerConfig.roundRobinCount);
    roundRobinCounter = (roundRobinCounter + 1) % rr;
    activeMidiNote.store (midiNoteNumber);
    activeVelocity.store (std::clamp (velocity, 0.05f, 1.0f));
    wavPlayhead = 0.0;
    noteActive.store (true);
}

void MainComponent::handleNoteOff (juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    if (activeMidiNote.load() == midiNoteNumber)
        noteActive.store (false);
}

void MainComponent::timerCallback()
{
    repaint (getWidth() - 48, 48, 48, getHeight() - 130);
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (std::max (1, samplesPerBlockExpected));
    spec.numChannels = 2;

    reverbProcessor.prepare (spec);
    compressorProcessor.prepare (spec);
    chorusProcessor.prepare (spec);
    limiterProcessor.prepare (spec);

    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 140.0f, 0.7f, 1.0f);
    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 4500.0f, 0.7f, 1.0f);
    lowShelf.prepare (spec);
    highShelf.prepare (spec);

    delayBuffer.setSize (2, (int) (currentSampleRate * 2.0));
    delayBuffer.clear();
    delayWritePos = 0;

    updateDspFromNodes();
}

void MainComponent::releaseResources()
{
    reverbProcessor.reset();
    compressorProcessor.reset();
    chorusProcessor.reset();
    limiterProcessor.reset();
    lowShelf.reset();
    highShelf.reset();
}

void MainComponent::addOrLoadNode (const std::string& type, int gx, int gy)
{
    for (auto& n : nodes)
    {
        if (n.type == type && !n.loaded)
        {
            n.loaded = true;
            n.bypassed = false;
            n.gridX = gx;
            n.gridY = gy;
            return;
        }
    }

    int nextId = 1;
    for (const auto& n : nodes)
        nextId = std::max (nextId, n.id + 1);

    PatchNode node;
    node.id = nextId;
    node.type = type;
    node.name = type + " FX";
    node.gridX = gx;
    node.gridY = gy;
    node.loaded = true;
    node.bypassed = false;
    nodes.push_back (node);

    if (nodes.size() >= 2)
        addCable (nodes[nodes.size() - 2].id, node.id);
}

void MainComponent::toggleBypassNode (int nodeId)
{
    for (auto& n : nodes)
    {
        if (n.id == nodeId)
        {
            n.bypassed = !n.bypassed;
            break;
        }
    }
}

void MainComponent::addCable (int fromId, int toId)
{
    if (fromId == toId)
        return;
    for (const auto& c : cables)
        if (c.fromNodeId == fromId && c.toNodeId == toId)
            return;
    cables.push_back ({ fromId, toId });
}

void MainComponent::addFrontControl (const std::string& kind, const std::string& style, int gx, int gy)
{
    int nextId = 101;
    for (const auto& c : controls)
        nextId = std::max (nextId, c.id + 1);

    FrontControl fc;
    fc.id = nextId;
    fc.kind = kind;
    fc.style = style;
    fc.label = kind + " " + std::to_string (nextId - 100);
    fc.gridX = std::max (0, gx);
    fc.gridY = std::max (0, gy);
    fc.spanW = (kind == "HFader") ? 2 : 1;
    fc.spanH = (kind == "VFader") ? 2 : 1;
    fc.value = 0.5f;
    fc.linkedNodeId = nodes.empty() ? 1 : nodes.front().id;
    fc.linkedParamIndex = 0;
    controls.push_back (fc);
}

void MainComponent::applyControlToLinkedNode (int controlId, float newValue)
{
    for (auto& c : controls)
    {
        if (c.id == controlId)
        {
            c.value = std::clamp (newValue, 0.0f, 1.0f);
            for (auto& n : nodes)
            {
                if (n.id == c.linkedNodeId)
                {
                    if (c.linkedParamIndex == 0) n.paramA = c.value;
                    else if (c.linkedParamIndex == 1) n.paramB = c.value;
                    else
                    {
                        n.paramC = c.value;
                        n.bypassed = (c.value < 0.5f);
                    }
                }
            }
            break;
        }
    }
}

void MainComponent::updateDspFromNodes()
{
    float lowGainDb = 0.0f;
    float highGainDb = 0.0f;
    float wetLevel = 0.35f;
    float roomSize = 0.60f;
    bool verbBypass = false;

    compressorProcessor.setThreshold (-18.0f);
    compressorProcessor.setRatio (3.0f);
    compressorProcessor.setAttack (10.0f);
    compressorProcessor.setRelease (120.0f);

    chorusProcessor.setRate (0.8f);
    chorusProcessor.setDepth (0.25f);
    chorusProcessor.setMix (0.35f);

    limiterProcessor.setThreshold (-1.0f);
    limiterProcessor.setRelease (50.0f);

    for (const auto& n : nodes)
    {
        if (!n.loaded || n.bypassed)
            continue;

        if (n.type == "EQ")
        {
            lowGainDb = (n.paramA - 0.5f) * 14.0f;
            highGainDb = (n.paramB - 0.5f) * 14.0f;
        }
        else if (n.type == "Compressor")
        {
            compressorProcessor.setThreshold (-6.0f - n.paramA * 30.0f);
            compressorProcessor.setRatio (1.5f + n.paramA * 6.5f);
        }
        else if (n.type == "Chorus")
        {
            chorusProcessor.setMix (n.paramA);
            chorusProcessor.setDepth (n.paramB * 0.6f);
        }
        else if (n.type == "Space3D")
        {
            wetLevel = std::clamp (n.paramA, 0.0f, 1.0f);
            roomSize = std::clamp (n.paramB, 0.05f, 0.98f);
            verbBypass = n.bypassed;
        }
        else if (n.type == "Limiter")
        {
            limiterProcessor.setThreshold (-0.5f - (1.0f - n.paramA) * 12.0f);
        }
    }

    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        currentSampleRate, 140.0f, 0.7f, juce::Decibels::decibelsToGain (lowGainDb));
    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 4500.0f, 0.7f, juce::Decibels::decibelsToGain (highGainDb));

    reverbParams.wetLevel = verbBypass ? 0.0f : wetLevel;
    reverbParams.dryLevel = 1.0f - (verbBypass ? 0.0f : wetLevel * 0.45f);
    reverbParams.roomSize = roomSize;
    reverbParams.damping = 0.42f;
    reverbParams.width = 0.95f;
    reverbProcessor.setParameters (reverbParams);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;
    const int startSample = bufferToFill.startSample;
    const int numSamples = bufferToFill.numSamples;
    const int numChannels = buffer->getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
        buffer->clear (ch, startSample, numSamples);

    float samplerDyn = 0.80f;
    float driveAmount = 1.0f;
    bool hasComp = false, hasChor = false, hasDelay = false, hasVerb = false, hasLim = false;
    float delayMix = 0.3f;

    for (const auto& n : nodes)
    {
        if (!n.loaded || n.bypassed) continue;
        if (n.type == "Sampler")         samplerDyn = n.paramA;
        else if (n.type == "Saturation") driveAmount = 1.0f + n.paramA * 6.5f;
        else if (n.type == "Compressor") hasComp = true;
        else if (n.type == "Chorus")     hasChor = true;
        else if (n.type == "Delay")      { hasDelay = true; delayMix = n.paramA * 0.55f; }
        else if (n.type == "Space3D")    hasVerb = true;
        else if (n.type == "Limiter")    hasLim = true;
    }

    const int midiNote = activeMidiNote.load();
    const float vel = activeVelocity.load();
    const bool isOn = noteActive.load();
    const float rrDetune = 1.0f + ((float) roundRobinCounter - 1.0f) * 0.0015f;
    const float pitchRatio = std::pow (2.0f, ((float) (midiNote - samplerConfig.rootNote) + samplerConfig.pitchSemitones) / 12.0f) * rrDetune;
    const float targetGain = isOn ? (vel * samplerDyn * 0.55f) : 0.0f;

    const float attackCoeff  = 1.0f - std::exp (-1.0f / std::max (1.0f, (samplerConfig.attackMs * 0.001f) * (float) currentSampleRate));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / std::max (1.0f, (samplerConfig.releaseMs * 0.001f) * (float) currentSampleRate));

    {
        const juce::ScopedLock sl (sampleLock);
        const float noteFreq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote) * rrDetune;
        const float phaseInc = (juce::MathConstants<float>::twoPi * noteFreq) / (float) currentSampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            envGain += (targetGain - envGain) * (targetGain > envGain ? attackCoeff : releaseCoeff);

            float outL = 0.0f;
            float outR = 0.0f;

            if (hasWavLoaded && wavSampleBuffer.getNumSamples() > 1)
            {
                const int totalSamps = wavSampleBuffer.getNumSamples();
                if (wavPlayhead < (double) (totalSamps - 1))
                {
                    int idx0 = (int) wavPlayhead;
                    int idx1 = std::min (idx0 + 1, totalSamps - 1);
                    float frac = (float) (wavPlayhead - (double) idx0);

                    float sL = wavSampleBuffer.getSample (0, idx0) * (1.0f - frac) + wavSampleBuffer.getSample (0, idx1) * frac;
                    float sR = (wavSampleBuffer.getNumChannels() > 1)
                                   ? (wavSampleBuffer.getSample (1, idx0) * (1.0f - frac) + wavSampleBuffer.getSample (1, idx1) * frac)
                                   : sL;
                    outL = sL * envGain;
                    outR = sR * envGain;
                    wavPlayhead += pitchRatio * (wavSampleRate / currentSampleRate);
                }
            }
            else
            {
                // Rich harmonic string/pad fallback when no WAV is loaded yet
                float s = (std::sin (synthPhase) * 0.60f + std::sin (synthPhase * 2.0f) * 0.25f + std::sin (synthPhase * 3.0f) * 0.15f) * envGain;
                outL = s;
                outR = s;
                synthPhase += phaseInc;
                if (synthPhase >= juce::MathConstants<float>::twoPi)
                    synthPhase -= juce::MathConstants<float>::twoPi;
            }

            outL = std::tanh (outL * driveAmount);
            outR = std::tanh (outR * driveAmount);

            if (hasDelay && delayBuffer.getNumSamples() > 0)
            {
                int delaySamps = std::min (delayBuffer.getNumSamples() - 1, (int) (currentSampleRate * 0.35));
                int readPos = (delayWritePos - delaySamps + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                float dL = delayBuffer.getSample (0, readPos);
                float dR = delayBuffer.getSample (1, readPos);
                delayBuffer.setSample (0, delayWritePos, outL + dL * 0.35f);
                delayBuffer.setSample (1, delayWritePos, outR + dR * 0.35f);
                delayWritePos = (delayWritePos + 1) % delayBuffer.getNumSamples();
                outL = outL * (1.0f - delayMix) + dL * delayMix;
                outR = outR * (1.0f - delayMix) + dR * delayMix;
            }

            if (numChannels >= 1) buffer->setSample (0, startSample + i, outL);
            if (numChannels >= 2) buffer->setSample (1, startSample + i, outR);
        }
    }

    updateDspFromNodes();

    if (numChannels >= 2)
    {
        juce::dsp::AudioBlock<float> fullBlock (*buffer);
        auto subBlock = fullBlock.getSubBlock ((size_t) startSample, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context (subBlock);

        lowShelf.process (context);
        highShelf.process (context);
        if (hasComp) compressorProcessor.process (context);
        if (hasChor) chorusProcessor.process (context);
        if (hasVerb) reverbProcessor.process (context);
        if (hasLim)  limiterProcessor.process (context);

        meterLevelL.store (buffer->getRMSLevel (0, startSample, numSamples));
        meterLevelR.store (buffer->getRMSLevel (1, startSample, numSamples));
    }
}

void MainComponent::refreshButtonVisibility()
{
    const bool isGrid = (activeTab == 1);
    const bool isEdit = (activeTab == 2);

    gridToggleBtn.setVisible (isGrid || isEdit);
    bool gridOn = isGrid ? showGridInRouting : showGridInUI;
    gridToggleBtn.setButtonText (gridOn ? "GRID SNAP: ON" : "GRID SNAP: OFF");

    addKnobBtn.setVisible (isEdit);
    addHFaderBtn.setVisible (isEdit);
    addVFaderBtn.setVisible (isEdit);
    addSwitchBtn.setVisible (isEdit);
    toggleStyleBtn.setVisible (isEdit);

    modEqBtn.setVisible (isGrid);
    modCompBtn.setVisible (isGrid);
    modSatBtn.setVisible (isGrid);
    modChorBtn.setVisible (isGrid);
    modDlyBtn.setVisible (isGrid);
    modVerbBtn.setVisible (isGrid);
    modWideBtn.setVisible (isGrid);
    modLimBtn.setVisible (isGrid);

    resized();
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop (48).reduced (10, 8);

    // Leave space on left for Brand Title
    topBar.removeFromLeft (190);
    tabFrontBtn.setBounds (topBar.removeFromLeft (120));
    topBar.removeFromLeft (6);
    tabGridBtn.setBounds (topBar.removeFromLeft (125));
    topBar.removeFromLeft (6);
    tabSamplerBtn.setBounds (topBar.removeFromLeft (125));

    loadBgBtn.setBounds (topBar.removeFromRight (95));
    topBar.removeFromLeft (6);
    loadWavBtn.setBounds (topBar.removeFromRight (105).translated (-8, 0));

    // Bottom Piano Keyboard
    auto kbArea = area.removeFromBottom (82).reduced (8, 4);
    keyboardComponent.setBounds (kbArea);

    if (activeTab == 1)
    {
        auto subBar = area.removeFromTop (36).reduced (10, 4);
        modEqBtn.setBounds   (subBar.removeFromLeft (72));  subBar.removeFromLeft (4);
        modCompBtn.setBounds (subBar.removeFromLeft (80));  subBar.removeFromLeft (4);
        modSatBtn.setBounds  (subBar.removeFromLeft (95));  subBar.removeFromLeft (4);
        modChorBtn.setBounds (subBar.removeFromLeft (86));  subBar.removeFromLeft (4);
        modDlyBtn.setBounds  (subBar.removeFromLeft (78));  subBar.removeFromLeft (4);
        modVerbBtn.setBounds (subBar.removeFromLeft (100)); subBar.removeFromLeft (4);
        modWideBtn.setBounds (subBar.removeFromLeft (84));  subBar.removeFromLeft (4);
        modLimBtn.setBounds  (subBar.removeFromLeft (86));  subBar.removeFromLeft (10);
        gridToggleBtn.setBounds (subBar.removeFromLeft (125));
    }
    else if (activeTab == 2)
    {
        auto subBar = area.removeFromTop (36).reduced (10, 4);
        addKnobBtn.setBounds     (subBar.removeFromLeft (85));  subBar.removeFromLeft (6);
        addHFaderBtn.setBounds   (subBar.removeFromLeft (95));  subBar.removeFromLeft (6);
        addVFaderBtn.setBounds   (subBar.removeFromLeft (95));  subBar.removeFromLeft (6);
        addSwitchBtn.setBounds   (subBar.removeFromLeft (92));  subBar.removeFromLeft (6);
        toggleStyleBtn.setBounds (subBar.removeFromLeft (140)); subBar.removeFromLeft (10);
        gridToggleBtn.setBounds  (subBar.removeFromLeft (125));
    }
}

void MainComponent::drawStudioHeader (juce::Graphics& g, juce::Rectangle<int> headerArea)
{
    juce::ColourGradient grad (juce::Colour (0xff161e2e), 0.0f, 0.0f,
                               juce::Colour (0xff0b0f19), 0.0f, (float) headerArea.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (headerArea);

    g.setColour (juce::Colour (0xff38bdf8));
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText ("PATCHSAMPLER", headerArea.withWidth (185).reduced (14, 0), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff334155));
    g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) headerArea.getRight());
}

void MainComponent::drawGridBackground (juce::Graphics& g, juce::Rectangle<int> area, int cellW, int cellH)
{
    g.setColour (juce::Colour (0xff1e2533));
    for (int x = area.getX(); x <= area.getRight(); x += cellW)
        g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
    for (int y = area.getY(); y <= area.getBottom(); y += cellH)
        g.drawHorizontalLine (y, (float) area.getX(), (float) area.getRight());
}

void MainComponent::drawFrontPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (customBackgroundImage.isValid())
        g.drawImageWithin (customBackgroundImage, area.getX(), area.getY(), area.getWidth(), area.getHeight(), juce::RectanglePlacement::stretchToFit);

    if (showGridInUI)
        drawGridBackground (g, area, 120, 120);

    for (const auto& c : controls)
    {
        juce::Rectangle<int> box (
            area.getX() + 18 + c.gridX * 120,
            area.getY() + 18 + c.gridY * 120,
            c.spanW * 110,
            c.spanH * 110
        );

        const bool isVintage = (c.style == "Vintage");
        juce::Colour topCol = isVintage ? juce::Colour (0xff33271e) : juce::Colour (0xff1b2434);
        juce::Colour botCol = isVintage ? juce::Colour (0xff1f1712) : juce::Colour (0xff0f1623);
        juce::Colour accent = isVintage ? juce::Colour (0xfff59e0b) : juce::Colour (0xff38bdf8);

        juce::ColourGradient cardGrad (topCol, (float) box.getX(), (float) box.getY(),
                                       botCol, (float) box.getX(), (float) box.getBottom(), false);
        g.setGradientFill (cardGrad);
        g.fillRoundedRectangle (box.toFloat(), 10.0f);

        g.setColour (accent.withAlpha (0.75f));
        g.drawRoundedRectangle (box.toFloat(), 10.0f, 1.5f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.drawText (c.label, box.removeFromTop (24), juce::Justification::centred);

        auto body = box.reduced (10, 6);
        if (c.kind == "Knob")
        {
            const float radius = (float) std::min (body.getWidth(), body.getHeight()) * 0.38f;
            auto centre = body.getCentre().toFloat();

            // LED Value Arc
            juce::Path arc;
            const float startA = juce::degreesToRadians (-135.0f);
            const float endA   = juce::degreesToRadians (-135.0f + c.value * 270.0f);
            arc.addCentredArc (centre.x, centre.y, radius + 5.0f, radius + 5.0f, 0.0f, startA, endA, true);
            g.setColour (accent);
            g.strokePath (arc, juce::PathStrokeType (3.0f));

            // 3D Metallic Knob Body
            juce::ColourGradient knobGrad (juce::Colour (0xff4b5563), centre.x - radius, centre.y - radius,
                                           juce::Colour (0xff111827), centre.x + radius, centre.y + radius, true);
            g.setGradientFill (knobGrad);
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

            g.setColour (juce::Colours::white);
            g.drawLine (centre.x, centre.y,
                        centre.x + std::sin (endA) * (radius * 0.85f),
                        centre.y - std::cos (endA) * (radius * 0.85f), 2.5f);
        }
        else if (c.kind == "HFader")
        {
            auto track = body.withSizeKeepingCentre (body.getWidth(), 10);
            g.setColour (juce::Colour (0xff090d16));
            g.fillRoundedRectangle (track.toFloat(), 4.0f);
            int thumbX = track.getX() + (int) (c.value * (float) std::max (1, track.getWidth() - 22));
            g.setColour (accent);
            g.fillRoundedRectangle ((float) thumbX, (float) (track.getCentreY() - 12), 22.0f, 24.0f, 4.0f);
        }
        else if (c.kind == "VFader")
        {
            auto track = body.withSizeKeepingCentre (10, body.getHeight());
            g.setColour (juce::Colour (0xff090d16));
            g.fillRoundedRectangle (track.toFloat(), 4.0f);
            int thumbY = track.getBottom() - 22 - (int) (c.value * (float) std::max (1, track.getHeight() - 22));
            g.setColour (accent);
            g.fillRoundedRectangle ((float) (track.getCentreX() - 13), (float) thumbY, 26.0f, 22.0f, 4.0f);
        }
        else
        {
            auto sw = body.withSizeKeepingCentre (54, 28);
            g.setColour (c.value >= 0.5f ? juce::Colour (0xff10b981) : juce::Colour (0xff334155));
            g.fillRoundedRectangle (sw.toFloat(), 6.0f);
            g.setColour (juce::Colours::white);
            g.drawText (c.value >= 0.5f ? "ON" : "OFF", sw, juce::Justification::centred);
        }
    }
}

void MainComponent::drawPatcherGrid (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (showGridInRouting)
        drawGridBackground (g, area, 170, 120);

    auto getNodeRect = [&] (const PatchNode& n)
    {
        return juce::Rectangle<int> (
            area.getX() + 18 + n.gridX * 170,
            area.getY() + 18 + n.gridY * 120,
            150,
            88
        );
    };

    // Draw FL Patcher-style curved cables
    for (const auto& cab : cables)
    {
        const PatchNode* a = nullptr;
        const PatchNode* b = nullptr;
        for (const auto& n : nodes)
        {
            if (!n.loaded) continue;
            if (n.id == cab.fromNodeId) a = &n;
            if (n.id == cab.toNodeId) b = &n;
        }
        if (a != nullptr && b != nullptr)
        {
            auto ra = getNodeRect (*a);
            auto rb = getNodeRect (*b);
            float x1 = (float) ra.getRight(), y1 = (float) ra.getCentreY();
            float x2 = (float) rb.getX(),     y2 = (float) rb.getCentreY();
            float midX = (x1 + x2) * 0.5f;

            juce::Path cable;
            cable.startNewSubPath (x1, y1);
            cable.cubicTo (midX, y1, midX, y2, x2, y2);
            g.setColour (juce::Colour (0xfffbbf24));
            g.strokePath (cable, juce::PathStrokeType (2.8f));
        }
    }

    for (const auto& n : nodes)
    {
        if (!n.loaded) continue;
        auto r = getNodeRect (n);
        g.setColour (n.bypassed ? juce::Colour (0xff1e293b) : juce::Colour (0xff172554));
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (n.bypassed ? juce::Colour (0xff64748b) : juce::Colour (0xff38bdf8));
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.8f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (n.name, r.reduced (8, 6).removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xffcbd5e1));
        g.setFont (10.5f);
        g.drawText ("P1:" + juce::String (n.paramA, 2) + "  P2:" + juce::String (n.paramB, 2),
                    r.reduced (8, 4).removeFromBottom (22), juce::Justification::centredLeft);
    }
}

void MainComponent::drawSamplerAndUiDesigner (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto left = area.removeFromLeft (area.getWidth() / 2).reduced (14);
    auto right = area.reduced (14);

    g.setColour (juce::Colour (0xff151c2c));
    g.fillRoundedRectangle (left.toFloat(), 10.0f);
    g.fillRoundedRectangle (right.toFloat(), 10.0f);

    g.setColour (juce::Colour (0xff38bdf8));
    g.setFont (juce::Font (13.5f, juce::Font::bold));
    g.drawText ("KONTAKT SAMPLER ENGINE & MAPPING", left.removeFromTop (30).reduced (12, 4), juce::Justification::centredLeft);

    const auto& sc = samplerConfig;
    juce::String info =
        "Loaded Sample: " + juce::String (sc.loadedFileName) + "\n" +
        "Group: " + juce::String (sc.groupName) + "\n" +
        "Key Range: " + juce::String (sc.keyLow) + " - " + juce::String (sc.keyHigh) + " (Root: " + juce::String (sc.rootNote) + ")\n" +
        "Velocity Range: " + juce::String (sc.velLow) + " - " + juce::String (sc.velHigh) + "\n" +
        "Round-Robin Cycles: " + juce::String (sc.roundRobinCount) + "x\n" +
        "Loop Crossfade: " + juce::String (sc.loopCrossfadeMs, 1) + " ms\n" +
        "ADSR: A " + juce::String (sc.attackMs, 0) + "ms | D " + juce::String (sc.decayMs, 0) +
        "ms | S " + juce::String (sc.sustainLevel, 2) + " | R " + juce::String (sc.releaseMs, 0) + "ms";

    g.setColour (juce::Colour (0xffe2e8f0));
    g.setFont (12.0f);
    g.drawFittedText (info, left.reduced (14), juce::Justification::topLeft, 12);

    g.setColour (juce::Colour (0xfff59e0b));
    g.setFont (juce::Font (13.5f, juce::Font::bold));
    g.drawText ("FRONT PANEL UI DESIGNER", right.removeFromTop (30).reduced (12, 4), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffe2e8f0));
    g.setFont (12.0f);
    g.drawFittedText ("Active Front Panel Controls: " + juce::String ((int) controls.size()) +
                      "\n- Click LOAD WAV (top right) to load your own WAV sample."
                      "\n- Click LOAD BG (top right) to set a custom background image."
                      "\n- Play the bottom Piano Keyboard to audition in real time."
                      "\n- Right-click any module in Patcher Grid to Bypass."
                      "\n- Alt + Drag between modules to connect Curved Patch Cables.",
                      right.reduced (14), juce::Justification::topLeft, 12);
}

void MainComponent::drawLevelMeter (juce::Graphics& g, juce::Rectangle<int> meterArea)
{
    g.setColour (juce::Colour (0xff090d16));
    g.fillRect (meterArea);

    auto inner = meterArea.reduced (8, 12);
    auto leftBar = inner.removeFromLeft (inner.getWidth() / 2 - 2);
    inner.removeFromLeft (4);
    auto rightBar = inner;

    float lvlL = std::clamp (meterLevelL.load() * 2.5f, 0.0f, 1.0f);
    float lvlR = std::clamp (meterLevelR.load() * 2.5f, 0.0f, 1.0f);

    g.setColour (juce::Colour (0xff1e293b));
    g.fillRect (leftBar);
    g.fillRect (rightBar);

    g.setColour (juce::Colour (0xff10b981));
    g.fillRect (leftBar.removeFromBottom ((int) ((float) leftBar.getHeight() * lvlL)));
    g.fillRect (rightBar.removeFromBottom ((int) ((float) rightBar.getHeight() * lvlR)));
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0f19));
    auto full = getLocalBounds();
    drawStudioHeader (g, full.removeFromTop (48));

    auto mainArea = full.withTrimmedBottom (82);
    auto meterArea = mainArea.removeFromRight (44);
    drawLevelMeter (g, meterArea);

    auto content = mainArea.withTrimmedTop (activeTab == 0 ? 0 : 36);

    if (activeTab == 0)
        drawFrontPanel (g, content);
    else if (activeTab == 1)
        drawPatcherGrid (g, content);
    else
        drawSamplerAndUiDesigner (g, content);
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    auto content = getLocalBounds().withTrimmedTop (activeTab == 0 ? 48 : 84).withTrimmedBottom (82).withTrimmedRight (44);
    if (!content.contains (e.getPosition()))
        return;

    if (activeTab == 0)
    {
        for (auto& c : controls)
        {
            juce::Rectangle<int> box (
                content.getX() + 18 + c.gridX * 120,
                content.getY() + 18 + c.gridY * 120,
                c.spanW * 110,
                c.spanH * 110
            );
            if (box.contains (e.getPosition()))
            {
                draggedControlId = c.id;
                if (c.kind == "Switch")
                {
                    applyControlToLinkedNode (c.id, c.value >= 0.5f ? 0.0f : 1.0f);
                    repaint();
                }
                return;
            }
        }
    }
    else if (activeTab == 1)
    {
        for (const auto& n : nodes)
        {
            if (!n.loaded) continue;
            juce::Rectangle<int> r (
                content.getX() + 18 + n.gridX * 170,
                content.getY() + 18 + n.gridY * 120,
                150,
                88
            );
            if (r.contains (e.getPosition()))
            {
                if (e.mods.isRightButtonDown())
                {
                    toggleBypassNode (n.id);
                    repaint();
                    return;
                }
                if (e.mods.isAltDown())
                {
                    cableSourceNodeId = n.id;
                    return;
                }
                draggedNodeId = n.id;
                return;
            }
        }
    }
}

void MainComponent::mouseDrag (const juce::MouseEvent& e)
{
    auto content = getLocalBounds().withTrimmedTop (activeTab == 0 ? 48 : 84).withTrimmedBottom (82).withTrimmedRight (44);

    if (activeTab == 0 && draggedControlId != -1)
    {
        for (const auto& c : controls)
        {
            if (c.id == draggedControlId && c.kind != "Switch")
            {
                float delta = (float) (e.getMouseDownY() - e.getPosition().y) / 95.0f;
                applyControlToLinkedNode (c.id, c.value + delta);
                repaint();
                break;
            }
        }
    }
    else if (activeTab == 1 && draggedNodeId != -1)
    {
        for (auto& n : nodes)
        {
            if (n.id == draggedNodeId)
            {
                int relX = std::max (0, e.getPosition().x - content.getX() - 18);
                int relY = std::max (0, e.getPosition().y - content.getY() - 18);
                n.gridX = showGridInRouting ? (relX / 170) : (relX / 40);
                n.gridY = showGridInRouting ? (relY / 120) : (relY / 40);
                repaint();
                break;
            }
        }
    }
}

void MainComponent::mouseUp (const juce::MouseEvent& e)
{
    if (cableSourceNodeId != -1 && activeTab == 1)
    {
        auto content = getLocalBounds().withTrimmedTop (84).withTrimmedBottom (82).withTrimmedRight (44);
        for (const auto& n : nodes)
        {
            if (!n.loaded) continue;
            juce::Rectangle<int> r (
                content.getX() + 18 + n.gridX * 170,
                content.getY() + 18 + n.gridY * 120,
                150,
                88
            );
            if (r.contains (e.getPosition()))
            {
                addCable (cableSourceNodeId, n.id);
                break;
            }
        }
    }

    draggedControlId = -1;
    draggedNodeId = -1;
    cableSourceNodeId = -1;
    repaint();
}
