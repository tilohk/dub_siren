#pragma once

#include "PluginProcessor.h"
#include "ui/SirenLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

// A labelled cell in a section: knob, combo box or a stack of toggles.
class ControlCell : public juce::Component
{
public:
    ControlCell (const juce::String& labelText);
    void resized() override;
    void setLabelText (const juce::String& text) { label.setText (text.toUpperCase(), juce::dontSendNotification); }
    juce::Component* control = nullptr;

private:
    juce::Label label;
};

// A hardware-style push button driving a bool parameter: the big black TRIGGER or a small
// black button with an LED above it. Momentary buttons are on while held; latching buttons
// toggle on each click.
class PerformButton : public juce::Component
{
public:
    enum class Style { Big, Small };
    enum class Mode { Momentary, Latching };

    PerformButton (juce::RangedAudioParameter& param, juce::String text, Style, Mode, juce::Colour colour,
                   std::atomic<bool>* activeFlag = nullptr);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    void setGlow (bool shouldGlow) { glow = shouldGlow; repaint(); }

private:
    juce::RangedAudioParameter& param;
    juce::String text;
    Style style;
    Mode mode;
    juce::Colour colour;
    std::atomic<bool>* activeFlag;
    bool pressed = false;
    bool lit = false;
    bool glow = true;
};

// Red rotating siren lamp that lights up while the siren is gated (button or MIDI).
class BeaconLight : public juce::Component
{
public:
    BeaconLight() { setInterceptsMouseClicks (false, false); }
    void setActive (bool on);   // called at the UI frame rate; animates while on
    void paint (juce::Graphics&) override;

private:
    bool active = false;
    double phase = 0.0;
};

// Preset selector in the header: a black display with the preset name between two arrow
// buttons. Clicking the display lists all presets.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (DubSirenProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();   // called at the UI frame rate

private:
    juce::Rectangle<float> prevArea() const { return getLocalBounds().toFloat().removeFromLeft (static_cast<float> (getHeight())); }
    juce::Rectangle<float> nextArea() const { return getLocalBounds().toFloat().removeFromRight (static_cast<float> (getHeight())); }
    void load (int index);

    DubSirenProcessor& processor;
    int shown = -1;
};

// Everything on screen, laid out at its natural size.
class SirenPanel : public juce::Component, private juce::Timer
{
public:
    explicit SirenPanel (DubSirenProcessor&);
    ~SirenPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section
    {
        juce::String title;
        std::vector<ControlCell*> cells;
        juce::Rectangle<int> bounds;
        juce::Colour barColour;
    };

    void timerCallback() override;
    void paintPanel (juce::Graphics&, const Section&, int seed);

    ControlCell& addKnob (Section&, const char* paramId, const juce::String& label);
    ControlCell& addCombo (Section&, const char* paramId, const juce::String& label);
    void layoutSection (Section&, juce::Rectangle<int> area);
    void layoutEchoGrid (juce::Rectangle<int> area);

    DubSirenProcessor& sirenProcessor;
    ui::SirenLookAndFeel lnf;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;

    Section source { "SIREN", {}, {}, ui::colours::rastaRed };
    Section lfo { "LFO", {}, {}, ui::colours::dubOrange };
    Section decay { "DECAY", {}, {}, ui::colours::rastaGold };
    Section echo { "ECHO", {}, {}, ui::colours::rastaGreen };
    Section output { "OUT", {}, {}, ui::colours::bakelite };

    ControlCell* echoTimeCell = nullptr;
    ControlCell* lfoRateCell = nullptr;
    ControlCell* lfoAccelCell = nullptr;
    ControlCell* echoDivCell = nullptr;

    PerformButton trigger, throwButton, killButton, freezeButton, syncButton;
    BeaconLight beacon;
    PresetBar presetBar;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SirenPanel)
};

// The plug-in window: shows the panel scaled down by kUiScale.
class DubSirenEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr float kUiScale = 0.85f;

    explicit DubSirenEditor (DubSirenProcessor&);

private:
    SirenPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubSirenEditor)
};
