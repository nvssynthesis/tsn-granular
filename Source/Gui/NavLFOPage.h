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

struct Navigator2DLFOPanel	:	public juce::Component
{
	Navigator2DLFOPanel(juce::AudioProcessorValueTreeState &apvts);
	void resized() override;
private:
	std::array<AttachedSlider, NUM_LFO2D_PARAMS> sliderArray;
};


struct NavigatorRandomWalkPanel	:	public juce::Component
{
	NavigatorRandomWalkPanel(juce::AudioProcessorValueTreeState &apvts);
	void resized() override;
private:
	std::array<AttachedSlider, NUM_RANDOM_WALK_PARAMS> sliderArray;
};


using PanelVariant = std::variant<Navigator2DLFOPanel, NavigatorRandomWalkPanel>;


struct NavLFOPage :	public juce::Component, public juce::ComboBox::Listener
{
	NavLFOPage(juce::AudioProcessorValueTreeState &apvts, nvs::nav::Navigator &navigatorVar, std::function<void(const std::vector<double>&)> onUpdateFn);
	void resized() override;
	void comboBoxChanged(juce::ComboBox* cb) override;

	void showPanel(int menuId);
	
private:
	juce::AudioProcessorValueTreeState &_apvts;
	juce::ComboBox navigatorTypeMenu;
	PanelVariant navigatorPanelVariant;
	nvs::nav::Navigator &_navigatorVariant;
	std::function<void(const std::vector<double>&)> onUpdate;
};

