/*
  =============================================================================

    NavLFOPage.cpp
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#include "NavLFOPage.h"

#include "Navigation/Navigator.h"

NavigatorPanel::NavigatorPanel(juce::AudioProcessorValueTreeState &apvts, juce::String paramsSubGroup)
{
	for (auto &p : nvs::param::ParameterRegistry::getParametersForSubGroup(paramsSubGroup)){
		auto slider = std::make_unique<AttachedSlider>(apvts, p, juce::Slider::SliderStyle::LinearVertical);
		addAndMakeVisible(slider.get());
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
		s->setBounds(left, 0, alottedCompWidth, alottedCompHeight);
	}
}
void NavigatorPanel::paint(juce::Graphics &g) {
	g.setColour(juce::Colours::whitesmoke.withAlpha(0.33f));
	g.drawRoundedRectangle(getLocalBounds().reduced(3).toFloat(), 5.f, 1.f);
}

NavigatorPage::NavigatorPage(juce::AudioProcessorValueTreeState &apvts)
:	_apvts(apvts)
,   navigatorTypeMenu(apvts, nvs::param::ParameterRegistry::getParameterByID("navigator_type"))
{
	setSize(100, 100);

	addAndMakeVisible(navigatorTypeMenu);

	for (const auto& [navType, str] : nvs::timbrespace::navigationTypeToStringMap)
	{
	    const auto id = static_cast<int>(navType) + 1;
	    std::cout << "str: " << str << " id: " << id << std::endl;
		navigatorTypeMenu._comboBox.addItem(str, id);
	}
	selPanel = std::make_unique<NavigatorPanel>(_apvts, "selection");
	addAndMakeVisible(selPanel.get());
    navigatorTypeMenu._comboBox.addListener(this);
}
NavigatorPage::~NavigatorPage() = default;

static std::map<nvs::timbrespace::NavigationType_e, juce::String> navTypeParamSubgroupMap
{
{nvs::timbrespace::NavigationType_e::LFO, "nav_lfo"},
{nvs::timbrespace::NavigationType_e::RandomWalk, "nav_rwalk"},
{nvs::timbrespace::NavigationType_e::Lorenz, "nav_lorenz"}
};

void NavigatorPage::comboBoxChanged(ComboBox *comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &navigatorTypeMenu._comboBox) {
        auto const navType = static_cast<nvs::timbrespace::NavigationType_e>(comboBoxThatHasChanged->getSelectedId() - 1);
        navPanel.reset();
        navPanel = std::make_unique<NavigatorPanel>(_apvts, navTypeParamSubgroupMap.at(navType));
        addAndMakeVisible(navPanel.get());
        repaint();
    }
}
void NavigatorPage::paint(juce::Graphics &g) {
    if (navPanel != nullptr) {
        navPanel->setBounds(navPanelBounds);
    }
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
		navPanelBounds = right;
	}
}

