/*
  =============================================================================

    NavLFOPage.cpp
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#include "NavLFOPage.h"


Navigator2DLFOPanel::Navigator2DLFOPanel(juce::AudioProcessorValueTreeState &apvts)
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
void Navigator2DLFOPanel::resized()
{
	auto localBounds = getLocalBounds();
	int const alottedCompHeight = localBounds.getHeight();
	int const alottedCompWidth = localBounds.getWidth() / sliderArray.size();
	
	for (size_t i = 0; i < sliderArray.size(); ++i){
		int left = (int)i * alottedCompWidth + localBounds.getX();
		sliderArray[i]._slider.setBounds(left, 0, alottedCompWidth, alottedCompHeight);
	}
}


NavigatorRandomWalkPanel::NavigatorRandomWalkPanel(juce::AudioProcessorValueTreeState &apvts)
:	sliderArray
{
	AttachedSlider(apvts, params_e::nav_random_walk_step_size, juce::Slider::SliderStyle::LinearVertical),
	AttachedSlider(apvts, params_e::nav_random_walk_rate, juce::Slider::SliderStyle::LinearVertical)
}
{
	for (auto &s : sliderArray){
		addAndMakeVisible(s._slider);
	}
}
void NavigatorRandomWalkPanel::resized()
{
	auto localBounds = getLocalBounds();
	int const alottedCompHeight = localBounds.getHeight();
	int const alottedCompWidth = localBounds.getWidth() / sliderArray.size();
	
	for (size_t i = 0; i < sliderArray.size(); ++i){
		int left = (int)i * alottedCompWidth + localBounds.getX();
		sliderArray[i]._slider.setBounds(left, 0, alottedCompWidth, alottedCompHeight);
	}
}

NavLFOPage::NavLFOPage(juce::AudioProcessorValueTreeState &apvts, nvs::nav::Navigator &navigatorVar, std::function<void(const std::vector<double>&)> onUpdateFn)
:	_apvts(apvts)
,	navigatorPanelVariant { std::in_place_type<Navigator2DLFOPanel>, _apvts }
,	_navigatorVariant(navigatorVar)
,	onUpdate(std::move(onUpdateFn))
{
	addAndMakeVisible(navigatorTypeMenu);
	navigatorTypeMenu.addItem("2-D LFO", 1);
	navigatorTypeMenu.addItem("6-D LFO", 2);
	navigatorTypeMenu.setSelectedId(1);
	navigatorTypeMenu.addListener(this);

	std::visit([this](auto &nav){
		addAndMakeVisible(nav);
	}, navigatorPanelVariant);

	showPanel(1);
}
void NavLFOPage::resized() {
	// carve out a strip for the combo box at the top
	auto r = getLocalBounds();
	auto menuArea = r.removeFromTop(24).reduced(4);
	navigatorTypeMenu.setBounds(menuArea);

	std::visit([r](auto &nav){
		nav.setBounds(r);
	}, navigatorPanelVariant);
	
}
void NavLFOPage::comboBoxChanged(juce::ComboBox* cb)
{
	if (cb == &navigatorTypeMenu) {
		showPanel(cb->getSelectedId());
	}
}

void NavLFOPage::showPanel(int menuId)
{
	// hide active
	std::visit([](auto &nav){
		nav.setVisible(false);
	}, navigatorPanelVariant);
	

	// then show the one the user picked
	if (menuId == 1){
		navigatorPanelVariant.emplace<Navigator2DLFOPanel>( _apvts );
		_navigatorVariant.emplace<nvs::nav::LFO2D>(_apvts, 60.f);
	}
	else if (menuId == 2) {
		navigatorPanelVariant.emplace<NavigatorRandomWalkPanel>( _apvts );
		_navigatorVariant.emplace<nvs::nav::RandomWalkND>(6, 1, 0.5);
	}
	std::visit([this](auto &navPanel){
		addAndMakeVisible(navPanel);
	}, navigatorPanelVariant);
	
	std::visit([this](auto &nav){
		nav.setOnUpdateCallback(onUpdate);
	}, _navigatorVariant);
	
	// force re‐layout
	resized();
}
