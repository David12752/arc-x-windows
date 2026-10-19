/*
 * ============================================================================
 * ARC System Audio Host - Look And Feel Implementation
 * ============================================================================
 */

#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
ARCLookAndFeel::ARCLookAndFeel()
    : accentColour(Colors::accent)
{
    // Set default colours
    setColour(juce::ResizableWindow::backgroundColourId, Colors::background);
    setColour(juce::DocumentWindow::textColourId, Colors::textPrimary);

    setColour(juce::TextButton::buttonColourId, Colors::backgroundLight);
    setColour(juce::TextButton::buttonOnColourId, Colors::accent);
    setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
    setColour(juce::TextButton::textColourOnId, Colors::background);

    setColour(juce::ComboBox::backgroundColourId, Colors::backgroundLight);
    setColour(juce::ComboBox::textColourId, Colors::textPrimary);
    setColour(juce::ComboBox::outlineColourId, Colors::border);
    setColour(juce::ComboBox::arrowColourId, Colors::textSecondary);

    setColour(juce::PopupMenu::backgroundColourId, Colors::backgroundPanel);
    setColour(juce::PopupMenu::textColourId, Colors::textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Colors::accent);
    setColour(juce::PopupMenu::highlightedTextColourId, Colors::background);

    setColour(juce::TextEditor::backgroundColourId, Colors::backgroundLight);
    setColour(juce::TextEditor::textColourId, Colors::textPrimary);
    setColour(juce::TextEditor::highlightColourId, Colors::accent.withAlpha(0.3f));
    setColour(juce::TextEditor::outlineColourId, Colors::border);
    setColour(juce::TextEditor::focusedOutlineColourId, Colors::accent);

    setColour(juce::Label::textColourId, Colors::textPrimary);

    setColour(juce::Slider::backgroundColourId, Colors::backgroundLight);
    setColour(juce::Slider::trackColourId, Colors::accent);
    setColour(juce::Slider::thumbColourId, Colors::textPrimary);
    setColour(juce::Slider::textBoxTextColourId, Colors::textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, Colors::backgroundLight);
    setColour(juce::Slider::textBoxOutlineColourId, Colors::border);

    setColour(juce::ScrollBar::thumbColourId, Colors::backgroundHover);
    setColour(juce::ScrollBar::trackColourId, Colors::backgroundPanel);

    setColour(juce::ListBox::backgroundColourId, Colors::backgroundPanel);
    setColour(juce::ListBox::textColourId, Colors::textPrimary);

    setColour(juce::TableHeaderComponent::backgroundColourId, Colors::backgroundLight);
    setColour(juce::TableHeaderComponent::textColourId, Colors::textSecondary);

    // Initialize fonts
    mainFont = juce::Font("Segoe UI", 14.0f, juce::Font::plain);
    boldFont = juce::Font("Segoe UI", 14.0f, juce::Font::bold);

    setDefaultSansSerifTypeface(mainFont.getTypefacePtr());
}

ARCLookAndFeel::~ARCLookAndFeel() = default;

//=============================================================================
void ARCLookAndFeel::setAccentColour(const juce::Colour& colour)
{
    accentColour = colour;

    setColour(juce::TextButton::buttonOnColourId, colour);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colour);
    setColour(juce::Slider::trackColourId, colour);
    setColour(juce::TextEditor::focusedOutlineColourId, colour);
}

//=============================================================================
void ARCLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                          juce::Button& button,
                                          const juce::Colour& backgroundColour,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto cornerSize = 6.0f;

    juce::Colour baseColour = backgroundColour;

    if (shouldDrawButtonAsDown)
        baseColour = baseColour.brighter(0.1f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.05f);

    // Draw shadow
    if (!shouldDrawButtonAsDown)
    {
        auto shadowBounds = bounds.translated(0, 1);
        g.setColour(juce::Colours::black.withAlpha(0.2f));
        g.fillRoundedRectangle(shadowBounds, cornerSize);
    }

    // Draw button background
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Draw border
    g.setColour(Colors::border);
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

    // Draw glow for toggle buttons that are on
    if (button.getToggleState())
    {
        g.setColour(accentColour.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.reduced(1), cornerSize, 2.0f);
    }
}

void ARCLookAndFeel::drawButtonText(juce::Graphics& g,
                                    juce::TextButton& button,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    auto textColour = button.getToggleState()
        ? Colors::background
        : (button.isEnabled() ? Colors::textPrimary : Colors::textDisabled);

    g.setColour(textColour);

    auto textBounds = button.getLocalBounds();
    g.drawText(button.getButtonText(), textBounds, juce::Justification::centred);
}

void ARCLookAndFeel::drawToggleButton(juce::Graphics& g,
                                      juce::ToggleButton& button,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    auto toggleSize = 18.0f;
    auto toggleBounds = bounds.removeFromLeft(toggleSize).reduced(2);

    // Draw toggle background
    g.setColour(Colors::backgroundLight);
    g.fillRoundedRectangle(toggleBounds, 3.0f);
    g.setColour(Colors::border);
    g.drawRoundedRectangle(toggleBounds, 3.0f, 1.0f);

    // Draw check mark if toggled
    if (button.getToggleState())
    {
        g.setColour(accentColour);
        g.fillRoundedRectangle(toggleBounds.reduced(3), 2.0f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        g.setColour(Colors::backgroundHover);
        g.fillRoundedRectangle(toggleBounds.reduced(3), 2.0f);
    }

    // Draw label
    g.setColour(button.isEnabled() ? Colors::textPrimary : Colors::textDisabled);
    g.setFont(mainFont);
    g.drawText(button.getButtonText(),
               bounds.withTrimmedLeft(8),
               juce::Justification::centredLeft);
}

//=============================================================================
void ARCLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                      int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      const juce::Slider::SliderStyle style,
                                      juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, style);

    auto bounds = juce::Rectangle<float>(x, y, width, height);
    auto trackHeight = 4.0f;
    auto thumbSize = 12.0f;

    bool isHorizontal = slider.isHorizontal();

    // Draw track background
    juce::Rectangle<float> track;
    if (isHorizontal)
    {
        track = bounds.withSizeKeepingCentre(bounds.getWidth(), trackHeight);
    }
    else
    {
        track = bounds.withSizeKeepingCentre(trackHeight, bounds.getHeight());
    }

    g.setColour(Colors::backgroundLight);
    g.fillRoundedRectangle(track, trackHeight / 2);

    // Draw filled portion
    juce::Rectangle<float> filled;
    if (isHorizontal)
    {
        filled = track.withWidth(sliderPos - x);
    }
    else
    {
        filled = track.withTop(sliderPos).withHeight(track.getBottom() - sliderPos);
    }

    g.setColour(accentColour);
    g.fillRoundedRectangle(filled, trackHeight / 2);

    // Draw thumb
    juce::Point<float> thumbPos;
    if (isHorizontal)
    {
        thumbPos = juce::Point<float>(sliderPos, track.getCentreY());
    }
    else
    {
        thumbPos = juce::Point<float>(track.getCentreX(), sliderPos);
    }

    g.setColour(Colors::textPrimary);
    g.fillEllipse(thumbPos.x - thumbSize / 2,
                  thumbPos.y - thumbSize / 2,
                  thumbSize, thumbSize);
}

void ARCLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                      int x, int y, int width, int height,
                                      float sliderPosProportional,
                                      float rotaryStartAngle, float rotaryEndAngle,
                                      juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto arcRadius = radius - 6.0f;
    auto arcWidth = 4.0f;

    // Draw background arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);

    g.setColour(Colors::backgroundLight);
    g.strokePath(backgroundArc, juce::PathStrokeType(arcWidth,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Draw value arc
    auto angle = rotaryStartAngle +
        sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                           0.0f, rotaryStartAngle, angle, true);

    g.setColour(accentColour);
    g.strokePath(valueArc, juce::PathStrokeType(arcWidth,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Draw center circle
    auto innerRadius = arcRadius - 12.0f;
    g.setColour(Colors::backgroundPanel);
    g.fillEllipse(centreX - innerRadius, centreY - innerRadius,
                  innerRadius * 2, innerRadius * 2);

    // Draw pointer
    juce::Path pointer;
    auto pointerLength = innerRadius * 0.6f;
    auto pointerThickness = 3.0f;
    pointer.addRoundedRectangle(-pointerThickness / 2, -pointerLength,
                                 pointerThickness, pointerLength, 1.0f);

    g.setColour(Colors::textPrimary);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
}

//=============================================================================
void ARCLookAndFeel::drawComboBox(juce::Graphics& g,
                                  int width, int height,
                                  bool isButtonDown,
                                  int buttonX, int buttonY, int buttonW, int buttonH,
                                  juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);

    auto bounds = juce::Rectangle<float>(0, 0, width, height).reduced(1);
    auto cornerSize = 6.0f;

    // Background
    g.setColour(isButtonDown ? Colors::backgroundHover : Colors::backgroundLight);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Border
    g.setColour(box.hasKeyboardFocus(true) ? accentColour : Colors::border);
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

    // Arrow
    auto arrowZone = juce::Rectangle<float>(width - 20.0f, 0.0f, 15.0f, height);
    juce::Path arrow;
    arrow.addTriangle(arrowZone.getCentreX() - 4, arrowZone.getCentreY() - 2,
                      arrowZone.getCentreX() + 4, arrowZone.getCentreY() - 2,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 3);

    g.setColour(Colors::textSecondary);
    g.fillPath(arrow);
}

void ARCLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                       const juce::Rectangle<int>& area,
                                       bool isSeparator, bool isActive, bool isHighlighted,
                                       bool isTicked, bool hasSubMenu,
                                       const juce::String& text,
                                       const juce::String& shortcutKeyText,
                                       const juce::Drawable* icon,
                                       const juce::Colour* textColour)
{
    juce::ignoreUnused(icon, textColour);

    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        r.removeFromTop(r.getHeight() / 2 - 1);

        g.setColour(Colors::border);
        g.fillRect(r.removeFromTop(1));
        return;
    }

    auto r = area.reduced(2);

    if (isHighlighted && isActive)
    {
        g.setColour(accentColour);
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.setColour(Colors::background);
    }
    else
    {
        g.setColour(isActive ? Colors::textPrimary : Colors::textDim);
    }

    auto font = mainFont;
    g.setFont(font);

    auto textArea = r.reduced(10, 0);

    if (isTicked)
    {
        auto tickArea = textArea.removeFromLeft(20);
        g.drawText(juce::String::charToString(0x2713), tickArea,
                   juce::Justification::centred);
    }

    g.drawText(text, textArea, juce::Justification::centredLeft);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(Colors::textDim);
        g.drawText(shortcutKeyText, textArea, juce::Justification::centredRight);
    }

    if (hasSubMenu)
    {
        auto arrowArea = r.removeFromRight(15);
        g.drawText(">", arrowArea, juce::Justification::centred);
    }
}

//=============================================================================
void ARCLookAndFeel::fillTextEditorBackground(juce::Graphics& g,
                                              int width, int height,
                                              juce::TextEditor& textEditor)
{
    juce::ignoreUnused(textEditor);

    g.setColour(Colors::backgroundLight);
    g.fillRoundedRectangle(0.0f, 0.0f, width, height, 6.0f);
}

void ARCLookAndFeel::drawTextEditorOutline(juce::Graphics& g,
                                           int width, int height,
                                           juce::TextEditor& textEditor)
{
    auto colour = textEditor.hasKeyboardFocus(true) ? accentColour : Colors::border;
    g.setColour(colour);
    g.drawRoundedRectangle(0.5f, 0.5f, width - 1.0f, height - 1.0f, 6.0f, 1.0f);
}

//=============================================================================
void ARCLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));

    auto textArea = label.getLocalBounds().reduced(2);
    g.drawText(label.getText(), textArea, label.getJustificationType());
}

//=============================================================================
void ARCLookAndFeel::drawScrollbar(juce::Graphics& g,
                                   juce::ScrollBar& scrollbar,
                                   int x, int y, int width, int height,
                                   bool isScrollbarVertical,
                                   int thumbStartPosition, int thumbSize,
                                   bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused(scrollbar);

    g.setColour(Colors::backgroundPanel);
    g.fillRect(x, y, width, height);

    juce::Rectangle<int> thumbBounds;

    if (isScrollbarVertical)
    {
        thumbBounds = juce::Rectangle<int>(x + 2, thumbStartPosition,
                                           width - 4, thumbSize);
    }
    else
    {
        thumbBounds = juce::Rectangle<int>(thumbStartPosition, y + 2,
                                           thumbSize, height - 4);
    }

    auto colour = isMouseDown ? Colors::accent
                              : (isMouseOver ? Colors::backgroundSelected
                                             : Colors::backgroundHover);

    g.setColour(colour);
    g.fillRoundedRectangle(thumbBounds.toFloat(), 4.0f);
}

//=============================================================================
void ARCLookAndFeel::drawProgressBar(juce::Graphics& g,
                                     juce::ProgressBar& progressBar,
                                     int width, int height,
                                     double progress,
                                     const juce::String& textToShow)
{
    auto bounds = juce::Rectangle<float>(0, 0, width, height).reduced(2);

    // Background
    g.setColour(Colors::backgroundLight);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Progress
    if (progress >= 0)
    {
        auto fillWidth = bounds.getWidth() * static_cast<float>(progress);
        g.setColour(accentColour);
        g.fillRoundedRectangle(bounds.withWidth(fillWidth), 4.0f);
    }
    else
    {
        // Indeterminate progress - animated stripe
        auto time = juce::Time::getMillisecondCounter() / 1000.0f;
        auto offset = std::fmod(time * 100.0f, 30.0f);

        for (float x = -30 + offset; x < static_cast<float>(bounds.getWidth()); x += 30)
        {
            juce::Path stripe;
            float bx = static_cast<float>(bounds.getX()) + x;
            float by = static_cast<float>(bounds.getY());
            float bh = static_cast<float>(bounds.getHeight());
            stripe.startNewSubPath(bx, by);
            stripe.lineTo(bx + 15, by);
            stripe.lineTo(bx + 25, by + bh);
            stripe.lineTo(bx + 10, by + bh);
            stripe.closeSubPath();
            g.setColour(accentColour.withAlpha(0.3f));
            g.fillPath(stripe);
        }
    }

    // Text
    if (textToShow.isNotEmpty())
    {
        g.setColour(Colors::textPrimary);
        g.setFont(mainFont);
        g.drawText(textToShow, bounds, juce::Justification::centred);
    }

    juce::ignoreUnused(progressBar);
}

//=============================================================================
void ARCLookAndFeel::drawTableHeaderBackground(juce::Graphics& g,
                                               juce::TableHeaderComponent& header)
{
    juce::ignoreUnused(header);

    g.setColour(Colors::backgroundLight);
    g.fillRect(g.getClipBounds());

    g.setColour(Colors::border);
    g.drawLine(0, g.getClipBounds().getBottom() - 0.5f,
               g.getClipBounds().getWidth(), g.getClipBounds().getBottom() - 0.5f);
}

void ARCLookAndFeel::drawTableHeaderColumn(juce::Graphics& g,
                                           juce::TableHeaderComponent& header,
                                           const juce::String& columnName, int columnId,
                                           int width, int height,
                                           bool isMouseOver, bool isMouseDown,
                                           int columnFlags)
{
    juce::ignoreUnused(header, columnId, columnFlags);

    if (isMouseDown)
        g.setColour(Colors::backgroundHover);
    else if (isMouseOver)
        g.setColour(Colors::backgroundLight.brighter(0.02f));

    if (isMouseDown || isMouseOver)
        g.fillRect(0, 0, width, height);

    g.setColour(Colors::textSecondary);
    g.setFont(boldFont);
    g.drawText(columnName, 8, 0, width - 16, height, juce::Justification::centredLeft);

    g.setColour(Colors::border);
    g.drawLine(width - 0.5f, 4, width - 0.5f, height - 4);
}

//=============================================================================
juce::Font ARCLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    juce::ignoreUnused(button, buttonHeight);
    return mainFont;
}

juce::Font ARCLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    return mainFont;
}

juce::Font ARCLookAndFeel::getLabelFont(juce::Label& label)
{
    juce::ignoreUnused(label);
    return mainFont;
}

//=============================================================================
void ARCLookAndFeel::drawGlowEffect(juce::Graphics& g,
                                    const juce::Rectangle<float>& bounds,
                                    const juce::Colour& colour,
                                    float radius)
{
    for (float i = radius; i > 0; i -= 2.0f)
    {
        g.setColour(colour.withAlpha((radius - i) / radius * 0.1f));
        g.drawRoundedRectangle(bounds.expanded(i), 8.0f, 2.0f);
    }
}

void ARCLookAndFeel::drawRoundedPanel(juce::Graphics& g,
                                      const juce::Rectangle<float>& bounds,
                                      const juce::Colour& fillColour,
                                      float cornerRadius,
                                      bool drawBorder)
{
    g.setColour(fillColour);
    g.fillRoundedRectangle(bounds, cornerRadius);

    if (drawBorder)
    {
        g.setColour(Colors::border);
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
    }
}

} // namespace ARC

