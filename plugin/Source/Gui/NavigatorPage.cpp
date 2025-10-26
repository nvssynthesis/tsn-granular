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
            comp->setBounds(juce::Rectangle<int>(left, 0, allottedCompWidth, allottedCompHeight).reduced(pad));
        }
    }
    else {
        // Top to bottom layout
        int const allottedCompWidth = localBounds.getWidth();
        int const allottedCompHeight = localBounds.getHeight() / static_cast<int>(components.size());

        for (size_t i = 0; i < components.size(); ++i){
            const int top = static_cast<int>(i) * allottedCompHeight + localBounds.getY();
            auto const comp = components[i].get();
            comp->setBounds(juce::Rectangle<int>(0, top, allottedCompWidth, allottedCompHeight).reduced(pad));
        }
    }
}
void NavigatorPanel::paint(juce::Graphics &g) {
	g.setColour(juce::Colours::whitesmoke.withAlpha(0.33f));
	g.drawRoundedRectangle(getLocalBounds().reduced(pad).toFloat(), 5.f, 1.f);
}

NavigatorPage::NavigatorPage(juce::AudioProcessorValueTreeState &apvts)
:	_apvts(apvts)
,   navigatorTypeMenu(apvts, nvs::param::ParameterRegistry::getParameterByID("navigator_type"))
{
	setSize(100, 100);

	addAndMakeVisible(navigatorTypeMenu);

	timbreSpaceParamsPanel = std::make_unique<NavigatorPanel>(_apvts, "timbre_space", false);
	addAndMakeVisible(timbreSpaceParamsPanel.get());

    navCommonParamsPanel = std::make_unique<NavigatorPanel>(_apvts, "nav_common", true);
    addAndMakeVisible(navCommonParamsPanel.get());

    navigatorTypeMenu._comboBox.addListener(this);
    updateDisplayedParameters();
}
NavigatorPage::~NavigatorPage() = default;

static std::map<nvs::timbrespace::NavigationType_e, juce::String> navTypeParamSubgroupMap
{
{nvs::timbrespace::NavigationType_e::Manual, "nav_manual"},
{nvs::timbrespace::NavigationType_e::LFO, "nav_lfo"},
{nvs::timbrespace::NavigationType_e::RandomWalk, "nav_rwalk"},
{nvs::timbrespace::NavigationType_e::Lorenz, "nav_lorenz"},
{nvs::timbrespace::NavigationType_e::Hyperchaos, "nav_hyperchaos"},
};

void NavigatorPage::updateDisplayedParameters() {
    auto const navType = static_cast<nvs::timbrespace::NavigationType_e>(navigatorTypeMenu._comboBox.getSelectedId() - 1);
    navigatorParamsPanel.reset();
    navigatorParamsPanel = std::make_unique<NavigatorPanel>(_apvts, navTypeParamSubgroupMap.at(navType));
    addAndMakeVisible(navigatorParamsPanel.get());
    repaint();
}

void NavigatorPage::comboBoxChanged(ComboBox *comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &navigatorTypeMenu._comboBox) {
        updateDisplayedParameters();
    }
}

void NavigatorPage::paint(juce::Graphics &) {
    if (navigatorParamsPanel != nullptr) {
        navigatorParamsPanel->setBounds(navParamsPanelBounds);
    }
}

void NavigatorPage::resized() {
	// carve out a strip for the combo box at the top
	auto bounds = getLocalBounds();
	auto menuArea = bounds.removeFromTop(24).reduced(4);
	navigatorTypeMenu.setBounds(menuArea);
	
	{
		const int totalW = bounds.getWidth();
		const int middleW = static_cast<int>(totalW * 0.333f);
	    const int rightW = static_cast<int>(totalW * 0.333f);

		const juce::Rectangle left {
			bounds.getX(),
			bounds.getY(),
			totalW - middleW - rightW,
			bounds.getHeight()
		};
		
		const juce::Rectangle middle {
			left.getRight(),
			bounds.getY(),
			middleW,
			bounds.getHeight()
		};

	    const juce::Rectangle right {
	        middle.getRight(),
	        bounds.getY(),
	        rightW,
	        bounds.getHeight()
	    };
		
		if (timbreSpaceParamsPanel != nullptr){
			timbreSpaceParamsPanel->setBounds(left);
		}

	    navParamsPanelBounds = middle;

	    if (navCommonParamsPanel != nullptr) {
	        navCommonParamsPanel->setBounds(right);
	    }
	}
}

