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
    void setLabelText (const juce::String& text) { label.setText (text, juce::dontSendNotification); }
    juce::Component* control = nullptr;

private:
    juce::Label label;
};

// A performance button driving a bool parameter: the big round TRIGGER or a small pill.
// Momentary buttons are on while held; latching buttons toggle on each click.
class PerformButton : public juce::Component
{
public:
    enum class Style { Round, Pill };
    enum class Mode { Momentary, Latching };

    PerformButton (juce::RangedAudioParameter& param, juce::String text, Style, Mode, juce::Colour colour,
                   std::atomic<bool>* activeFlag = nullptr);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

private:
    juce::RangedAudioParameter& param;
    juce::String text;
    Style style;
    Mode mode;
    juce::Colour colour;
    std::atomic<bool>* activeFlag;
    bool pressed = false;
    bool lit = false;
};

class DubSirenEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DubSirenEditor (DubSirenProcessor&);
    ~DubSirenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section
    {
        juce::String title;
        std::vector<ControlCell*> cells;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;

    ControlCell& addKnob (Section&, const char* paramId, const juce::String& label);
    ControlCell& addCombo (Section&, const char* paramId, const juce::String& label);
    ControlCell& addToggles (Section&, std::initializer_list<std::pair<const char*, const char*>> toggles);
    void layoutSection (Section&, juce::Rectangle<int> area);

    DubSirenProcessor& sirenProcessor;
    ui::SirenLookAndFeel lnf;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    Section source { "SIREN", {}, {} };
    Section lfo { "LFO", {}, {} };
    Section decay { "DECAY", {}, {} };
    Section echo { "ECHO", {}, {} };
    Section output { "OUT", {}, {} };

    ControlCell* echoTimeCell = nullptr;
    ControlCell* lfoRateCell = nullptr;
    ControlCell* lfoAccelCell = nullptr;
    ControlCell* echoDivCell = nullptr;

    PerformButton trigger, throwButton, killButton, freezeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubSirenEditor)
};
