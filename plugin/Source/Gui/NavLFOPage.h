/*
  =============================================================================

    NavLFOPage.h
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../slicer_granular/Source/Gui/AttachedSlider.h"
#include "../slicer_granular/Source/Gui/AttachedComboBox.h"
#include "../Navigation/LFO.h"

class NavigatorPanel	:	public juce::Component
{
public:
	NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, juce::String paramsSubGroup);
	void paint(juce::Graphics &g) override;
	void resized() override;
private:
	std::vector<std::unique_ptr<AttachedSlider>> sliders;
};

class NavigatorPage :	public juce::Component
{
public:
	explicit NavigatorPage(juce::AudioProcessorValueTreeState &apvts);
	~NavigatorPage() override;
	void resized() override;

private:
	juce::AudioProcessorValueTreeState &_apvts;
	AttachedComboBox navigatorTypeMenu;

	std::unique_ptr<NavigatorPanel> selPanel;
	std::unique_ptr<NavigatorPanel> navPanel;
};

