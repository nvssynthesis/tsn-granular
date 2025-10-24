/*
  ==============================================================================

    SettingsWindow.cpp
    Created: 10 Apr 2024 5:22:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "SettingsWindow.h"

#include <memory>
#include <utility>

SettingsWindow::SettingsWindow (TSNGranularAudioProcessor& processor,
					const juce::Colour backgroundColour)
  : DocumentWindow ("Settings", backgroundColour, allButtons),
	proc (processor)
{
	setAlwaysOnTop (true);
	constrainer.setMinimumSize (300, 300);
	setConstrainer (&constrainer);

	// Build the tabs
	tabs = std::make_unique<juce::TabbedComponent> (juce::TabbedButtonBar::TabsAtTop);
	tabs->setSize (500, 400);
	
	auto settingsVT = proc.getAPVTS().state.getChildWithName ("Settings");
	
	// For each branch in our specs map, create a tab
	for (auto& [branchName, specMapPtr] : nvs::analysis::specsByBranch)
	{
		const auto page = createPageForBranch (settingsVT, branchName, *specMapPtr);
		tabs->addTab (branchName,
					  juce::Colours::darkgrey,
					  page,
					  /*takeOwnership*/ true);
	}

	setContentOwned (tabs.get(), true);
}

void SettingsWindow::closeButtonPressed()
{
	delete this;
}

juce::Component* SettingsWindow::createPageForBranch (juce::ValueTree& settingsVT,
									  const juce::String& branchName,
									  const std::map<juce::String,nvs::analysis::AnySpec>& specMap) {
	struct Page  : public juce::Component
	{
		juce::ValueTree tree;
		std::map<juce::String, nvs::analysis::AnySpec> specs;

		Page (juce::ValueTree t,
			  const std::map<juce::String, nvs::analysis::AnySpec>& m)
		   : tree (std::move(t)), specs (m)
		{
			int y = 10;

			for (const auto&[specStr, specVar] : specs)
			{
				const juce::String propName = specStr;
				const nvs::analysis::AnySpec& anySpec = specVar;

				// Now you can use propName *everywhere* without capture headaches:
				addAndMakeVisible (labels[propName]);
				labels[propName].setText (propName, juce::dontSendNotification);
				labels[propName].setBounds (10, y, 20 * 8, 20); // adjust width

				std::visit ([&]<typename T0>(T0&& spec){
					using SpecT = std::decay_t<T0>;
					using RangeWithDefaultInt = nvs::analysis::RangeWithDefault<int>;
					using RangeWithDefaultFloat = nvs::analysis::RangeWithDefault<float>;
					using RangeWithDefaultDouble = nvs::analysis::RangeWithDefault<double>;

					if constexpr (std::is_same_v<SpecT, RangeWithDefaultInt> ||
								  std::is_same_v<SpecT, RangeWithDefaultFloat> ||
								  std::is_same_v<SpecT, RangeWithDefaultDouble> )
					{
						auto& s = sliders[propName];
						addAndMakeVisible (s);

						s.setNormalisableRange (spec.range);            // range is double
					    s.setNumDecimalPlacesToDisplay(3);
						
						auto const val = [this, spec, propName](const bool use_default){
							if (use_default){
								return static_cast<double>(spec.defaultValue);
							}
							else {
								return static_cast<double>(tree.getPropertyAsValue(propName, nullptr).getValue());
							}
						}(false);
						
						s.setValue (val);
						
						s.setDoubleClickReturnValue(true, static_cast<double>(spec.defaultValue));

						s.setBounds (170, y, 300, 20);
						s.onValueChange = [this, propName]()
						{
							double v = sliders[propName].getValue();
							// decide type by variant:
							auto& propSpec = specs.at (propName);
							std::visit ([&]<typename spec_t>([[maybe_unused]] spec_t && sp){
								using ST = std::decay_t<spec_t>;
								if constexpr (std::is_same_v<ST, RangeWithDefaultInt>)
									tree.setProperty (propName, static_cast<int>(std::round(v)), nullptr);
								else
									tree.setProperty (propName, static_cast<decltype(sp.defaultValue)>(v), nullptr);
							}, propSpec);
						};
					}
					else if constexpr (std::is_same_v<SpecT, nvs::analysis::ChoiceWithDefault>)
					{
						addAndMakeVisible (labels[propName]);
						labels[propName].setText (propName, juce::dontSendNotification);
						labels[propName].setBounds (10, y, 150, 20);

						auto& cb = combos[propName];
						addAndMakeVisible (cb);
						cb.clear();

						// Build the ComboBox items and a helper map
						std::map<juce::String, int> textToId;
						for (int id = 1; auto& opt : spec.options)
						{
							cb.addItem (opt, id);
							textToId[opt] = id;
							++id;
						}

						// Look up the default value’s ID (or fallback)
						int val = [this, textToId, spec, propName](bool use_default){
							int id = 1;
						    if (use_default){
								auto it = textToId.find (spec.defaultValue);
								if (it != textToId.end())
									id = it->second;
							}
							else {
								auto const propVal = tree.getPropertyAsValue(propName, nullptr).getValue();
								auto it = textToId.find(propVal);
								if (it != textToId.end()){
									id = it->second;
								}
							}
							return id;
						}(false);
						
						cb.setSelectedId (val, juce::dontSendNotification);
						cb.setBounds (170, y, 200, 20);

						cb.onChange = [this, propName, &cb]()
						{
							tree.setProperty (propName, cb.getText(), nullptr);
						};
					}
					else if constexpr (std::is_same_v<SpecT, nvs::analysis::BoolWithDefault>)
					{
						auto& tb = toggles[propName];
						addAndMakeVisible (tb);


						
						bool val = [this, spec, propName](const bool use_default) -> bool {
							if (use_default){
								return static_cast<bool>(spec.defaultValue);
							}
							else {
								auto const prop = tree.getPropertyAsValue(propName, nullptr).getValue();
								return static_cast<bool>(prop);
							}
						}(false);
						
						std::unordered_map<bool, juce::String> displayMap {
							{
								false, "Off"
							},
							{
								true, "On"
							}
						};
						tb.setButtonText (displayMap.at(val));

						tb.setToggleState (val,
										   juce::dontSendNotification);
						tb.setBounds (170, y, 200, 24);

						tb.onClick = [this, propName, displayMap]()
						{
							auto const togState = toggles[propName].getToggleState();
							tree.setProperty (propName,
											  togState,
											  nullptr);
							toggles[propName].setButtonText(displayMap.at(togState));
						};
					}
				}, anySpec);
				y += 30;
			}
			setSize (500, y + 20);
		}
		std::map<juce::String, juce::Label>       labels;
		std::map<juce::String, juce::Slider>      sliders;
		std::map<juce::String, juce::ComboBox>    combos;
		std::map<juce::String, juce::ToggleButton> toggles;
	};

	// get or create the branch VT
    const auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);
	return new Page (branchVT, specMap);
}
