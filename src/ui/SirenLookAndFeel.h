#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <vector>

// Vintage hardware look: green powder-coated case, brushed silver panels with
// engraved lettering, black bakelite knobs and a red push button.
namespace ui
{
namespace colours
{
    inline const juce::Colour caseGreen     { 0xff1f4d31 };
    inline const juce::Colour caseGreenDark { 0xff113020 };
    inline const juce::Colour silver        { 0xffc8cbce };
    inline const juce::Colour silverLight   { 0xfff1f2f3 };
    inline const juce::Colour silverDark    { 0xff83878c };
    inline const juce::Colour ink           { 0xff1a1b1c };   // engraved text on silver
    inline const juce::Colour cream         { 0xfff1ece0 };   // text on black and green
    inline const juce::Colour bakelite      { 0xff151515 };
    inline const juce::Colour red           { 0xffd2231a };
    inline const juce::Colour ledRed        { 0xffff3b2e };
    inline const juce::Colour ledGreen      { 0xff45f06e };
    // Rasta colours for the section titles.
    inline const juce::Colour rastaRed      { 0xffc81e14 };
    inline const juce::Colour rastaGold     { 0xffcc8f00 };
    inline const juce::Colour rastaGreen    { 0xff13863a };
    inline const juce::Colour dubOrange     { 0xffe2621b };   // fourth title colour, between red and gold
} // namespace colours

//==============================================================================
// Drawing helpers shared by the look-and-feel and the editor.

// Brushed metal: per-row noise smeared horizontally, plus fine grain.
inline juce::Image makeBrushedMetal (int w, int h, juce::int64 seed)
{
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Random rng (seed);
    juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    constexpr int radius = 28;
    std::vector<float> noise (static_cast<size_t> (w + 2 * radius + 1));
    for (int y = 0; y < h; ++y)
    {
        for (auto& n : noise)
            n = rng.nextFloat() - 0.5f;
        const float rowBias = (rng.nextFloat() - 0.5f) * 0.01f;

        float sum = 0.0f;
        for (int i = 0; i < 2 * radius + 1; ++i)
            sum += noise[static_cast<size_t> (i)];

        for (int x = 0; x < w; ++x)
        {
            const float streak = sum / static_cast<float> (2 * radius + 1) * 0.45f;
            const float v = juce::jlimit (0.0f, 1.0f, 0.79f + rowBias + streak + (rng.nextFloat() - 0.5f) * 0.045f);
            data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (v * 0.985f, v * 0.99f, v, 1.0f));
            sum += noise[static_cast<size_t> (x + 2 * radius + 1)] - noise[static_cast<size_t> (x)];
        }
    }
    return img;
}

// Painted case: vertical green gradient, fine grain and darkened edges.
inline juce::Image makeCaseBackground (int w, int h)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
    {
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (colours::caseGreen.brighter (0.12f), 0.0f, 0.0f,
                                                 colours::caseGreen.darker (0.25f), 0.0f, static_cast<float> (h), false));
        g.fillAll();

        juce::ColourGradient vignette (juce::Colours::transparentBlack, static_cast<float> (w) * 0.5f, static_cast<float> (h) * 0.45f,
                                       juce::Colours::black.withAlpha (0.35f), 0.0f, 0.0f, true);
        g.setGradientFill (vignette);
        g.fillAll();
    }

    juce::Random rng (7);
    juce::Image::BitmapData data (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
        {
            const float d = (rng.nextFloat() - 0.5f) * 0.06f;
            auto c = data.getPixelColour (x, y);
            data.setPixelColour (x, y, d > 0 ? c.brighter (d) : c.darker (-d));
        }
    return img;
}

// Text stamped into metal: a light edge below, dark ink on top.
inline void drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                              juce::Justification just, float size, bool bold = true)
{
    g.setFont (juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (0.08f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText (text, area.translated (0.0f, 1.0f), just, false);
    g.setColour (colours::ink);
    g.drawText (text, area, just, false);
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (juce::Rectangle<float> (2 * r + 1.5f, 2 * r + 1.5f).withCentre (c.translated (0.3f, 0.9f)));
    g.setGradientFill (juce::ColourGradient (colours::silverLight, c.x - r, c.y - r,
                                             colours::silverDark, c.x + r, c.y + r, false));
    g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c));
    g.setColour (colours::silverDark.darker (0.5f));
    g.drawEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c), 0.6f);

    const auto d = juce::Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.8f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawLine ({ c - d + juce::Point<float> (0.0f, 0.7f), c + d + juce::Point<float> (0.0f, 0.7f) }, 1.0f);
    g.setColour (colours::ink.withAlpha (0.85f));
    g.drawLine ({ c - d, c + d }, 1.3f);
}

// Black rounded well with a soft inner shadow, e.g. behind a button cap.
inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (colours::bakelite.darker (0.6f));
    g.fillRoundedRectangle (r, corner);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), 0.0f, r.getY(),
                                             juce::Colours::transparentBlack, 0.0f, r.getY() + r.getHeight() * 0.3f, false));
    g.fillRoundedRectangle (r, corner);
}

// Polished chrome bezel (light from above).
inline void drawChromeBezel (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    juce::ColourGradient grad (colours::silverLight, 0.0f, r.getY(), colours::silverDark, 0.0f, r.getBottom(), false);
    grad.addColour (0.45, colours::silver);
    grad.addColour (0.55, colours::silverDark.brighter (0.25f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::ink.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

inline void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool lit, bool glow = true)
{
    if (lit && glow)
    {
        juce::ColourGradient halo (colour.withAlpha (0.55f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 3.0f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (c));
    }
    g.setColour (colours::ink);
    g.fillEllipse (juce::Rectangle<float> (2 * r + 2.0f, 2 * r + 2.0f).withCentre (c));
    const auto body = lit ? colour : colour.darker (2.2f).withMultipliedSaturation (0.6f);
    g.setGradientFill (juce::ColourGradient (body.brighter (lit ? 0.6f : 0.2f), c.x - r * 0.3f, c.y - r * 0.5f,
                                             body.darker (0.3f), c.x + r, c.y + r, true));
    g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (lit ? 0.7f : 0.25f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.7f, r * 0.5f).withCentre (c.translated (-r * 0.25f, -r * 0.4f)));
}

// Waveform icon for a wave or LFO shape name, drawn inside `r` (one shot for Rotor).
inline juce::Path waveIconPath (const juce::String& name, juce::Rectangle<float> r)
{
    std::function<float (float)> f; // t in [0, 1] -> height in [0, 1]
    auto frac = [] (float t) { return t * 2.0f - std::floor (t * 2.0f); };   // two cycles
    if (name == "Square")        f = [=] (float t) { return frac (t) < 0.5f ? 1.0f : 0.0f; };
    else if (name == "Saw" || name == "Saw Up") f = [=] (float t) { return frac (t); };
    else if (name == "Saw Down") f = [=] (float t) { return 1.0f - frac (t); };
    else if (name == "Triangle") f = [=] (float t) { const float p = frac (t); return p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; };
    else if (name == "Sine")     f = [] (float t) { return 0.5f + 0.5f * std::sin (t * 4.0f * juce::MathConstants<float>::pi); };
    else if (name == "S&H")      f = [] (float t) { static const float v[] { 0.35f, 0.9f, 0.1f, 0.65f, 1.0f, 0.3f };
                                                    return v[juce::jmin (5, static_cast<int> (t * 6.0f))]; };
    else if (name == "Rotor")    f = [] (float t) { return t < 0.55f ? (1.0f - std::exp (-t * 9.0f)) / (1.0f - std::exp (-4.95f))
                                                                     : std::exp (-(t - 0.55f) * 7.0f); };
    else                         f = [] (float) { return 0.5f; };

    juce::Path p;
    constexpr int n = 240;
    for (int i = 0; i <= n; ++i)
    {
        const float t = juce::jmin (static_cast<float> (i) / n, 0.9999f);
        const juce::Point<float> pt (r.getX() + t * r.getWidth(), r.getBottom() - f (t) * r.getHeight());
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    return p;
}

//==============================================================================
class SirenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SirenLookAndFeel()
    {
        using namespace colours;
        setColour (juce::Slider::textBoxTextColourId, ink);
        setColour (juce::Slider::rotarySliderOutlineColourId, ink);   // scale ticks
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, red.withAlpha (0.5f));
        setColour (juce::Label::textColourId, ink);
        setColour (juce::Label::textWhenEditingColourId, cream);
        setColour (juce::Label::backgroundWhenEditingColourId, bakelite);
        setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId, bakelite);
        setColour (juce::TextEditor::textColourId, cream);
        setColour (juce::TextEditor::highlightColourId, red.withAlpha (0.6f));
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::CaretComponent::caretColourId, cream);
        setColour (juce::ComboBox::backgroundColourId, bakelite);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, cream);
        setColour (juce::ComboBox::arrowColourId, red);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff181818));
        setColour (juce::PopupMenu::textColourId, silver);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, red);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, ink);
        setColour (juce::ToggleButton::tickColourId, ledGreen);
        setColour (juce::ToggleButton::tickDisabledColourId, silverDark);
    }

    // A brushed-silver plate with bevelled edges and corner screws. The top screws sit
    // below a title bar of height `titleBarH`.
    void drawMetalPanel (juce::Graphics& g, juce::Rectangle<int> bounds, int seed, float titleBarH = 0.0f)
    {
        const auto b = bounds.toFloat();
        constexpr float corner = 5.0f;

        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 10, { 0, 4 }).drawForRectangle (g, bounds);

        juce::Path plate;
        plate.addRoundedRectangle (b, corner);
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (plate);
            const int maxX = juce::jmax (0, brushed.getWidth() - bounds.getWidth());
            const int maxY = juce::jmax (0, brushed.getHeight() - bounds.getHeight());
            g.drawImage (brushed, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                         (seed * 97) % (maxX + 1), (seed * 41) % (maxY + 1), bounds.getWidth(), bounds.getHeight());

            // Light from above.
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0.0f, b.getY(),
                                                     juce::Colours::black.withAlpha (0.14f), 0.0f, b.getBottom(), false));
            g.fillPath (plate);
        }

        g.setColour (colours::ink.withAlpha (0.75f));
        g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.85f), 0.0f, b.getY(),
                                                 juce::Colours::black.withAlpha (0.25f), 0.0f, b.getBottom(), false));
        g.drawRoundedRectangle (b.reduced (1.5f), corner - 1.0f, 1.0f);

        constexpr float inset = 7.5f;
        const float topY = b.getY() + titleBarH + inset;
        const juce::Point<float> corners[] { { b.getX() + inset, topY },
                                             { b.getRight() - inset, topY },
                                             { b.getX() + inset, b.getBottom() - inset },
                                             { b.getRight() - inset, b.getBottom() - inset } };
        for (int i = 0; i < 4; ++i)
            drawScrew (g, corners[i], 3.6f, static_cast<float> ((seed * 4 + i) * 37 % 180) * 0.01745f);
    }

    // Title bar across the top of a panel: a filled strip (rounded like the panel's top
    // corners) with the title centred in it, in white.
    static void drawPanelTitle (juce::Graphics& g, const juce::String& title, juce::Rectangle<int> bar, juce::Colour barColour)
    {
        const auto textColour = juce::Colours::white;
        const auto r = bar.toFloat();
        constexpr float corner = 5.0f;
        juce::Path strip;
        strip.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), corner, corner, true, true, false, false);
        g.setColour (barColour);
        g.fillPath (strip);
        // A dark line where the bar meets the plate.
        g.setColour (colours::ink.withAlpha (0.8f));
        g.drawLine (r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, 1.0f);

        g.setColour (textColour);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)).withExtraKerningFactor (0.12f));
        g.drawText (title, r.translated (0.0f, 0.5f), juce::Justification::centred);
    }

    //==========================================================================
    // Analog knob: printed tick scale, black fluted skirt with a white pointer line,
    // spun-aluminium centre cap.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float outerR = knobRadius (bounds, slider);
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool enabled = slider.isEnabled();
        auto circle = [] (float r, juce::Point<float> c) { return juce::Rectangle<float> (2 * r, 2 * r).withCentre (c); };

        if (! enabled)
            g.beginTransparencyLayer (0.4f);

        // Scale: alternating long and short ticks.
        constexpr int ticks = 21;
        for (int i = 0; i < ticks; ++i)
        {
            const bool major = i % 2 == 0;
            const float a = startAngle + static_cast<float> (i) / (ticks - 1) * (endAngle - startAngle);
            g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId).withAlpha (major ? 0.9f : 0.7f));
            g.drawLine ({ centre.getPointOnCircumference (outerR, a),
                          centre.getPointOnCircumference (outerR - (major ? 6.0f : 3.5f), a) }, major ? 1.4f : 1.1f);
        }

        const float skirtR = outerR - 9.0f;

        // Fluted outline that turns with the knob.
        constexpr int flutes = 8;
        juce::Path skirt;
        constexpr int steps = 180;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * static_cast<float> (i) / steps;
            const float r = skirtR * (0.955f + 0.045f * std::cos (static_cast<float> (flutes) * (t - angle)));
            const auto p = centre.getPointOnCircumference (r, t);
            if (i == 0) skirt.startNewSubPath (p); else skirt.lineTo (p);
        }
        skirt.closeSubPath();

        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillPath (skirt, juce::AffineTransform::scale (1.08f, 1.08f, centre.x, centre.y).translated (0.0f, 3.0f));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (skirt, juce::AffineTransform::translation (0.0f, 1.8f));

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e2e), 0.0f, centre.y - skirtR,
                                                 juce::Colour (0xff050505), 0.0f, centre.y + skirtR, false));
        g.fillPath (skirt);

        const float capR = skirtR * 0.56f;
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0.0f, centre.y - skirtR,
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, centre.y, false));
        g.strokePath (skirt, juce::PathStrokeType (1.0f));

        // Pointer line on the skirt.
        g.setColour (colours::cream);
        g.drawLine ({ centre.getPointOnCircumference (capR + 2.0f, angle), centre.getPointOnCircumference (skirtR * 0.97f, angle) }, 2.4f);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (circle (capR + 1.0f, centre.translated (0.0f, 0.8f)));
        const auto capRect = circle (capR, centre);
        if (slider.getProperties()["plainCap"])
        {
            // All-black knob: a softly lit black cap instead of the aluminium one.
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff383838), 0.0f, capRect.getY(),
                                                     juce::Colour (0xff0d0d0d), 0.0f, capRect.getBottom(), false));
            g.fillEllipse (capRect);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), 0.0f, capRect.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), 0.0f, capRect.getCentreY(), false));
            g.drawEllipse (capRect.reduced (0.6f), 1.0f);
        }
        else
        {
            // Spun-aluminium cap: the sheen depends on the direction around the centre (fixed light).
            constexpr int wedges = 72;
            for (int i = 0; i < wedges; ++i)
            {
                const float a0 = juce::MathConstants<float>::twoPi * static_cast<float> (i) / wedges;
                const float a1 = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / wedges + 0.01f;
                const float mid = 0.5f * (a0 + a1);
                const float v = 0.58f + 0.24f * std::cos (2.0f * (mid + 0.6f)) + 0.06f * std::cos (6.0f * mid);
                juce::Path wedge;
                wedge.addPieSegment (capRect, a0, a1, 0.0f);
                g.setColour (juce::Colour::fromFloatRGBA (v * 0.98f, v * 0.99f, v, 1.0f));
                g.fillPath (wedge);
            }
        }
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (capRect, 0.8f);

        if (! enabled)
            g.endTransparencyLayer();
    }

    // Radius of a knob's tick scale in its drawing area. The main knobs are drawn a touch
    // smaller than their cell; the plain-cap OUT knobs already sit in smaller cells.
    static float knobRadius (juce::Rectangle<float> area, juce::Slider& slider)
    {
        const float sizeScale = slider.getProperties()["plainCap"] ? 1.0f : 0.92f;
        return (juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f) * sizeScale;
    }

    // Pull the value text up to just below the knob's lowest scale ticks.
    juce::Slider::SliderLayout getSliderLayout (juce::Slider& slider) override
    {
        auto layout = LookAndFeel_V4::getSliderLayout (slider);
        if (slider.isRotary() && slider.getTextBoxPosition() == juce::Slider::TextBoxBelow)
        {
            const auto area = layout.sliderBounds.toFloat();
            const float lowestTick = area.getCentreY() + knobRadius (area, slider) * 0.81f;
            layout.textBoxBounds.setY (juce::roundToInt (lowestTick) + 3);
        }
        return layout;
    }

    //==========================================================================
    // Selectors marked with the "icons" property draw a square button showing the
    // waveform of the current choice, with its name underneath.
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
        r.removeFromBottom (kIconTextH);
        const float side = juce::jmin (r.getWidth() - 8.0f, r.getHeight() - 12.0f, 56.0f);
        const auto square = r.withSizeKeepingCentre (side, side);
        const auto textArea = juce::Rectangle<float> (r.getX(), square.getBottom() + 7.0f, r.getWidth(), kIconTextH);

        drawChromeBezel (g, square, 6.0f);
        const auto well = square.reduced (3.5f);
        drawWell (g, well, 4.0f);

        const auto icon = waveIconPath (box.getText(), well.reduced (well.getWidth() * 0.18f, well.getHeight() * 0.26f));
        g.setColour (colours::ledGreen.withAlpha (0.25f));
        g.strokePath (icon, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::cream);
        g.strokePath (icon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path arrow;
        const auto a = juce::Rectangle<float> (6.0f, 4.0f).withCentre (well.getBottomRight().translated (-6.0f, -5.0f));
        arrow.addTriangle (a.getTopLeft(), a.getTopRight(), a.getCentre().withY (a.getBottom()));
        g.setColour (colours::red);
        g.fillPath (arrow);

        g.setColour (colours::ink);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (box.getText(), textArea, juce::Justification::centred, false);
    }

    void positionComboBoxText (juce::ComboBox&, juce::Label& label) override
    {
        label.setBounds ({});
    }

    // Popup entries with their waveform icon.
    static void addIconItems (juce::ComboBox& box, const juce::StringArray& names)
    {
        auto* menu = box.getRootMenu();
        menu->clear();
        for (int i = 0; i < names.size(); ++i)
        {
            auto drawable = std::make_unique<juce::DrawablePath>();
            drawable->setPath (waveIconPath (names[i], { 0.0f, 3.0f, 24.0f, 14.0f }));
            drawable->setFill (juce::FillType (juce::Colours::transparentBlack));
            drawable->setStrokeFill (juce::FillType (colours::cream));
            drawable->setStrokeType (juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::PopupMenu::Item item (names[i]);
            item.itemID = i + 1;
            item.image = std::move (drawable);
            menu->addItem (std::move (item));
        }
    }

    static constexpr float kIconTextH = 16.0f;

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        return label;
    }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }

private:
    juce::Image brushed = makeBrushedMetal (560, 400, 1234);
};
} // namespace ui
