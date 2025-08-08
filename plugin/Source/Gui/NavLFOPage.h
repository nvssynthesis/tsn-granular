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
#include "../Navigation/LFO.h"

struct NavigatorPanel	:	public juce::Component
{
	NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, juce::String paramsSubGroup);
	void paint(juce::Graphics &g) override;
	void resized() override;
private:
	std::vector<std::unique_ptr<AttachedSlider>> sliders;
};


struct NavigatorPage :	public juce::Component, public juce::ComboBox::Listener
{
	NavigatorPage(juce::AudioProcessorValueTreeState &apvts, nvs::nav::Navigator &navigatorVar);
	~NavigatorPage();
	void resized() override;
	void comboBoxChanged(juce::ComboBox* cb) override;

	void showPanel(int menuId);
	
	
	void activateLFO2D();
	void activateRandomWalk();
private:
	juce::AudioProcessorValueTreeState &_apvts;
	juce::ComboBox navigatorTypeMenu;

	std::unique_ptr<NavigatorPanel> selPanel;
	std::unique_ptr<NavigatorPanel> navPanel;
	
	nvs::nav::Navigator &_navigator;
	

};

