/*
 * ============================================================================
 * ARC System Audio Host - Look And Feel Header
 * Modern dark theme for the application
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"

namespace ARC
{

//=============================================================================
/**
 * Color palette for the application
 */
namespace Colors
{
    // Background colors
    const juce::Colour background          = juce::Colour(0xFF0D0D14);
    const juce::Colour backgroundLight     = juce::Colour(0xFF1A1A24);
    const juce::Colour backgroundPanel     = juce::Colour(0xFF14141E);
    const juce::Colour backgroundHover     = juce::Colour(0xFF222230);
    const juce::Colour backgroundSelected  = juce::Colour(0xFF2A2A3C);

    // Accent colors
    const juce::Colour accent              = juce::Colour(0xFF00D4AA);
    const juce::Colour accentBright        = juce::Colour(0xFF00FFD0);
    const juce::Colour accentDark          = juce::Colour(0xFF00A885);
    const juce::Colour accentSecondary     = juce::Colour(0xFF6366F1);

    // Status colors
    const juce::Colour active              = juce::Colour(0xFF00D4AA);
    const juce::Colour bypassed            = juce::Colour(0xFF505060);
    const juce::Colour error               = juce::Colour(0xFFFF4757);
    const juce::Colour warning             = juce::Colour(0xFFFFB800);
    const juce::Colour highCPU             = juce::Colour(0xFFFF6B6B);

    // Text colors
    const juce::Colour textPrimary         = juce::Colour(0xFFFFFFFF);
    const juce::Colour textSecondary       = juce::Colour(0xFFB0B0C0);
    const juce::Colour textDim             = juce::Colour(0xFF707080);
    const juce::Colour textDisabled        = juce::Colour(0xFF404050);

    // Border colors
    const juce::Colour border              = juce::Colour(0xFF303040);
    const juce::Colour borderLight         = juce::Colour(0xFF404050);
    const juce::Colour borderFocus         = accent;

    // Meter colors
    const juce::Colour meterLow            = juce::Colour(0xFF00D4AA);
    const juce::Colour meterMid            = juce::Colour(0xFFFFB800);
    const juce::Colour meterHigh           = juce::Colour(0xFFFF4757);
    const juce::Colour meterBackground     = juce::Colour(0xFF1A1A24);
}

//=============================================================================
/**
 * Custom Look and Feel for ARC application
 */
class ARCLookAndFeel : public juce::LookAndFeel_V4
{
public:
    //=========================================================================
    ARCLookAndFeel();
    ~ARCLookAndFeel() override;

    //=========================================================================
    // Colors
    void setAccentColour(const juce::Colour& colour);
    juce::Colour getAccentColour() const { return accentColour; }

    //=========================================================================
    // Buttons
    void drawButtonBackground(juce::Graphics& g,
                             juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                       juce::TextButton& button,
                       bool shouldDrawButtonAsHighlighted,
                       bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics& g,
                         juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    //=========================================================================
    // Sliders
    void drawLinearSlider(juce::Graphics& g,
                         int x, int y, int width, int height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         const juce::Slider::SliderStyle style,
                         juce::Slider& slider) override;

    void drawRotarySlider(juce::Graphics& g,
                         int x, int y, int width, int height,
                         float sliderPosProportional,
                         float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override;

    //=========================================================================
    // Combo boxes
    void drawComboBox(juce::Graphics& g,
                     int width, int height,
                     bool isButtonDown,
                     int buttonX, int buttonY, int buttonW, int buttonH,
                     juce::ComboBox& box) override;

    void drawPopupMenuItem(juce::Graphics& g,
                          const juce::Rectangle<int>& area,
                          bool isSeparator, bool isActive, bool isHighlighted,
                          bool isTicked, bool hasSubMenu,
                          const juce::String& text,
                          const juce::String& shortcutKeyText,
                          const juce::Drawable* icon,
                          const juce::Colour* textColour) override;

    //=========================================================================
    // Text editor
    void fillTextEditorBackground(juce::Graphics& g,
                                  int width, int height,
                                  juce::TextEditor& textEditor) override;

    void drawTextEditorOutline(juce::Graphics& g,
                              int width, int height,
                              juce::TextEditor& textEditor) override;

    //=========================================================================
    // Labels
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    //=========================================================================
    // Scroll bars
    void drawScrollbar(juce::Graphics& g,
                      juce::ScrollBar& scrollbar,
                      int x, int y, int width, int height,
                      bool isScrollbarVertical,
                      int thumbStartPosition, int thumbSize,
                      bool isMouseOver, bool isMouseDown) override;

    //=========================================================================
    // Progress bar
    void drawProgressBar(juce::Graphics& g,
                        juce::ProgressBar& progressBar,
                        int width, int height,
                        double progress,
                        const juce::String& textToShow) override;

    //=========================================================================
    // Table headers
    void drawTableHeaderBackground(juce::Graphics& g,
                                   juce::TableHeaderComponent& header) override;

    void drawTableHeaderColumn(juce::Graphics& g,
                              juce::TableHeaderComponent& header,
                              const juce::String& columnName, int columnId,
                              int width, int height,
                              bool isMouseOver, bool isMouseDown,
                              int columnFlags) override;

    //=========================================================================
    // Fonts
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getLabelFont(juce::Label& label) override;

    //=========================================================================
    // Custom drawing helpers
    static void drawGlowEffect(juce::Graphics& g,
                               const juce::Rectangle<float>& bounds,
                               const juce::Colour& colour,
                               float radius = 10.0f);

    static void drawRoundedPanel(juce::Graphics& g,
                                 const juce::Rectangle<float>& bounds,
                                 const juce::Colour& fillColour,
                                 float cornerRadius = 8.0f,
                                 bool drawBorder = true);

private:
    //=========================================================================
    juce::Colour accentColour;
    juce::Font mainFont;
    juce::Font boldFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ARCLookAndFeel)
};

} // namespace ARC

