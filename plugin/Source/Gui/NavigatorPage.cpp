/*
  =============================================================================

    NavLFOPage.cpp
    Created: 23 Jan 2025 4:16:56pm
    Author:  Nicholas Solem

  =============================================================================
*/

#include "NavigatorPage.h"

#include "Navigation/Navigator.h"

NavigatorPanel::NavigatorPanel(juce::AudioProcessorValueTreeState &apvts,
                               const juce::String &paramsSubGroup,
                               const bool isHorizontal)
    : horizontal(isHorizontal)
{
    for (auto &p : nvs::param::ParameterRegistry::getParametersForSubGroup(paramsSubGroup)){
        if (p.getParameterType() == nvs::param::ParameterType::Float) {
            auto slider = std::make_unique<AttachedSlider>(apvts, p, isHorizontal ? Slider::SliderStyle::LinearVertical : Slider::SliderStyle::LinearHorizontal);
            addAndMakeVisible(slider.get());
            components.push_back(std::move(slider));
        }
        else if (p.getParameterType() == nvs::param::ParameterType::Choice) {
            auto cb = std::make_unique<AttachedComboBox>(apvts, p);
            addAndMakeVisible(cb.get());
            components.push_back(std::move(cb));
        }
    }
}

void NavigatorPanel::resized()
{
    const auto localBounds = getLocalBounds();

    if (horizontal) {
        // Left to right layout
        int const allottedCompHeight = localBounds.getHeight();
        auto const sliderHeight = allottedCompHeight * 0.8;
        auto const labelHeight = allottedCompHeight - sliderHeight;
        int const allottedCompWidth = localBounds.getWidth() / components.size();

        for (size_t i = 0; i < components.size(); ++i){
            const int left = static_cast<int>(i) * allottedCompWidth + localBounds.getX();
            auto const comp = components[i].get();
            comp->setBounds(left, 0, allottedCompWidth, allottedCompHeight);
        }
    }
    else {
        // Top to bottom layout
        int const allottedCompWidth = localBounds.getWidth();
        int const allottedCompHeight = localBounds.getHeight() / static_cast<int>(components.size());

        for (size_t i = 0; i < components.size(); ++i){
            const int top = static_cast<int>(i) * allottedCompHeight + localBounds.getY();
            auto const comp = components[i].get();
            comp->setBounds(0, top, allottedCompWidth, allottedCompHeight);
        }
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

	timbreSpacePanel = std::make_unique<NavigatorPanel>(_apvts, "timbre_space", false);
	addAndMakeVisible(timbreSpacePanel.get());
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
void NavigatorPage::paint(juce::Graphics &) {
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
		
		if (timbreSpacePanel != nullptr){
			timbreSpacePanel->setBounds(left);
		}
		navPanelBounds = right;
	}
}

