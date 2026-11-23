#include "PluginEditor.h"
#include "params/Parameters.h"
#include "params/Presets.h"

namespace
{
constexpr int kCellW = 94;
constexpr int kCellH = 120;
constexpr int kSectionPad = 10;
constexpr int kSectionTitleH = 22;
constexpr int kTitleBarH = 20;   // black or white strip holding a section title
constexpr int kGap = 10;
constexpr int kMargin = 16;
constexpr int kHeaderTop = 14;   // caption like the other plug-ins: 30 px high, right-aligned
constexpr int kCaptionH = 30;
constexpr int kHeaderH = kHeaderTop + kCaptionH + 12;
constexpr int kOutCellH = 104;   // the OUT panel's knob is a little smaller

// Left: SIREN and DECAY side by side, LFO below at their combined width.
constexpr int kLeftW = 2 * (2 * kCellW + 2 * kSectionPad) + kGap;
// Right of it: ECHO, two columns by three rows, as tall as the left column.
constexpr int kEchoPad = 16;
constexpr int kEchoW = 2 * kCellW + 2 * kEchoPad;
constexpr int kBodyH = kSectionTitleH + 3 * kCellH + kSectionPad;
constexpr int kPerformW = 220;
constexpr int kBodyW = kLeftW + kGap + kEchoW + kGap + kPerformW;
} // namespace

//==============================================================================
ControlCell::ControlCell (const juce::String& labelText)
{
    setLabelText (labelText);
    label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void ControlCell::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    if (control == nullptr)
        return;
    control->setBounds (r);
}

//==============================================================================
PerformButton::PerformButton (juce::RangedAudioParameter& p, juce::String t, Style st, Mode m, juce::Colour c,
                              std::atomic<bool>* flag)
    : param (p), text (std::move (t)), style (st), mode (m), colour (c), activeFlag (flag)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PerformButton::paint (juce::Graphics& g)
{
    using namespace ui;
    auto r = getLocalBounds().toFloat();

    if (style == Style::Small)
    {
        drawLed (g, r.removeFromTop (14.0f).getCentre(), 4.0f, colour, lit, glow);
        r.removeFromTop (4.0f);
        r = r.reduced (1.0f, 0.0f).withTrimmedBottom (1.0f);

        drawChromeBezel (g, r, 4.0f);
        auto well = r.reduced (3.0f);
        drawWell (g, well, 3.0f);
        auto cap = well.reduced (2.0f).translated (0.0f, lit ? 1.0f : -0.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), 0.0f, cap.getY(),
                                                 juce::Colour (0xff0e0e0e), 0.0f, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.08f : 0.18f));
        g.drawLine (cap.getX() + 3.0f, cap.getY() + 1.0f, cap.getRight() - 3.0f, cap.getY() + 1.0f, 1.0f);
        g.setColour (colours::silver.withAlpha (lit ? 1.0f : 0.85f));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)).withExtraKerningFactor (0.1f));
        g.drawText (text, cap, juce::Justification::centred);
        return;
    }

    // Big push button: a raised black cap whose face bulges outwards. It sinks a little
    // while held; the beacon above it shows the gate.
    auto cap = r.reduced (6.0f, 4.0f).withTrimmedBottom (6.0f);
    const float travel = lit ? 2.5f : 0.0f;
    constexpr float corner = 14.0f;

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (cap.expanded (3.0f).translated (0.0f, lit ? 3.0f : 7.0f), corner + 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.expanded (1.0f).translated (0.0f, lit ? 1.5f : 3.5f), corner + 1.0f);
    cap = cap.translated (0.0f, travel);

    // Body: lit from above, sides falling into shadow.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3c3c), 0.0f, cap.getY(),
                                             juce::Colour (0xff070707), 0.0f, cap.getBottom(), false));
    g.fillRoundedRectangle (cap, corner);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0.0f, cap.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0.0f, cap.getCentreY(), false));
    g.drawRoundedRectangle (cap.reduced (0.7f), corner, 1.3f);

    // Domed face: brightest a little above the middle, darkening towards its edges.
    auto face = cap.reduced (cap.getWidth() * 0.06f, 0.0f).withTrimmedTop (cap.getHeight() * 0.06f)
                   .withTrimmedBottom (cap.getHeight() * 0.16f);
    juce::Path facePath;
    facePath.addRoundedRectangle (face, corner * 0.75f);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (facePath);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff474747), face.getCentreX(), face.getY() + face.getHeight() * 0.3f,
                                                 juce::Colour (0xff111111), face.getX(), face.getBottom(), true));
        g.fillRect (face);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.1f), 0.0f, face.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, face.getY() + face.getHeight() * 0.45f, false));
        g.fillRect (face);
    }
    // Light catches the top of the bulge; its lower edge throws a shadow onto the body.
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (face.getX() + corner * 0.6f, face.getBottom() + 0.8f, face.getRight() - corner * 0.6f, face.getBottom() + 0.8f, 1.6f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (face.getX() + corner * 0.6f, face.getY() + 0.6f, face.getRight() - corner * 0.6f, face.getY() + 0.6f, 1.0f);

    // Thin white border hugging the cap.
    g.setColour (colours::cream);
    g.drawRoundedRectangle (cap.expanded (0.75f), corner + 0.75f, 1.5f);
}

//==============================================================================
void BeaconLight::setActive (bool on)
{
    if (on)
        phase = std::fmod (phase + 0.2, juce::MathConstants<double>::twoPi);   // about 1 turn per second at 30 Hz
    if (on || on != active)
        repaint();
    active = on;
}

// A rotating siren lamp seen from above: a red glass dome in a chrome ring. While active
// it glows and the reflector's beam sweeps round under the glass.
void BeaconLight::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float ringR = juce::jmin (b.getWidth(), b.getHeight()) * 0.34f;
    const float glassR = ringR * 0.78f;
    auto circle = [] (float r, juce::Point<float> p) { return juce::Rectangle<float> (2 * r, 2 * r).withCentre (p); };

    if (active)
    {
        // Fades out before the component's edges, so the glow is never cut off.
        const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        juce::ColourGradient halo (colours::ledRed.withAlpha (0.6f), c.x, c.y,
                                   colours::ledRed.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (circle (radius, c));
    }

    // Chrome ring with a shadow on the case.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (circle (ringR + 1.0f, c.translated (0.0f, 2.0f)));
    g.setGradientFill (juce::ColourGradient (colours::silverLight, c.x, c.y - ringR, colours::silverDark, c.x, c.y + ringR, false));
    g.fillEllipse (circle (ringR, c));
    g.setColour (colours::ink.withAlpha (0.6f));
    g.drawEllipse (circle (ringR, c), 0.8f);
    g.drawEllipse (circle (glassR + 0.8f, c), 1.0f);

    // Glass dome: brightest towards the light, darker at the rim.
    const auto glass = active ? colours::ledRed : colours::red.darker (1.2f);
    g.setGradientFill (juce::ColourGradient (glass.brighter (active ? 0.5f : 0.35f), c.x - glassR * 0.25f, c.y - glassR * 0.3f,
                                             glass.darker (0.7f), c.x + glassR, c.y + glassR, true));
    g.fillEllipse (circle (glassR, c));

    juce::Path glassPath;
    glassPath.addEllipse (circle (glassR, c));
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (glassPath);
        if (active)
        {
            // Reflector beam: a bright wedge turning around the bulb.
            const float a = static_cast<float> (phase);
            juce::Path beam;
            beam.addPieSegment (circle (glassR * 1.2f, c), a - 0.45f, a + 0.45f, 0.0f);
            juce::ColourGradient grad (juce::Colours::white.withAlpha (0.9f), c.x, c.y,
                                       colours::ledRed.withAlpha (0.0f), c.x + glassR, c.y, true);
            g.setGradientFill (grad);
            g.fillPath (beam);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillEllipse (circle (glassR * 0.18f, c));
        }
        // Fresnel rings in the glass.
        for (float r = glassR * 0.35f; r < glassR; r += glassR * 0.22f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.14f));
            g.drawEllipse (circle (r, c), 0.8f);
        }
    }

    // Reflection of the room light on the dome.
    g.setColour (juce::Colours::white.withAlpha (active ? 0.5f : 0.35f));
    g.fillEllipse (juce::Rectangle<float> (glassR * 0.5f, glassR * 0.3f).withCentre (c.translated (-glassR * 0.35f, -glassR * 0.45f)));
}

void PerformButton::mouseDown (const juce::MouseEvent&)
{
    param.beginChangeGesture();
    if (mode == Mode::Momentary)
    {
        pressed = true;
        param.setValueNotifyingHost (1.0f);
    }
    else
    {
        param.setValueNotifyingHost (param.getValue() > 0.5f ? 0.0f : 1.0f);
        param.endChangeGesture();
    }
    refresh();
}

void PerformButton::mouseUp (const juce::MouseEvent&)
{
    if (! pressed)
        return;
    pressed = false;
    param.setValueNotifyingHost (0.0f);
    param.endChangeGesture();
    refresh();
}

void PerformButton::refresh()
{
    const bool nowLit = pressed || param.getValue() > 0.5f || (activeFlag != nullptr && activeFlag->load());
    if (nowLit != lit)
    {
        lit = nowLit;
        repaint();
    }
}

//==============================================================================
SirenPanel::SirenPanel (DubSirenProcessor& p)
    : sirenProcessor (p),
      trigger (*p.apvts.getParameter (params::id::trigger), "TRIGGER", PerformButton::Style::Big,
               PerformButton::Mode::Momentary, ui::colours::bakelite, &p.gateActive),
      throwButton (*p.apvts.getParameter (params::id::echoThrow), "THROW", PerformButton::Style::Small,
                   PerformButton::Mode::Momentary, ui::colours::ledRed),
      killButton (*p.apvts.getParameter (params::id::echoKill), "KILL", PerformButton::Style::Small,
                  PerformButton::Mode::Momentary, ui::colours::ledRed),
      freezeButton (*p.apvts.getParameter (params::id::echoFreeze), "FREEZE", PerformButton::Style::Small,
                    PerformButton::Mode::Latching, ui::colours::ledGreen),
      syncButton (*p.apvts.getParameter (params::id::delaySync), "SYNC", PerformButton::Style::Small,
                  PerformButton::Mode::Latching, ui::colours::ledGreen),
      presetBar (p)
{
    using namespace params::id;

    addCombo (source, wave, "Wave");
    addKnob (source, pitch, "Pitch");

    addCombo (lfo, lfoShape, "Shape");
    lfoRateCell = &addKnob (lfo, lfoRate, "Rate");
    addKnob (lfo, lfoDepth, "Depth");
    lfoAccelCell = &addKnob (lfo, lfoAccel, "Accel");

    addKnob (decay, release, "Release");
    addKnob (decay, sweep, "Sweep");

    echoTimeCell = &addKnob (echo, delayTime, "Time");
    echoDivCell = &addKnob (echo, delayDivision, "Time");
    addKnob (echo, highCut, "Tone");
    addKnob (echo, feedback, "Feedback");
    addKnob (echo, tapeAmount, "Tape");
    addKnob (echo, echoLevel, "Level");

    addKnob (output, params::id::drive, "Drive");
    addKnob (output, params::id::output, "Volume");
    for (auto* cell : output.cells)
        cell->control->getProperties().set ("plainCap", true);

    // The synced and free echo time controls share a slot: only one is shown at a time.
    echo.cells.erase (std::remove (echo.cells.begin(), echo.cells.end(), echoDivCell), echo.cells.end());

    for (auto* b : { &trigger, &throwButton, &killButton, &freezeButton, &syncButton })
        addAndMakeVisible (*b);
    addAndMakeVisible (presetBar);
    presetBar.refresh();
    freezeButton.setGlow (false);
    syncButton.setGlow (false);
    addAndMakeVisible (beacon);

    // Set last so every child (including the sliders' value boxes) is rebuilt with it.
    setLookAndFeel (&lnf);

    setSize (2 * kMargin + kBodyW, kHeaderH + kBodyH + kMargin);
    timerCallback();
    startTimerHz (30);
}

SirenPanel::~SirenPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ControlCell& SirenPanel::addKnob (Section& section, const char* paramId, const juce::String& label)
{
    auto cell = std::make_unique<ControlCell> (label);
    auto slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, kCellW, 16);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (sirenProcessor.apvts, paramId, *slider));
    cell->control = slider.get();
    cell->addAndMakeVisible (*slider);
    owned.push_back (std::move (slider));

    auto& ref = *cell;
    addAndMakeVisible (ref);
    section.cells.push_back (&ref);
    owned.push_back (std::move (cell));
    return ref;
}

ControlCell& SirenPanel::addCombo (Section& section, const char* paramId, const juce::String& label)
{
    auto cell = std::make_unique<ControlCell> (label);
    auto combo = std::make_unique<juce::ComboBox>();
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (sirenProcessor.apvts.getParameter (paramId)))
        ui::SirenLookAndFeel::addIconItems (*combo, choiceParam->choices);
    combo->getProperties().set ("icons", true);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (sirenProcessor.apvts, paramId, *combo));
    cell->control = combo.get();
    cell->addAndMakeVisible (*combo);
    owned.push_back (std::move (combo));

    auto& ref = *cell;
    addAndMakeVisible (ref);
    section.cells.push_back (&ref);
    owned.push_back (std::move (cell));
    return ref;
}

void SirenPanel::layoutSection (Section& section, juce::Rectangle<int> area)
{
    section.bounds = area;
    auto r = area.reduced (kSectionPad, 0).withTrimmedTop (kSectionTitleH).withTrimmedBottom (kSectionPad);
    // Shifted down a little: the value text sits close under each knob, which leaves the
    // bottom of a cell empty.
    r = r.withSizeKeepingCentre (static_cast<int> (section.cells.size()) * kCellW, kCellH).translated (0, 7);
    for (auto* cell : section.cells)
        cell->setBounds (r.removeFromLeft (kCellW));
}

// Two columns, three rows: Sync, Time / Tone, Feedback / Tape, Level.
void SirenPanel::layoutEchoGrid (juce::Rectangle<int> area)
{
    echo.bounds = area;
    auto grid = area.reduced (kEchoPad, 0).withTrimmedTop (kSectionTitleH).withTrimmedBottom (kSectionPad);
    auto slot = [&] (int index) {
        return juce::Rectangle<int> (grid.getX() + (index % 2) * kCellW, grid.getY() + (index / 2) * kCellH, kCellW, kCellH);
    };
    for (size_t i = 0; i < echo.cells.size(); ++i)
        echo.cells[i]->setBounds (slot (static_cast<int> (i) + 1));
    echoDivCell->setBounds (echoTimeCell->getBounds());

    // Sync sits where a knob would be, its button level with the knobs beside it.
    syncButton.setBounds (slot (0).withTrimmedTop (16).withTrimmedBottom (16).withSizeKeepingCentre (64, 50));
}

void SirenPanel::resized()
{
    presetBar.setBounds (kMargin, kHeaderTop + (kCaptionH - 22) / 2, 220, 22);
    auto area = getLocalBounds().reduced (kMargin, 0).withTrimmedTop (kHeaderH).withTrimmedBottom (kMargin);

    // Perform column: OUT panel on top, then the trigger and the echo buttons on the case.
    auto perform = area.removeFromRight (kPerformW);
    area.removeFromRight (kGap);
    layoutSection (output, perform.removeFromTop (kSectionTitleH + kOutCellH + kSectionPad));
    for (auto* cell : output.cells)
        cell->setSize (kCellW, kOutCellH);
    {
        int x = output.bounds.getCentreX() - static_cast<int> (output.cells.size()) * kCellW / 2;
        for (auto* cell : output.cells)
        {
            cell->setTopLeftPosition (x, output.bounds.getY() + kSectionTitleH);
            x += kCellW;
        }
    }
    perform.removeFromTop (8);
    auto buttons = perform.removeFromTop (50);
    const int buttonW = buttons.getWidth() / 3;
    throwButton.setBounds (buttons.removeFromLeft (buttonW).reduced (4, 0));
    killButton.setBounds (buttons.removeFromLeft (buttonW).reduced (4, 0));
    freezeButton.setBounds (buttons.reduced (4, 0));
    // Lamp and trigger shift down so the cap's lower edge lines up with the bottom of the
    // panels (the trigger component keeps room below the cap for its shadow).
    constexpr int capShadowRoom = 7;
    perform = perform.withTrimmedTop (capShadowRoom + 2).withBottom (perform.getBottom() + capShadowRoom);
    beacon.setBounds (perform.removeFromTop (48));
    perform.removeFromTop (2);
    trigger.setBounds (perform);

    layoutEchoGrid (area.removeFromRight (kEchoW));
    area.removeFromRight (kGap);

    const int rowH = (area.getHeight() - kGap) / 2;
    auto top = area.removeFromTop (rowH);
    layoutSection (source, top.removeFromLeft ((top.getWidth() - kGap) / 2));
    top.removeFromLeft (kGap);
    layoutSection (decay, top);
    area.removeFromTop (kGap);
    layoutSection (lfo, area);

    background = ui::makeCaseBackground (getWidth(), getHeight());
}

void SirenPanel::paintPanel (juce::Graphics& g, const Section& section, int seed)
{
    lnf.drawMetalPanel (g, section.bounds, seed, static_cast<float> (kTitleBarH));
    ui::SirenLookAndFeel::drawPanelTitle (g, section.title, section.bounds.withHeight (kTitleBarH), section.barColour);
}

void SirenPanel::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    // Caption as in the other plug-ins: Avenir Next Demi Bold 25, top right, version to its left.
    const auto header = getLocalBounds().withTrimmedTop (kHeaderTop).removeFromTop (kCaptionH).reduced (kMargin, 0);
    const juce::Font titleFont (juce::FontOptions().withName ("Avenir Next").withStyle ("Demi Bold").withHeight (25.0f));
    g.setColour (ui::colours::cream);
    g.setFont (titleFont);
    g.drawText (JucePlugin_Name, header, juce::Justification::centredRight);

    const int titleW = static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (titleFont, JucePlugin_Name)));
    g.setColour (ui::colours::silver.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" + juce::String (JucePlugin_VersionString), header.withTrimmedRight (titleW + 10),
                juce::Justification::centredRight);

    int seed = 1;
    for (const auto* s : { &source, &lfo, &decay, &echo, &output })
        paintPanel (g, *s, seed++);
}

void SirenPanel::timerCallback()
{
    const bool echoSynced = sirenProcessor.apvts.getRawParameterValue (params::id::delaySync)->load() > 0.5f;
    echoTimeCell->setVisible (! echoSynced);
    echoDivCell->setVisible (echoSynced);

    // In Rotor mode the Rate knob sets the spin-up speed, and Accel has no effect.
    const bool rotor = juce::roundToInt (sirenProcessor.apvts.getRawParameterValue (params::id::lfoShape)->load())
                       == params::rotorShapeIndex;
    lfoRateCell->setLabelText (rotor ? "Spin-up" : "Rate");
    lfoAccelCell->setEnabled (! rotor);

    for (auto* b : { &trigger, &throwButton, &killButton, &freezeButton, &syncButton })
        b->refresh();
    presetBar.refresh();
    beacon.setActive (sirenProcessor.gateActive.load());
}

//==============================================================================
void PresetBar::load (int index)
{
    processor.setCurrentProgram ((index + params::numPresets) % params::numPresets);
    refresh();
}

void PresetBar::refresh()
{
    if (const int current = processor.getCurrentProgram(); current != shown)
    {
        shown = current;
        repaint();
    }
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    if (prevArea().contains (pos))
        return load (shown - 1);
    if (nextArea().contains (pos))
        return load (shown + 1);

    juce::PopupMenu menu;
    for (int i = 0; i < params::numPresets; ++i)
        menu.addItem (i + 1, params::presets[i].name, true, i == shown);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [safe = juce::Component::SafePointer<PresetBar> (this)] (int result) {
                            if (safe != nullptr && result > 0)
                                safe->load (result - 1);
                        });
}

void PresetBar::paint (juce::Graphics& g)
{
    using namespace ui;
    // One slim black display with the arrows at its ends.
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = r.getHeight() * 0.5f;
    g.setColour (colours::bakelite);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::silver.withAlpha (0.35f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    g.setColour (colours::silver.withAlpha (0.8f));
    for (auto area : { prevArea(), nextArea() })
    {
        const bool left = area == prevArea();
        const auto c = area.getCentre().translated (left ? 2.0f : -2.0f, 0.0f);
        const float s = 4.0f;
        juce::Path arrow;
        arrow.addTriangle (c.x + (left ? -s : s), c.y, c.x + (left ? s : -s) * 0.6f, c.y - s, c.x + (left ? s : -s) * 0.6f, c.y + s);
        g.fillPath (arrow);
    }

    g.setColour (colours::cream);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (params::presets[juce::jlimit (0, params::numPresets - 1, shown)].name,
                getLocalBounds().reduced (getHeight(), 0), juce::Justification::centred);
}

//==============================================================================
DubSirenEditor::DubSirenEditor (DubSirenProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    // The panel is laid out at its natural size and drawn scaled down as a whole.
    addAndMakeVisible (panel);
    panel.setTransform (juce::AffineTransform::scale (kUiScale));
    setSize (juce::roundToInt (static_cast<float> (panel.getWidth()) * kUiScale),
             juce::roundToInt (static_cast<float> (panel.getHeight()) * kUiScale));
}
