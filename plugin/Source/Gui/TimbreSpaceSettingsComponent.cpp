/*
  ==============================================================================

    TimbreSpaceSettingsComponent.cpp
    Created: 1 May 2025 12:49:57am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceSettingsComponent.h"
#if false	// this is completely unused, opted for generic settings


TimbreSpaceSettingsComponent::TimbreSpaceSettingsComponent(TSNGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed)
:	proc(p), editor(ed)
{
	using namespace nvs::analysis;
	
	for (auto f : featuresIterator()){
		featureXMenu.addItem(toString(f), int(f) + 1);
		featureYMenu.addItem(toString(f), int(f) + 1);
	}
	featureXMenu.onChange = [this]() {
		handleXMenuChange();
	};
	featureYMenu.onChange = [this]() {
		handleYMenuChange();
	};
	addAndMakeVisible(featureXMenu);
	addAndMakeVisible(featureYMenu);
	
	setSize (100, 100);
}
	
void TimbreSpaceSettingsComponent::resized() {
	featureXMenu.setBounds(10, 10, 200, 24);
	featureYMenu.setBounds(10, 36, 200, 24);
}

void TimbreSpaceSettingsComponent::handleXMenuChange()
{
	using namespace nvs::analysis;
	int selectedId = featureXMenu.getSelectedId();
	std::cout << "x menu selection: " << toString(Features(selectedId)) << " \n";
}
void TimbreSpaceSettingsComponent::handleYMenuChange()
{
	using namespace nvs::analysis;
	int selectedId = featureYMenu.getSelectedId();
	std::cout << "y menu selection: " << toString(Features(selectedId)) << " \n";
}

void TimbreSpaceSettingsComponent::buttonClicked(juce::Button *button) {
	if (button == &applyButton){
		// set settings in value tree!
		std::cout << " TimbreSpaceSettingsComponent::buttonClicked\n" ;
	}
}
#endif
