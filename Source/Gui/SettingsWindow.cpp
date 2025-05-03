/*
  ==============================================================================

    SettingsWindow.cpp
    Created: 10 Apr 2024 5:22:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "SettingsWindow.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"


SettingsWindow::SettingsWindow (TsnGranularAudioProcessor& processor,
					juce::Colour backgroundColour)
  : DocumentWindow ("Settings", backgroundColour, allButtons),
	proc (processor)
{
	setAlwaysOnTop (true);
	constrainer.setMinimumSize (300, 300);
	setConstrainer (&constrainer);

	// Build the tabs
	tabs.reset (new juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop));
	tabs->setSize (500, 400);
	
	auto settingsVT = proc.getNonAutomatableState()
						 .getChildWithName ("Settings");

	
	// For each branch in our specs map, create a tab
	for (auto& [branchName, specMapPtr] : nvs::analysis::specsByBranch)
	{
		auto page = createPageForBranch (settingsVT, branchName, *specMapPtr);
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
									  const std::map<juce::String,nvs::analysis::AnySpec>& specMap)
{
	struct Page  : public juce::Component
	{
		juce::ValueTree tree;
		std::map<juce::String, nvs::analysis::AnySpec> specs;

		Page (juce::ValueTree t,
			  const std::map<juce::String, nvs::analysis::AnySpec>& m)
		   : tree (t), specs (m)
		{
			int y = 10;

			for (auto const& kv : specs)
			{
				const juce::String propName = kv.first;
				const nvs::analysis::AnySpec& anySpec = kv.second;

				// Now you can use propName *everywhere* without capture headaches:
				addAndMakeVisible (labels[propName]);
				labels[propName].setText (propName, juce::dontSendNotification);
				labels[propName].setBounds (10, y, 20 * 8, 20); // adjust width

				std::visit ([&](auto&& spec){
					using SpecT = std::decay_t<decltype(spec)>;
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
						s.setValue ((double) spec.defaultValue);
						s.setDoubleClickReturnValue(true, (double) spec.defaultValue);

						s.setBounds (170, y, 300, 20);
						s.onValueChange = [this, propName]()
						{
							double v = sliders[propName].getValue();
							// decide type by variant:
							auto& anySpec = specs.at (propName);
							std::visit ([&](auto&& sp){
								using ST = std::decay_t<decltype(sp)>;
								if constexpr (std::is_same_v<ST, RangeWithDefaultInt>)
									tree.setProperty (propName, int (std::round (v)), nullptr);
								else
									tree.setProperty (propName, (decltype(sp.defaultValue)) v, nullptr);
							}, anySpec);
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
						int id = 1;
						for (auto& opt : spec.options)
						{
							cb.addItem (opt, id);
							textToId[opt] = id;
							++id;
						}

						// Look up the default value’s ID (or fallback)
						int defaultId = 1;
						auto it = textToId.find (spec.defaultValue);
						if (it != textToId.end())
							defaultId = it->second;

						cb.setSelectedId (defaultId, juce::dontSendNotification);
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

						tb.setButtonText (propName);
						tb.setToggleState (spec.defaultValue,
										   juce::dontSendNotification);
						tb.setBounds (10, y, 200, 24);

						tb.onClick = [this, propName]()
						{
							tree.setProperty (propName,
											  toggles[propName].getToggleState(),
											  nullptr);
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
	auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);
	return new Page (branchVT, specMap);
}
