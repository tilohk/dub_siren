#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
namespace colours
{
    inline const juce::Colour background { 0xff15171a };
    inline const juce::Colour panel      { 0xff1f2226 };
    inline const juce::Colour panelEdge  { 0xff2c3036 };
    inline const juce::Colour track      { 0xff33373e };
    inline const juce::Colour text       { 0xffd9dcd6 };
    inline const juce::Colour dimText    { 0xff8a8f94 };
    inline const juce::Colour accent     { 0xfff2b33d };   // amber
    inline const juce::Colour trigger    { 0xffe0412f };   // red
} // namespace colours

class SirenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SirenLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, colours::dimText);
        setColour (juce::ComboBox::backgroundColourId, colours::track);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::accent);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, colours::background);
        setColour (juce::ToggleButton::textColourId, colours::text);
        setColour (juce::ToggleButton::tickColourId, colours::accent);
        setColour (juce::ToggleButton::tickDisabledColourId, colours::dimText);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float lineW = 3.5f;
        const float arcR = radius - lineW * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool enabled = slider.isEnabled();

        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::track);
        g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Bipolar parameters (range crossing zero) draw their arc from the centre.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? startAngle + static_cast<float> (slider.valueToProportionOfLength (0.0)) * (endAngle - startAngle)
                                        : startAngle;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        g.setColour (enabled ? colours::accent : colours::dimText);
        g.strokePath (arc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float knobR = arcR - 6.0f;
        g.setColour (colours::panelEdge);
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
        g.setColour (enabled ? colours::text : colours::dimText);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
};
} // namespace ui
