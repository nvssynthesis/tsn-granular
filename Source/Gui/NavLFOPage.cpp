/*
  =============================================================================

    NavLFOPage.cpp
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#include "NavLFOPage.h"


NavigatorPanel::NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, navigator_category_e navigatorCategory)
{
	for (auto &p : categoryToParams.at(navigatorCategory)){
		auto slider = std::make_unique<AttachedSlider>(apvts, p, juce::Slider::SliderStyle::LinearVertical);
		addAndMakeVisible(slider->_slider);
		addAndMakeVisible(slider->_label);
		sliders.push_back(std::move(slider));
	}
}
void NavigatorPanel::resized()
{
	auto localBounds = getLocalBounds();
	int const alottedCompHeight = localBounds.getHeight();
	auto const sliderHeight = alottedCompHeight * 0.8;
	auto const labelHeight = alottedCompHeight - sliderHeight;
	int const alottedCompWidth = localBounds.getWidth() / sliders.size();
	
	for (size_t i = 0; i < sliders.size(); ++i){
		int left = (int)i * alottedCompWidth + localBounds.getX();
		auto const s = sliders[i].get();
		s->_slider.setBounds(left, 0, alottedCompWidth, sliderHeight);
		s->_label.setBounds(left, sliderHeight, alottedCompWidth, labelHeight);
	}
}



NavLFOPage::NavLFOPage(juce::AudioProcessorValueTreeState &apvts, nvs::nav::Navigator &navigatorVar, std::function<void(const std::vector<double>&)> onUpdateFn)
:	_apvts(apvts)
,	_navigatorVariant(navigatorVar)
,	onUpdate(std::move(onUpdateFn))
{
	addAndMakeVisible(navigatorTypeMenu);
	navigatorTypeMenu.addItem("2-D LFO", 1);
	navigatorTypeMenu.addItem("Random Walk", 2);
	navigatorTypeMenu.setSelectedId(1);
	navigatorTypeMenu.addListener(this);
	{
		_navigatorVariant.emplace<nvs::nav::LFO2D>(_apvts, 60.f);
		panel = std::make_unique<NavigatorPanel>(_apvts, navigator_category_e::lfo_2d);
	}
	showPanel(navigatorTypeMenu.getSelectedItemIndex());
}
void NavLFOPage::resized() {
	// carve out a strip for the combo box at the top
	auto r = getLocalBounds();
	auto menuArea = r.removeFromTop(24).reduced(4);
	navigatorTypeMenu.setBounds(menuArea);

	panel->setBounds(r);
}
void NavLFOPage::comboBoxChanged(juce::ComboBox* cb)
{
	if (cb == &navigatorTypeMenu) {
		showPanel(cb->getSelectedId());
	}
}

void NavLFOPage::showPanel(int menuId)
{
	panel->setVisible(false);
	
	// then show the one the user picked
	if (menuId == 1){
		_navigatorVariant.emplace<nvs::nav::LFO2D>(_apvts, 90.f);
		panel = std::make_unique<NavigatorPanel>(_apvts, navigator_category_e::lfo_2d);
	}
	else if (menuId == 2) {
		_navigatorVariant.emplace<nvs::nav::RandomWalkND>(_apvts, 6, 10.f, 0.01);	// dim, rate, step size
		panel = std::make_unique<NavigatorPanel>(_apvts, navigator_category_e::random_walk);
	}
	addAndMakeVisible(panel.get());
	
	std::visit([this](auto &nav){
		nav.setOnUpdateCallback(onUpdate);
	}, _navigatorVariant);
	
	// force re‐layout
	resized();
}
