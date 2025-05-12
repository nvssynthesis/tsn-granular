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

struct Navigator2DLFO	:	public juce::Component
{
	Navigator2DLFO(juce::AudioProcessorValueTreeState &apvts)
	:	sliderArray
	{
		AttachedSlider(apvts, params_e::nav_lfo_2d_amount, juce::Slider::SliderStyle::LinearVertical),
		AttachedSlider(apvts, params_e::nav_lfo_2d_rate, juce::Slider::SliderStyle::LinearVertical),
		AttachedSlider(apvts, params_e::nav_lfo_2d_offset_x, juce::Slider::SliderStyle::LinearVertical),
		AttachedSlider(apvts, params_e::nav_lfo_2d_offset_y, juce::Slider::SliderStyle::LinearVertical)
	}
	{
		for (auto &s : sliderArray){
			addAndMakeVisible(s._slider);
		}
	}
	void resized() override
	{
		auto localBounds = getLocalBounds();
		int const alottedCompHeight = localBounds.getHeight();
		int const alottedCompWidth = localBounds.getWidth() / sliderArray.size();
		
		for (int i = 0; i < sliderArray.size(); ++i){
			int left = i * alottedCompWidth + localBounds.getX();
			sliderArray[i]._slider.setBounds(left, 0, alottedCompWidth, alottedCompHeight);
		}
	}
private:
	std::array<AttachedSlider, NUM_NAVIGATION_PARAMS> sliderArray;
};

struct Navigator6DLFO	:	public juce::Component
{
	Navigator6DLFO(juce::AudioProcessorValueTreeState &apvts){}
};

struct NavLFOPage :	public juce::Component, public juce::ComboBox::Listener
{
	NavLFOPage(juce::AudioProcessorValueTreeState &apvts)
	{
		addAndMakeVisible(navigatorTypeMenu);
		navigatorTypeMenu.addItem("2-D LFO", 1);
		navigatorTypeMenu.addItem("6-D LFO", 2);
		navigatorTypeMenu.setSelectedId(1);
		navigatorTypeMenu.addListener(this);

		twoDPanel.reset(new Navigator2DLFO(apvts));
		sixDPanel.reset(new Navigator6DLFO(apvts));
		addAndMakeVisible(twoDPanel.get());
		addAndMakeVisible(sixDPanel.get());

		showPanel(1);
	}
	void resized() override {
		// carve out a strip for the combo box at the top
		auto r = getLocalBounds();
		auto menuArea = r.removeFromTop(24).reduced(4);
		navigatorTypeMenu.setBounds(menuArea);

		// everything else is for the active panel
		if (activePanel) {
			activePanel->setBounds(r);
		}
	}
	void comboBoxChanged(juce::ComboBox* cb) override
	{
		if (cb == &navigatorTypeMenu) {
			showPanel(cb->getSelectedId());
		}
	}

	void showPanel(int menuId)
	{
		// hide both
		twoDPanel->setVisible(false);
		sixDPanel->setVisible(false);

		// then show the one the user picked
		if (menuId == 1)      activePanel = twoDPanel.get();
		else if (menuId == 2) activePanel = sixDPanel.get();

		if (activePanel) {
			activePanel->setVisible(true);
		}
		// force re‐layout
		resized();
	}
private:
	juce::ComboBox navigatorTypeMenu;
	std::unique_ptr<juce::Component> twoDPanel, sixDPanel;
	juce::Component* activePanel = nullptr;};

