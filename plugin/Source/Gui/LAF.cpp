/*
  ==============================================================================

    LAF.cpp
    Created: 25 May 2025 2:42:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "LAF.h"

namespace nvs::gui
{
LAF::LAF()
{
	auto scheme = getDarkColourScheme();
	scheme.setUIColour(ColourScheme::UIColour::defaultFill, notchColour);
	setColourScheme(scheme);
	setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour(juce::Colours::lightgrey).withAlpha(0.4f));
	
	setColour (ComboBox::backgroundColourId,   Colour(juce::Colours::transparentBlack).withAlpha(0.5f));
	setColour (ComboBox::outlineColourId,      juce::Colours::grey.withAlpha (0.3f));
	setColour (ComboBox::ColourIds::focusedOutlineColourId, juce::Colours::grey.withAlpha (0.6f));
	setColour (ComboBox::arrowColourId,        juce::Colours::lightgrey);
	
	setColour (PopupMenu::textColourId,        juce::Colours::snow.withBrightness(0.6f));
	setColour (PopupMenu::highlightedTextColourId, juce::Colours::snow);
	setColour (PopupMenu::backgroundColourId,      juce::Colours::darkgrey.darker());
	setColour (PopupMenu::highlightedBackgroundColourId, juce::Colours::darkgrey);
	
	setColour (TextButton::buttonColourId,          Colour(juce::Colours::transparentBlack).withAlpha(0.5f));
	setColour (TextButton::buttonOnColourId,        Colour(juce::Colours::transparentBlack).withAlpha(0.5f));
	
	setColour (TooltipWindow::backgroundColourId, juce::Colours::black.withAlpha (0.8f));
	setColour (TooltipWindow::textColourId,       juce::Colours::white.withAlpha (0.9f));
	setColour (TooltipWindow::outlineColourId,    juce::Colours::grey.withAlpha (0.5f));
}
void LAF::drawTabButton (TabBarButton& button,
					Graphics& g,
					const bool isMouseOver,
					const bool isMouseDown)
{
	Path outline;
	createTabButtonShape (button, outline, isMouseOver, isMouseDown);
	fillTabButtonShape   (button, g, outline, isMouseOver, isMouseDown);

	drawTabButtonText (button, g, isMouseOver, isMouseDown);
}

void LAF::drawTabButtonText (TabBarButton& button,
						Graphics& g,
						bool /*isMouseOver*/,
						bool /*isMouseDown*/)
{
	static constexpr auto fontHeight = 14.0f;
	const auto fontStyle  = button.isFrontTab() ? Font::bold : Font::plain;

	g.setFont (FontOptions(fontName, fontHeight, fontStyle));
	g.setColour(Colours::white);
	g.drawFittedText (button.getButtonText(),
					  button.getLocalBounds(),
					  Justification::centred,
					  1);
}
void LAF::drawComboBox (juce::Graphics& g,
    const int width, const int height,
    const bool isButtonDown,
	const int buttonX, const int buttonY, const int buttonW, const int buttonH,
	ComboBox& cb)
{
	const auto bounds = Rectangle(0.f, 0.f, static_cast<float>(width), static_cast<float>(height)).reduced (1.0f);
	
	g.setColour (findColour (ComboBox::backgroundColourId));
	g.fillRect (bounds);
	
	g.setColour (findColour (ComboBox::outlineColourId));
	g.drawRect (bounds, 1.0f);
	
	// Draw the arrow:
	Path arrow;
	arrow.addTriangle ({ 0.0f, 0.0f },
					   { 1.0f, 0.0f },
					   { 0.5f, 0.5f });

    const auto buttonHf = static_cast<float> (buttonH);
    const auto buttonWf = static_cast<float> (buttonW);
    const auto buttonXf = static_cast<float> (buttonX);
    const auto buttonYf = static_cast<float> (buttonY);

    const auto arrowH = buttonHf * 0.3f;
	const auto arrowW = arrowH * 1.3f;
	
	g.setColour (findColour (ComboBox::arrowColourId));
	g.fillPath (arrow,
				arrow.getTransformToScaleToFit (buttonXf + (buttonWf - arrowW) * 0.5f,
												buttonYf + (buttonHf - arrowH) * 0.5f,
												arrowW, arrowH,
												true));
}
Font LAF::getTextButtonFont(TextButton&, int buttonHeight) {
	return {FontOptions(fontName, 13.0, Font::plain)};
}
Font LAF::getComboBoxFont (ComboBox&) {
	return getPopupMenuFont();// use same font as its popup menu
}
Font LAF::getPopupMenuFont() {
	return {FontOptions(fontName, 13.0, Font::plain)};
}

void LAF::drawTooltip (Graphics &g, const String &text, const int width, const int height)
{
    const auto wf = static_cast<float>(width);
    const auto hf = static_cast<float>(height);

	// background
	g.setColour (findColour (TooltipWindow::backgroundColourId));
	g.fillRoundedRectangle (0.0f, 0.0f, wf, hf, 4.0f);

	// border
	g.setColour (findColour (TooltipWindow::outlineColourId));
	g.drawRoundedRectangle (0.0f, 0.0f, wf, hf, 4.0f, 1.0f);

	// text
	g.setColour (findColour (TooltipWindow::textColourId));
	g.setFont (Font(FontOptions(fontName, 13.0, Font::plain)));
	g.drawFittedText(text, g.getClipBounds(), juce::Justification::centred, 4);
}

void LAF::drawButtonBackground (Graphics& g, Button& b,
											const Colour& backgroundColour,
											bool isMouseOverButton, bool isButtonDown)
{
	const auto area = b.getLocalBounds().toFloat().reduced (1.0f);
	g.setColour (backgroundColour);
	g.fillRect (area);
	
	// subtle border
	g.setColour (findColour (TextButton::textColourOffId).withAlpha (0.3f));
	g.drawRect (area, 1.0f);
}
void LAF::drawButtonText (Graphics& g, TextButton& b,
									  bool /*isMouseOverButton*/, bool /*isButtonDown*/)
{
	g.setFont (getTextButtonFont(b, b.getHeight()));
	g.setColour (b.getToggleState()
				 ? findColour (TextButton::textColourOnId)
				 : findColour (TextButton::textColourOffId));
	
	g.drawFittedText (b.getButtonText(),
					  b.getLocalBounds().reduced (4, 2),
					  Justification::centred, 1);
}

void LAF::drawPopupMenuBackground (Graphics& g, const int width, const int height) {
	g.fillAll (findColour(PopupMenu::ColourIds::backgroundColourId));
	g.setColour (findColour(PopupMenu::ColourIds::highlightedBackgroundColourId));
	g.drawRect (0, 0, width, height, 1);
}
void LAF::drawPopupMenuItem (Graphics& g,
										 const Rectangle<int>& area,
										 bool isSeparator, bool isActive,
										 bool isHighlighted, bool isTicked,
										 bool hasSubMenu, const String& text,
										 const String& shortcutKeyText,
										 const Drawable* icon, Colour const*textColour)
{
	if (isSeparator)
	{
		LookAndFeel_V4::drawPopupMenuItem (g, area, true,
										   isActive, isHighlighted,
										   isTicked, hasSubMenu,
										   text, shortcutKeyText,
										   icon, textColour);
	}
	else
	{
		// subtle highlight
		if (isHighlighted)
			g.fillAll (Colours::grey.withAlpha (0.7f));
		
		if (textColour != nullptr){
			g.setColour (*textColour);
		}
		else {
			if (isHighlighted){
				g.setColour (findColour(PopupMenu::ColourIds::highlightedTextColourId));
			}
			else {
				g.setColour (findColour(PopupMenu::ColourIds::textColourId));
			}
		}
		g.setFont (getPopupMenuFont());
		g.drawText (text, area.reduced (4, 2),
					Justification::centredLeft);
	}
}
void LAF::drawLabel (Graphics& g, Label& label)
{
	if (const auto* slider = dynamic_cast<Slider*>(label.getParentComponent()))
	{
		if (slider->getSliderStyle() == Slider::LinearBarVertical)
		{
			return;
		}
	}
	LookAndFeel_V4::drawLabel(g, label);
}
void LAF::drawLinearSlider (Graphics& g,
										const int x, const int y, const int width, const int height,
										const float sliderPos,
										float minSliderPos, float maxSliderPos,
										const Slider::SliderStyle style,
										Slider& s)
{
	if (style == Slider::LinearBarVertical)
	{
		// Background track
		g.setColour(s.findColour(Slider::backgroundColourId));
		g.fillRect(x, y, width, height);
		
		// Filled portion (from bottom to slider position)
		const auto fillHeight = static_cast<float>(height) - (sliderPos - static_cast<float>(y));
		g.setColour(s.findColour(Slider::trackColourId));
		g.fillRect(static_cast<float>(x), sliderPos, static_cast<float>(width), fillHeight);

		const Rectangle tb (x, y + (height/2 - 10), width, 20);
		const bool over = (static_cast<float>(tb.getCentreY()) >= sliderPos);
		g.setColour (over ? juce::Colour(Colours::grey).withMultipliedBrightness(1.25f)
						  : juce::Colour(Colours::grey).withMultipliedBrightness(1.5f));
		g.setFont(Font (FontOptions(fontName, 13.5f, Font::plain)));
		g.drawFittedText (s.getTextFromValue (s.getValue()),
						 tb, Justification::centred, 1);
	}
	else
	{
		LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
										  sliderPos, 0.0f, 0.0f,
										  style, s);
	}
}
void LAF::drawPieSegment(juce::Graphics &g,
    const juce::Rectangle<float> ellipseRect,
    const float angle, const float notchWidth, const float sliderPosProportional,
    juce::Colour notchCol)
{
	juce::Path p;
	p.addPieSegment(ellipseRect.reduced(1.f), angle - juce::degreesToRadians(notchWidth), angle + juce::degreesToRadians(notchWidth), 0.f);
	
	if (sliderPosProportional == 0.0f){
		notchCol = notchCol.withAlpha(0.4f);
	}
	g.setColour(notchCol);
	g.fillPath (p);
}

void LAF::drawRotarySlider (juce::Graphics& g,
										const int x, const int y, const int width, const int height,
										const float sliderPosProportional,
										const float rotaryStartAngle,
										const float rotaryEndAngle,
										juce::Slider& slider)
{
    const auto wf = static_cast<float>(width);
    const auto hf = static_cast<float>(height);
    const auto xf = static_cast<float>(x);
    const auto yf = static_cast<float>(y);

	const float radius =  juce::jmin (wf, hf) * 0.4f;
	const float centreX = xf + wf  * 0.5f;
	const float centreY = yf + hf * 0.5f;
	
	// draw the circular outline
	g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
	juce::Rectangle<float> ellipseRect(centreX - radius,
									   centreY - radius,
									   radius * 2.0f,
									   radius * 2.0f);
	g.drawEllipse (ellipseRect,
				   2.1f);
	
	const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
	
	using juce::Path;
	using Point = juce::Point<float>;
	using Line = juce::Line<float>;
	
	// compute notch endpoints
	const float innerRadius     = radius * 0.2f;  // start of the notch
	const float notchLength     = radius * 0.6f;  // how long the notch is
	const float outerRadius     = innerRadius + notchLength;
	[[maybe_unused]] auto makeLine = [=](float a){
		const float x1 = centreX + std::cos (a) * innerRadius;
		const float y1 = centreY + std::sin (a) * innerRadius;
		const float x2 = centreX + std::cos (a) * outerRadius;
		const float y2 = centreY + std::sin (a) * outerRadius;
		return Line(Point(x1, y1), Point(x2, y2));
	};
	drawPieSegment(g, ellipseRect, angle, notchWidthDegrees * 1.618f, sliderPosProportional, juce::Colours::black);
}
}
