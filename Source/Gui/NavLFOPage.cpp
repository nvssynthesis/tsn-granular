/*
  =============================================================================

    NavLFOPage.cpp
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#include "NavLFOPage.h"


NavigatorPanel::NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, juce::String paramsSubGroup)
{
	for (auto &p : nvs::param::ParameterRegistry::getParametersForSubGroup(paramsSubGroup)){
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
void NavigatorPanel::paint(juce::Graphics &g) {
	g.setColour(juce::Colours::whitesmoke.withAlpha(0.33f));
	g.drawRoundedRectangle(getLocalBounds().reduced(3).toFloat(), 5.f, 1.f);
}

namespace {
using ActivatorFn = void (NavigatorPage::*)(void);
struct ItemData {
	int menuIndex;
	juce::String menuString;
	ActivatorFn fpActivator;
};
static const std::map<std::type_index, ItemData> navVarMenuIndices {
	{ typeid(nvs::nav::LFO2D), ItemData{
		.menuIndex=1,
		.menuString="2-D LFO",
		.fpActivator=&NavigatorPage::activateLFO2D
	} },
	{ typeid(nvs::nav::RandomWalkND), ItemData{
		.menuIndex=2,
		.menuString="Random Walk",
		.fpActivator=&NavigatorPage::activateRandomWalk
	} }
};
static const std::map<int,std::type_index> menuIndexToType = [](){
	std::map<int,std::type_index> m;
	for (auto const& [tidx, data] : navVarMenuIndices) {
		m.emplace( data.menuIndex, tidx );
		// or, if you worry about duplicate keys:
		// m.insert_or_assign(data.menuIndex, tidx);
	}
	return m;
}();
static const std::map<int,ActivatorFn> menuIndexToActivatorFn = []()
{
	std::map<int,ActivatorFn> map;
	for (auto const& [tidx, data] : navVarMenuIndices){
		map[data.menuIndex] = data.fpActivator;
	}
	return map;
}();
std::type_index getTypeIndex(nvs::nav::Navigator &nav){
	return std::visit([](auto& alt) -> std::type_index { return std::type_index(typeid(alt)); }, nav.activeNavigator );
}
ItemData getItemData(nvs::nav::Navigator &nav){
	std::type_index ti = getTypeIndex(nav);
	return navVarMenuIndices.at(ti);
}
}
NavigatorPage::NavigatorPage(juce::AudioProcessorValueTreeState &apvts, nvs::nav::Navigator &navigator)
:	_apvts(apvts)
,	_navigator(navigator)
{
	setSize(100, 100);

	addAndMakeVisible(navigatorTypeMenu);

	for (auto [typeId, itemData] : navVarMenuIndices)
	{
		navigatorTypeMenu.addItem(itemData.menuString, itemData.menuIndex);
	}
	auto const itemData = getItemData(_navigator);
	navigatorTypeMenu.setSelectedId(itemData.menuIndex);
	std::invoke(itemData.fpActivator, this);
	auto i = navigatorTypeMenu.getSelectedId();
	showPanel(i);

	navigatorTypeMenu.addListener(this);
	
	selPanel = std::make_unique<NavigatorPanel>(_apvts, "selection");
	addAndMakeVisible(selPanel.get());
	
}
NavigatorPage::~NavigatorPage(){
	navigatorTypeMenu.removeListener(this);
}
void NavigatorPage::resized() {
	// carve out a strip for the combo box at the top
	auto bounds = getLocalBounds();
	auto menuArea = bounds.removeFromTop(24).reduced(4);
	navigatorTypeMenu.setBounds(menuArea);
	
	{
		int totalW = bounds.getWidth();
		int splitW = int (totalW * 0.26f);
		
		juce::Rectangle<int> left {
			bounds.getX(),
			bounds.getY(),
			splitW,
			bounds.getHeight()
		};
		
		juce::Rectangle<int> right {
			bounds.getX() + splitW,
			bounds.getY(),
			totalW - splitW,
			bounds.getHeight()
		};
		
		if (selPanel != nullptr){
			selPanel->setBounds(left);
		}
		if (navPanel != nullptr){
			navPanel->setBounds(right);
		}
	}
}

void NavigatorPage::comboBoxChanged(juce::ComboBox* cb)
{
	if (cb == &navigatorTypeMenu) {
		showPanel(cb->getSelectedId());
	}
}

void NavigatorPage::activateLFO2D(){
	if (!std::holds_alternative<nvs::nav::LFO2D>(_navigator.activeNavigator)){
		_navigator.setLFO2D(_apvts);
	}
	navPanel = std::make_unique<NavigatorPanel>(_apvts, "nav_lfo");
}
void NavigatorPage::activateRandomWalk(){
	if (!std::holds_alternative<nvs::nav::RandomWalkND>(_navigator.activeNavigator)){
		_navigator.setRandomWalkND(_apvts);
	}
	navPanel = std::make_unique<NavigatorPanel>(_apvts, "nav_rwalk");
}
void NavigatorPage::showPanel(int menuId)
{
	navPanel->setVisible(false);
	
	
	// then show the one the user picked
//	if (menuId == navVarMenuIndices.at(typeid(nvs::nav::LFO2D)).menuIndex){
//		activateLFO2D();
//	}
//	else if (menuId == navVarMenuIndices.at(typeid(nvs::nav::RandomWalkND)).menuIndex) {
//		activateRandomWalk();
//	}
	jassert (menuId > 0);
	std::invoke(menuIndexToActivatorFn.at(menuId), this);
	
	addAndMakeVisible(navPanel.get());
	
	// force re‐layout
	resized();
}
