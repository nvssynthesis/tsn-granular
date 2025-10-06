/*
  =============================================================================

    NavLFOPage.h
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../Navigation/LFO.h"
#include "../slicer_granular/Source/Gui/AttachedComboBox.h"
#include "../slicer_granular/Source/Gui/AttachedSlider.h"

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
,                       private juce::ComboBox::Listener
{
public:
	explicit NavigatorPage(juce::AudioProcessorValueTreeState &apvts);
	~NavigatorPage() override;
	void resized() override;
    void paint(juce::Graphics &g) override;

private:
	juce::AudioProcessorValueTreeState &_apvts;
	AttachedComboBox navigatorTypeMenu;
    void comboBoxChanged(ComboBox *comboBoxThatHasChanged) override;

	std::unique_ptr<NavigatorPanel> selPanel;
	std::unique_ptr<NavigatorPanel> navPanel;
    juce::Rectangle<int> navPanelBounds;
};

