/*
  ==============================================================================

    LAF.h
    Created: 25 May 2025 2:42:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

namespace nvs::gui
{
using namespace juce;

class LAF  : public juce::LookAndFeel_V4
{
public:
	LAF();
	void drawLabel (Graphics& g, Label& label) override;
	void drawLinearSlider (Graphics& g,
						   int x, int y, int width, int height,
						   float sliderPos,
						   float minSliderPos, float maxSliderPos,
						   Slider::SliderStyle style,
						   Slider& s) override;
	
	static void drawPieSegment(juce::Graphics &g, juce::Rectangle<float> ellipseRect, float angle, float notchWidth, float sliderPosProportional, juce::Colour notchCol);
	
	void drawRotarySlider (juce::Graphics& g,
						   int x, int y, int width, int height,
						   float sliderPosProportional,
						   float rotaryStartAngle,
						   float rotaryEndAngle,
						   juce::Slider& slider) override;
	
	void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
					   int buttonX, int buttonY, int buttonW, int buttonH, ComboBox& cb) override;
	Font getComboBoxFont (ComboBox& /*cb*/) override;
	Font getTextButtonFont (TextButton&, int buttonHeight) override;
	
	Font getPopupMenuFont() override;
	void drawPopupMenuBackground (Graphics& g, int width, int height) override;
	void drawPopupMenuItem (Graphics& g,
							const Rectangle<int>& area,
							bool isSeparator, bool isActive,
							bool isHighlighted, bool isTicked,
							bool hasSubMenu, const String& text,
							const String& shortcutKeyText,
							const Drawable* icon, Colour const* textColour) override;
	
	void drawButtonBackground (Graphics& g, Button& b,
							   const Colour& backgroundColour,
							   bool isMouseOverButton, bool isButtonDown) override;
	void drawButtonText (Graphics& g, TextButton& b,
						 bool /*isMouseOverButton*/, bool /*isButtonDown*/) override;

	void drawTooltip (Graphics &, const String &text, int width, int height) override;
	
	void drawTabButton (TabBarButton& button,
						Graphics& g,
						bool isMouseOver,
						bool isMouseDown) override;

	void drawTabButtonText (TabBarButton& button,
							Graphics& g,
							bool /*isMouseOver*/,
							bool /*isMouseDown*/) override;
	Font getLabelFont (Label&) override
	{
		return Font (FontOptions(fontName, 14.0f, Font::bold));
	}
private:
	float const notchWidthDegrees {8.f};
	juce::Colour notchColour {juce::Colour(Colours::blueviolet).withMultipliedLightness(0.5f)};
	
	//	String fontName {"Gill Sans"};
		String fontName {"Palatino"};
//	String fontName {"Copperplate"};
};
}
