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
	NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, navigator_category_e navigatorCategory);
	void resized() override;
private:
	std::vector<std::unique_ptr<AttachedSlider>> sliders;
};


struct NavLFOPage :	public juce::Component, public juce::ComboBox::Listener
{
	NavLFOPage(juce::AudioProcessorValueTreeState &apvts, nvs::nav::Navigator &navigatorVar, std::function<void(const std::vector<double>&)> onUpdateFn);
	void resized() override;
	void comboBoxChanged(juce::ComboBox* cb) override;

	void showPanel(int menuId);
	
private:
	juce::AudioProcessorValueTreeState &_apvts;
	juce::ComboBox navigatorTypeMenu;
	std::unique_ptr<NavigatorPanel> panel;
	nvs::nav::Navigator &_navigatorVariant;
	std::function<void(const std::vector<double>&)> onUpdate;
};

