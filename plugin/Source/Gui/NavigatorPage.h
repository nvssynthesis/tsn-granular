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
	NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, const juce::String &paramsSubGroup, bool isHorizontal=true);
	void paint(juce::Graphics &g) override;
	void resized() override;
private:
	std::vector<std::unique_ptr<juce::Component>> components;
    bool horizontal;
    static constexpr int pad {3};
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
    void updateDisplayedParameters();

	std::unique_ptr<NavigatorPanel> timbreSpacePanel;
	std::unique_ptr<NavigatorPanel> navPanel;
    std::unique_ptr<NavigatorPanel> navCommonParamsPanel;
    juce::Rectangle<int> navPanelBounds;
};

