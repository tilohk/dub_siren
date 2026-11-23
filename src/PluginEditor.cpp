#include "PluginEditor.h"
#include "params/Parameters.h"

namespace
{
constexpr int kCellW = 76;
constexpr int kCellH = 96;
constexpr int kSectionPad = 10;
constexpr int kSectionTitleH = 22;
constexpr int kSectionH = kSectionTitleH + kCellH + kSectionPad;
constexpr int kGap = 10;
constexpr int kMargin = 16;
constexpr int kHeaderH = 44;
constexpr int kTriggerW = 230;

int sectionWidth (int cells) { return cells * kCellW + 2 * kSectionPad; }

// Width of the control rows: SIREN (2 cells), LFO (3) and DECAY (2) side by side.
constexpr int kRowW = 7 * kCellW + 6 * kSectionPad + 2 * kGap;
} // namespace

//==============================================================================
ControlCell::ControlCell (const juce::String& labelText)
{
    label.setText (labelText, juce::dontSendNotification);
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
    if (dynamic_cast<juce::ComboBox*> (control) != nullptr)
        control->setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 24).withY (r.getY() + 22));
    else
        control->setBounds (r);
}

//==============================================================================
TriggerButton::TriggerButton (juce::RangedAudioParameter& triggerParam, std::atomic<bool>& gateActive)
    : param (triggerParam), gate (gateActive)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TriggerButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    auto circle = r.withSizeKeepingCentre (size, size);

    g.setColour (ui::colours::panelEdge);
    g.fillEllipse (circle);

    auto cap = circle.reduced (size * 0.09f);
    const auto base = lit ? ui::colours::trigger : ui::colours::trigger.darker (1.6f);
    g.setGradientFill (juce::ColourGradient (base.brighter (lit ? 0.35f : 0.1f), cap.getCentreX(), cap.getY(),
                                             base.darker (0.4f), cap.getCentreX(), cap.getBottom(), false));
    g.fillEllipse (cap);

    if (lit)
    {
        g.setColour (ui::colours::trigger.withAlpha (0.35f));
        g.drawEllipse (circle.reduced (1.0f), 3.0f);
    }

    g.setColour (lit ? juce::Colours::white : ui::colours::text.withAlpha (0.7f));
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("TRIGGER", cap, juce::Justification::centred);
}

void TriggerButton::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    param.beginChangeGesture();
    param.setValueNotifyingHost (1.0f);
    refresh();
}

void TriggerButton::mouseUp (const juce::MouseEvent&)
{
    if (! pressed)
        return;
    pressed = false;
    param.setValueNotifyingHost (0.0f);
    param.endChangeGesture();
    refresh();
}

void TriggerButton::refresh()
{
    const bool nowLit = pressed || gate.load();
    if (nowLit != lit)
    {
        lit = nowLit;
        repaint();
    }
}

//==============================================================================
DubSirenEditor::DubSirenEditor (DubSirenProcessor& p)
    : AudioProcessorEditor (p),
      sirenProcessor (p),
      trigger (*p.apvts.getParameter (params::id::trigger), p.gateActive)
{
    setLookAndFeel (&lnf);
    using namespace params::id;

    addCombo (source, wave, "Wave");
    addKnob (source, pitch, "Pitch");

    addCombo (lfo, lfoShape, "Shape");
    addKnob (lfo, lfoRate, "Rate");
    addKnob (lfo, lfoDepth, "Depth");

    addKnob (decay, release, "Release");
    addKnob (decay, sweep, "Sweep");

    addToggles (echo, { { delaySync, "Sync" } });
    echoTimeCell = &addKnob (echo, delayTime, "Time");
    echoDivCell = &addKnob (echo, delayDivision, "Time");
    addKnob (echo, feedback, "Feedback");
    addKnob (echo, tapeAmount, "Tape");
    addKnob (echo, highCut, "Tone");
    addKnob (echo, echoLevel, "Level");

    addKnob (output, params::id::output, "Volume");

    // The synced and free echo time controls share a slot: only one is shown at a time.
    echo.cells.erase (std::remove (echo.cells.begin(), echo.cells.end(), echoDivCell), echo.cells.end());

    addAndMakeVisible (trigger);

    setSize (2 * kMargin + kRowW + kGap + kTriggerW, kHeaderH + 2 * kSectionH + kGap + kMargin);
    timerCallback();
    startTimerHz (30);
}

DubSirenEditor::~DubSirenEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ControlCell& DubSirenEditor::addKnob (Section& section, const char* paramId, const juce::String& label)
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

ControlCell& DubSirenEditor::addCombo (Section& section, const char* paramId, const juce::String& label)
{
    auto cell = std::make_unique<ControlCell> (label);
    auto combo = std::make_unique<juce::ComboBox>();
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (sirenProcessor.apvts.getParameter (paramId)))
        combo->addItemList (choiceParam->choices, 1);
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

// A cell holding a vertical stack of toggle buttons.
ControlCell& DubSirenEditor::addToggles (Section& section, std::initializer_list<std::pair<const char*, const char*>> toggles)
{
    struct Stack : juce::Component
    {
        std::vector<juce::ToggleButton*> buttons;
        void resized() override
        {
            auto r = getLocalBounds().withTrimmedTop (6);
            for (auto* b : buttons)
                b->setBounds (r.removeFromTop (26).reduced (4, 0));
        }
    };

    auto cell = std::make_unique<ControlCell> ("");
    auto stack = std::make_unique<Stack>();
    for (const auto& [paramId, text] : toggles)
    {
        auto button = std::make_unique<juce::ToggleButton> (text);
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (sirenProcessor.apvts, paramId, *button));
        stack->buttons.push_back (button.get());
        stack->addAndMakeVisible (*button);
        owned.push_back (std::move (button));
    }
    cell->control = stack.get();
    cell->addAndMakeVisible (*stack);
    owned.push_back (std::move (stack));

    auto& ref = *cell;
    addAndMakeVisible (ref);
    section.cells.push_back (&ref);
    owned.push_back (std::move (cell));
    return ref;
}

void DubSirenEditor::layoutSection (Section& section, juce::Rectangle<int> area)
{
    section.bounds = area;
    auto r = area.reduced (kSectionPad, 0).withTrimmedTop (kSectionTitleH).withHeight (kCellH);
    const int used = static_cast<int> (section.cells.size()) * kCellW;
    r.removeFromLeft ((r.getWidth() - used) / 2);
    for (auto* cell : section.cells)
        cell->setBounds (r.removeFromLeft (kCellW));
}

void DubSirenEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 0).withTrimmedTop (kHeaderH).withTrimmedBottom (kMargin);

    trigger.setBounds (area.removeFromRight (kTriggerW).reduced (8));
    area.removeFromRight (kGap);

    auto row1 = area.removeFromTop (kSectionH);
    layoutSection (source, row1.removeFromLeft (sectionWidth (2)));
    row1.removeFromLeft (kGap);
    layoutSection (lfo, row1.removeFromLeft (sectionWidth (3)));
    row1.removeFromLeft (kGap);
    layoutSection (decay, row1);

    area.removeFromTop (kGap);
    auto row2 = area.removeFromTop (kSectionH);
    layoutSection (echo, row2.removeFromLeft (sectionWidth (6)));
    row2.removeFromLeft (kGap);
    layoutSection (output, row2);

    echoDivCell->setBounds (echoTimeCell->getBounds());
}

void DubSirenEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);

    auto header = getLocalBounds().removeFromTop (kHeaderH).reduced (kMargin, 0);
    g.setColour (ui::colours::accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("DUB SIREN", header, juce::Justification::centredLeft);
    g.setColour (ui::colours::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v" + juce::String (JucePlugin_VersionString), header, juce::Justification::centredRight);

    for (const auto* s : { &source, &lfo, &decay, &echo, &output })
    {
        auto b = s->bounds.toFloat();
        g.setColour (ui::colours::panel);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (ui::colours::panelEdge);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (ui::colours::dimText);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (s->title, s->bounds.withHeight (kSectionTitleH).reduced (kSectionPad, 0),
                    juce::Justification::centredLeft);
    }
}

void DubSirenEditor::timerCallback()
{
    const bool echoSynced = sirenProcessor.apvts.getRawParameterValue (params::id::delaySync)->load() > 0.5f;
    echoTimeCell->setVisible (! echoSynced);
    echoDivCell->setVisible (echoSynced);

    trigger.refresh();
}
