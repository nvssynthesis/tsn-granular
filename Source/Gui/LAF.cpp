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
					bool isMouseOver,
					bool isMouseDown)
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
	auto fontHeight = 14.0f;
	auto fontStyle  = button.isFrontTab() ? Font::bold : Font::plain;
	Font  f (fontName, fontHeight, fontStyle);
	g.setFont (f);
	g.setColour(Colours::white);
	g.drawFittedText (button.getButtonText(),
					  button.getLocalBounds(),
					  Justification::centred,
					  1);
}
void LAF::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
									int buttonX, int buttonY, int buttonW, int buttonH, ComboBox& cb)
{
	auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
	
	g.setColour (findColour (ComboBox::backgroundColourId));
	g.fillRect (bounds);
	
	g.setColour (findColour (ComboBox::outlineColourId));
	g.drawRect (bounds, 1.0f);
	
	// Draw the arrow:
	Path arrow;
	arrow.addTriangle ({ 0.0f, 0.0f },
					   { 1.0f, 0.0f },
					   { 0.5f, 0.5f });
	auto arrowH = buttonH * 0.3f;
	auto arrowW = arrowH * 1.3f;
	
	g.setColour (findColour (ComboBox::arrowColourId));
	g.fillPath (arrow,
				arrow.getTransformToScaleToFit (buttonX + (buttonW - arrowW) * 0.5f,
												buttonY + (buttonH - arrowH) * 0.5f,
												arrowW, arrowH,
												true));
}
Font LAF::getTextButtonFont(TextButton&, int buttonHeight) {
	return Font(fontName, 13.0, Font::plain);
}
Font LAF::getComboBoxFont (ComboBox&) {
	return getPopupMenuFont();// use same font as its popup menu
}
Font LAF::getPopupMenuFont() {
	return Font(fontName, 13.0, Font::plain);
}

void LAF::drawTooltip (Graphics &g, const String &text, int width, int height)
{
	// background
	g.setColour (findColour (TooltipWindow::backgroundColourId));
	g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);

	// border
	g.setColour (findColour (TooltipWindow::outlineColourId));
	g.drawRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f, 1.0f);

	// text
	g.setColour (findColour (TooltipWindow::textColourId));
	g.setFont (Font(fontName, 13.0, Font::plain));
//	g.drawFittedText (text, area, juce::Justification::centred, 2)
	g.drawFittedText(text, g.getClipBounds(), juce::Justification::centred, 4);
}

void LAF::drawButtonBackground (Graphics& g, Button& b,
											const Colour& backgroundColour,
											bool isMouseOverButton, bool isButtonDown)
{
	auto area = b.getLocalBounds().toFloat().reduced (1.0f);
	g.setColour (backgroundColour);
	g.fillRect (area);
	
	// subtle border
	g.setColour (findColour (TextButton::textColourOffId).withAlpha (0.3f));
	g.drawRect (area, 1.0f);
}
void LAF::drawButtonText (Graphics& g, TextButton& b,
									  bool /*isMouseOverButton*/, bool /*isButtonDown*/)
{
	auto fontSize = jmin (16.0f, b.getHeight() * 0.6f);
	g.setFont (getTextButtonFont(b, b.getHeight()));
	g.setColour (b.getToggleState()
				 ? findColour (TextButton::textColourOnId)
				 : findColour (TextButton::textColourOffId));
	
	g.drawFittedText (b.getButtonText(),
					  b.getLocalBounds().reduced (4, 2),
					  Justification::centred, 1);
}

void LAF::drawPopupMenuBackground (Graphics& g, int width, int height) {
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
void LAF::drawLinearSlider (Graphics& g,
										int x, int y, int width, int height,
										float sliderPos,
										float minSliderPos, float maxSliderPos,
										Slider::SliderStyle style,
										Slider& s)
{
	if (style == Slider::LinearBarVertical)
	{
		LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
										  sliderPos, minSliderPos, maxSliderPos,
										  style, s);
		
		Rectangle<int> tb (x, y + (height/2 - 10), width, 20);
		
		bool over    = (tb.getCentreY() >= sliderPos);
		g.setColour (over ? juce::Colour(Colours::grey).withMultipliedBrightness(1.25f) : juce::Colour(Colours::grey).withMultipliedBrightness(1.5f));
		
		g.setFont(Font (fontName, 13.5f, Font::plain));
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
void LAF::drawPieSegment(juce::Graphics &g, juce::Rectangle<float> ellipseRect, float angle, float notchWidth, float sliderPosProportional, juce::Colour notchCol)
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
										int x, int y, int width, int height,
										float sliderPosProportional,
										float rotaryStartAngle,
										float rotaryEndAngle,
										juce::Slider& slider)
{
	const float radius = juce::jmin (width, height) * 0.4f;
	const float centreX = x + width  * 0.5f;
	const float centreY = y + height * 0.5f;
	
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
		float x1 = centreX + std::cos (a) * innerRadius;
		float y1 = centreY + std::sin (a) * innerRadius;
		float x2 = centreX + std::cos (a) * outerRadius;
		float y2 = centreY + std::sin (a) * outerRadius;
		return Line(Point(x1, y1), Point(x2, y2));
	};
	drawPieSegment(g, ellipseRect, angle, notchWidthDegrees * 1.618f, sliderPosProportional, juce::Colours::black);
}
}
