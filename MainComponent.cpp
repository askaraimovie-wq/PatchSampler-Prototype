#include "MainComponent.h"
#include <cmath>
#include <algorithm>

MainComponent::MainComponent()
{
    setupDefaultPrototypeRack();

    addAndMakeVisible (tabFrontBtn);
    addAndMakeVisible (tabGridBtn);
    addAndMakeVisible (tabEditBtn);
    addAndMakeVisible (testNoteBtn);

    addChildComponent (gridToggleBtn);
    addChildComponent (addKnobBtn);
    addChildComponent (addHFaderBtn);
    addChildComponent (addVFaderBtn);
    addChildComponent (toggleStyleBtn);

    addChildComponent (modSamplerBtn);
    addChildComponent (modEqBtn);
    addChildComponent (modSatBtn);
    addChildComponent (modVerbBtn);

    tabFrontBtn.onClick = [this] { activeTab = 0; refreshButtonVisibility(); repaint(); };
    tabGridBtn.onClick  = [this] { activeTab = 1; refreshButtonVisibility(); repaint(); };
    tabEditBtn.onClick  = [this] { activeTab = 2; refreshButtonVisibility(); repaint(); };

    testNoteBtn.onClick = [this]
    {
        testToneActive = !testToneActive;
        if (testToneActive)
        {
            const int rr = std::max (1, samplerConfig.roundRobinCount);
            roundRobinCounter = (roundRobinCounter + 1) % rr;
            testNoteBtn.setButtonText ("STOP TONE");
        }
        else
        {
            testNoteBtn.setButtonText ("TEST TONE");
        }
        repaint();
    };

    gridToggleBtn.onClick = [this]
    {
        if (activeTab == 1)
            showGridInRouting = !showGridInRouting;
        else
            showGridInUI = !showGridInUI;
        refreshButtonVisibility();
        repaint();
    };

    addKnobBtn.onClick   = [this] { addFrontControl ("Knob", currentControlStyle, 1, 1); repaint(); };
    addHFaderBtn.onClick = [this] { addFrontControl ("HFader", currentControlStyle, 0, 2); repaint(); };
    addVFaderBtn.onClick = [this] { addFrontControl ("VFader", currentControlStyle, 3, 0); repaint(); };

    toggleStyleBtn.onClick = [this]
    {
        currentControlStyle = (currentControlStyle == "Vintage") ? "Modern" : "Vintage";
        for (auto& c : controls)
            c.style = currentControlStyle;
        toggleStyleBtn.setButtonText ("STYLE: " + juce::String (currentControlStyle).toUpperCase());
        repaint();
    };

    modSamplerBtn.onClick = [this] { addOrLoadNode ("Sampler", 0, 1); repaint(); };
    modEqBtn.onClick      = [this] { addOrLoadNode ("EQ", 1, 1); repaint(); };
    modSatBtn.onClick     = [this] { addOrLoadNode ("Saturation", 2, 1); repaint(); };
    modVerbBtn.onClick    = [this] { addOrLoadNode ("Space3D", 3, 1); repaint(); };

    setSize (960, 600);
    refreshButtonVisibility();
    setAudioChannels (0, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::setupDefaultPrototypeRack()
{
    nodes.clear();
    cables.clear();
    controls.clear();

    nodes.push_back ({ 1, "Kontakt Sampler", "Sampler",    0, 0, true, false, 0.80f, 0.20f, 0.50f });
    nodes.push_back ({ 2, "Vintage EQ",      "EQ",         1, 0, true, false, 0.50f, 0.50f, 0.50f });
    nodes.push_back ({ 3, "Tape Drive",      "Saturation", 2, 0, true, false, 0.35f, 0.70f, 0.80f });
    nodes.push_back ({ 4, "3D Space Verb",   "Space3D",    3, 0, true, false, 0.45f, 0.60f, 0.70f });

    cables.push_back ({ 1, 2 });
    cables.push_back ({ 2, 3 });
    cables.push_back ({ 3, 4 });

    controls.push_back ({ 101, "DYNAMICS", "VFader", "Modern",  0, 0, 1, 2, 0.75f, 1, 0 });
    controls.push_back ({ 102, "SPACE 3D", "Knob",   "Vintage", 1, 0, 1, 1, 0.45f, 4, 0 });
    controls.push_back ({ 103, "DRIVE",    "Knob",   "Vintage", 2, 0, 1, 1, 0.35f, 3, 0 });
    controls.push_back ({ 104, "WIDTH",    "HFader", "Modern",  1, 1, 2, 1, 0.60f, 4, 1 });
    controls.push_back ({ 105, "VERB BYP", "Switch", "Modern",  3, 0, 1, 1, 1.00f, 4, 2 });
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (std::max (1, samplesPerBlockExpected));
    spec.numChannels = 2;

    reverbProcessor.prepare (spec);
    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 140.0f, 0.7f, 1.0f);
    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 4200.0f, 0.7f, 1.0f);
    lowShelf.prepare (spec);
    highShelf.prepare (spec);

    updateDspFromNodes();
}

void MainComponent::releaseResources()
{
    reverbProcessor.reset();
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
    node.name = type + " Module";
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
    {
        if (c.fromNodeId == fromId && c.toNodeId == toId)
            return;
    }

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
    float roomSize = 0.55f;
    bool verbBypass = false;

    for (const auto& n : nodes)
    {
        if (!n.loaded || n.bypassed)
            continue;

        if (n.type == "EQ")
        {
            lowGainDb = (n.paramA - 0.5f) * 12.0f;
            highGainDb = (n.paramB - 0.5f) * 12.0f;
        }
        else if (n.type == "Space3D")
        {
            wetLevel = std::clamp (n.paramA, 0.0f, 1.0f);
            roomSize = std::clamp (n.paramB, 0.05f, 0.98f);
            verbBypass = n.bypassed;
        }
    }

    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        currentSampleRate, 140.0f, 0.7f, juce::Decibels::decibelsToGain (lowGainDb));
    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 4200.0f, 0.7f, juce::Decibels::decibelsToGain (highGainDb));

    reverbParams.wetLevel = verbBypass ? 0.0f : wetLevel;
    reverbParams.dryLevel = 1.0f - (verbBypass ? 0.0f : wetLevel * 0.45f);
    reverbParams.roomSize = roomSize;
    reverbParams.damping = 0.45f;
    reverbParams.width = 0.9f;
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

    float samplerDyn = 0.75f;
    float driveAmount = 1.0f;

    for (const auto& n : nodes)
    {
        if (!n.loaded || n.bypassed)
            continue;
        if (n.type == "Sampler")
            samplerDyn = n.paramA;
        else if (n.type == "Saturation")
            driveAmount = 1.0f + n.paramA * 6.0f;
    }

    const float rrDetune = 1.0f + ((float) roundRobinCounter - 1.0f) * 0.0018f;
    const float semiShift = std::pow (2.0f, samplerConfig.pitchSemitones / 12.0f);
    const float noteFreq = 220.0f * semiShift * rrDetune;
    const float targetGain = testToneActive ? (0.28f * samplerDyn) : 0.0f;
    const float phaseInc = (juce::MathConstants<float>::twoPi * noteFreq) / (float) currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const float attackCoeff = 1.0f - std::exp (-1.0f / std::max (1.0f, (samplerConfig.attackMs * 0.001f) * (float) currentSampleRate));
        const float releaseCoeff = 1.0f - std::exp (-1.0f / std::max (1.0f, (samplerConfig.releaseMs * 0.001f) * (float) currentSampleRate));
        const float coeff = (targetGain > currentGain) ? attackCoeff : releaseCoeff;
        currentGain += (targetGain - currentGain) * coeff;

        float sampleVal = (std::sin (phase) * 0.65f + std::sin (phase * 2.0f) * 0.25f + std::sin (phase * 3.0f) * 0.10f) * currentGain;
        sampleVal = std::tanh (sampleVal * driveAmount);

        phase += phaseInc;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer->setSample (ch, startSample + i, sampleVal);
    }

    updateDspFromNodes();

    if (numChannels >= 2)
    {
        juce::dsp::AudioBlock<float> fullBlock (*buffer);
        auto subBlock = fullBlock.getSubBlock ((size_t) startSample, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context (subBlock);
        lowShelf.process (context);
        highShelf.process (context);

        bool hasVerb = false;
        for (const auto& n : nodes)
        {
            if (n.loaded && !n.bypassed && n.type == "Space3D")
            {
                hasVerb = true;
                break;
            }
        }
        if (hasVerb)
            reverbProcessor.process (context);
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
    toggleStyleBtn.setVisible (isEdit);

    modSamplerBtn.setVisible (isGrid);
    modEqBtn.setVisible (isGrid);
    modSatBtn.setVisible (isGrid);
    modVerbBtn.setVisible (isGrid);

    resized();
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop (44).reduced (8, 6);

    tabFrontBtn.setBounds (topBar.removeFromLeft (130));
    topBar.removeFromLeft (6);
    tabGridBtn.setBounds (topBar.removeFromLeft (135));
    topBar.removeFromLeft (6);
    tabEditBtn.setBounds (topBar.removeFromLeft (155));

    testNoteBtn.setBounds (topBar.removeFromRight (115));

    if (activeTab == 1)
    {
        auto subBar = area.removeFromTop (36).reduced (8, 4);
        modSamplerBtn.setBounds (subBar.removeFromLeft (105));
        subBar.removeFromLeft (6);
        modEqBtn.setBounds (subBar.removeFromLeft (80));
        subBar.removeFromLeft (6);
        modSatBtn.setBounds (subBar.removeFromLeft (115));
        subBar.removeFromLeft (6);
        modVerbBtn.setBounds (subBar.removeFromLeft (110));
        subBar.removeFromLeft (12);
        gridToggleBtn.setBounds (subBar.removeFromLeft (130));
    }
    else if (activeTab == 2)
    {
        auto subBar = area.removeFromTop (36).reduced (8, 4);
        addKnobBtn.setBounds (subBar.removeFromLeft (85));
        subBar.removeFromLeft (6);
        addHFaderBtn.setBounds (subBar.removeFromLeft (95));
        subBar.removeFromLeft (6);
        addVFaderBtn.setBounds (subBar.removeFromLeft (95));
        subBar.removeFromLeft (6);
        toggleStyleBtn.setBounds (subBar.removeFromLeft (145));
        subBar.removeFromLeft (10);
        gridToggleBtn.setBounds (subBar.removeFromLeft (130));
    }
}

void MainComponent::drawGridBackground (juce::Graphics& g, juce::Rectangle<int> area, int cellW, int cellH)
{
    g.setColour (juce::Colour (0xff252932));
    for (int x = area.getX(); x <= area.getRight(); x += cellW)
        g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
    for (int y = area.getY(); y <= area.getBottom(); y += cellH)
        g.drawHorizontalLine (y, (float) area.getX(), (float) area.getRight());
}

void MainComponent::drawFrontPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (showGridInUI)
        drawGridBackground (g, area, 110, 110);

    for (const auto& c : controls)
    {
        juce::Rectangle<int> box (
            area.getX() + 16 + c.gridX * 110,
            area.getY() + 16 + c.gridY * 110,
            c.spanW * 102,
            c.spanH * 102
        );

        const bool isVintage = (c.style == "Vintage");
        g.setColour (isVintage ? juce::Colour (0xff2b231d) : juce::Colour (0xff19202a));
        g.fillRoundedRectangle (box.toFloat(), 8.0f);
        g.setColour (isVintage ? juce::Colour (0xffb8864b) : juce::Colour (0xff38bdf8));
        g.drawRoundedRectangle (box.toFloat(), 8.0f, 1.5f);

        g.setColour (juce::Colours::white);
        g.setFont (12.0f);
        g.drawText (c.label, box.removeFromTop (22), juce::Justification::centred);

        auto body = box.reduced (10, 6);
        if (c.kind == "Knob")
        {
            const float radius = (float) std::min (body.getWidth(), body.getHeight()) * 0.36f;
            auto centre = body.getCentre().toFloat();
            g.setColour (juce::Colour (0xff3c424f));
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

            const float angle = juce::degreesToRadians (-135.0f + c.value * 270.0f);
            g.setColour (isVintage ? juce::Colour (0xfff59e0b) : juce::Colour (0xff22d3ee));
            g.drawLine (centre.x, centre.y,
                        centre.x + std::sin (angle) * radius,
                        centre.y - std::cos (angle) * radius, 2.5f);
        }
        else if (c.kind == "HFader")
        {
            auto track = body.withSizeKeepingCentre (body.getWidth(), 10);
            g.setColour (juce::Colour (0xff0f172a));
            g.fillRect (track);
            int thumbX = track.getX() + (int) (c.value * (float) std::max (1, track.getWidth() - 18));
            g.setColour (isVintage ? juce::Colour (0xfff59e0b) : juce::Colour (0xff38bdf8));
            g.fillRoundedRectangle ((float) thumbX, (float) (track.getCentreY() - 11), 18.0f, 22.0f, 3.0f);
        }
        else if (c.kind == "VFader")
        {
            auto track = body.withSizeKeepingCentre (10, body.getHeight());
            g.setColour (juce::Colour (0xff0f172a));
            g.fillRect (track);
            int thumbY = track.getBottom() - 18 - (int) (c.value * (float) std::max (1, track.getHeight() - 18));
            g.setColour (isVintage ? juce::Colour (0xfff59e0b) : juce::Colour (0xff38bdf8));
            g.fillRoundedRectangle ((float) (track.getCentreX() - 11), (float) thumbY, 22.0f, 18.0f, 3.0f);
        }
        else
        {
            auto sw = body.withSizeKeepingCentre (44, 24);
            g.setColour (c.value >= 0.5f ? juce::Colour (0xff10b981) : juce::Colour (0xff475569));
            g.fillRoundedRectangle (sw.toFloat(), 5.0f);
            g.setColour (juce::Colours::white);
            g.drawText (c.value >= 0.5f ? "ON" : "OFF", sw, juce::Justification::centred);
        }
    }
}

void MainComponent::drawPatcherGrid (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (showGridInRouting)
        drawGridBackground (g, area, 190, 125);

    auto getNodeRect = [&] (const PatchNode& n)
    {
        return juce::Rectangle<int> (
            area.getX() + 20 + n.gridX * 190,
            area.getY() + 20 + n.gridY * 125,
            165,
            92
        );
    };

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
            g.setColour (juce::Colour (0xfffbbf24));
            g.drawLine ((float) ra.getRight(), (float) ra.getCentreY(),
                        (float) rb.getX(), (float) rb.getCentreY(), 2.5f);
        }
    }

    for (const auto& n : nodes)
    {
        if (!n.loaded) continue;
        auto r = getNodeRect (n);
        g.setColour (n.bypassed ? juce::Colour (0xff334155) : juce::Colour (0xff1e293b));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (n.bypassed ? juce::Colour (0xff64748b) : juce::Colour (0xff38bdf8));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.5f);

        g.setColour (juce::Colours::white);
        g.setFont (12.0f);
        g.drawText (n.name, r.reduced (8, 6).removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xffcbd5e1));
        g.setFont (10.5f);
        g.drawText ("P1:" + juce::String (n.paramA, 2) + "  P2:" + juce::String (n.paramB, 2),
                    r.reduced (8, 4).removeFromBottom (22), juce::Justification::centredLeft);
    }
}

void MainComponent::drawSamplerAndUiEdit (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto left = area.removeFromLeft (area.getWidth() / 2).reduced (14);
    auto right = area.reduced (14);

    g.setColour (juce::Colour (0xff1e293b));
    g.fillRoundedRectangle (left.toFloat(), 8.0f);
    g.fillRoundedRectangle (right.toFloat(), 8.0f);

    g.setColour (juce::Colours::white);
    g.setFont (13.0f);
    g.drawText ("KONTAKT SAMPLER MAPPING & ENVELOPE", left.removeFromTop (28).reduced (10, 4), juce::Justification::centredLeft);

    const auto& sc = samplerConfig;
    juce::String info =
        "App: PatchSampler Prototype v1.0.1\n"
        "Group: " + juce::String (sc.groupName) + "\n" +
        "Key Range: " + juce::String (sc.keyLow) + " - " + juce::String (sc.keyHigh) + " (Root: " + juce::String (sc.rootNote) + ")\n" +
        "Velocity: " + juce::String (sc.velLow) + " - " + juce::String (sc.velHigh) + "\n" +
        "Round-Robin Cycles: " + juce::String (sc.roundRobinCount) + "x\n" +
        "Loop Crossfade: " + juce::String (sc.loopCrossfadeMs, 1) + " ms\n" +
        "ADSR: A " + juce::String (sc.attackMs, 0) + "ms / D " + juce::String (sc.decayMs, 0) +
        "ms / S " + juce::String (sc.sustainLevel, 2) + " / R " + juce::String (sc.releaseMs, 0) + "ms";

    g.setColour (juce::Colour (0xffe2e8f0));
    g.setFont (11.5f);
    g.drawFittedText (info, left.reduced (12), juce::Justification::topLeft, 10);

    g.setColour (juce::Colours::white);
    g.setFont (13.0f);
    g.drawText ("FRONT PANEL GRID & CONTROL LINKER", right.removeFromTop (28).reduced (10, 4), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffe2e8f0));
    g.setFont (11.5f);
    g.drawFittedText ("Active Controls: " + juce::String ((int) controls.size()) +
                      "\nClick + KNOB / + H-FADER / + V-FADER above to add controls."
                      "\nDrag controls on Front Panel to adjust values."
                      "\nRight-click module in Patcher Grid to Bypass."
                      "\nAlt + Drag between modules to connect Patch Cables.",
                      right.reduced (12), juce::Justification::topLeft, 8);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f172a));
    auto content = getLocalBounds().withTrimmedTop (activeTab == 0 ? 44 : 80);

    if (activeTab == 0)
        drawFrontPanel (g, content);
    else if (activeTab == 1)
        drawPatcherGrid (g, content);
    else
        drawSamplerAndUiEdit (g, content);
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    auto content = getLocalBounds().withTrimmedTop (activeTab == 0 ? 44 : 80);
    if (!content.contains (e.getPosition()))
        return;

    if (activeTab == 0)
    {
        for (auto& c : controls)
        {
            juce::Rectangle<int> box (
                content.getX() + 16 + c.gridX * 110,
                content.getY() + 16 + c.gridY * 110,
                c.spanW * 102,
                c.spanH * 102
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
                content.getX() + 20 + n.gridX * 190,
                content.getY() + 20 + n.gridY * 125,
                165,
                92
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
    auto content = getLocalBounds().withTrimmedTop (activeTab == 0 ? 44 : 80);

    if (activeTab == 0 && draggedControlId != -1)
    {
        for (const auto& c : controls)
        {
            if (c.id == draggedControlId && c.kind != "Switch")
            {
                float delta = (float) (e.getMouseDownY() - e.getPosition().y) / 90.0f;
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
                int relX = std::max (0, e.getPosition().x - content.getX() - 20);
                int relY = std::max (0, e.getPosition().y - content.getY() - 20);
                n.gridX = showGridInRouting ? (relX / 190) : (relX / 40);
                n.gridY = showGridInRouting ? (relY / 125) : (relY / 40);
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
        auto content = getLocalBounds().withTrimmedTop (80);
        for (const auto& n : nodes)
        {
            if (!n.loaded) continue;
            juce::Rectangle<int> r (
                content.getX() + 20 + n.gridX * 190,
                content.getY() + 20 + n.gridY * 125,
                165,
                92
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
