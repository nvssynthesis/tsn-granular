/*
  ==============================================================================

    SettingsWindow.cpp
    Created: 10 Apr 2024 5:22:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "SettingsWindow.h"
#include "Settings/ModernSettingsTypes.h"

#include <memory>
#include <utility>

SettingsWindow::SettingsWindow (TSNGranularAudioProcessor& processor,
					const Colour backgroundColour)
  : DocumentWindow ("Settings", backgroundColour, allButtons),
	proc (processor)
{
    tooltipWindow.setMillisecondsBeforeTipAppears(700);

	setAlwaysOnTop (true);
	constrainer.setMinimumSize (300, 300);
	setConstrainer (&constrainer);

	// Build the tabs
	tabs = std::make_unique<TabbedComponent> (TabbedButtonBar::TabsAtTop);
	tabs->setSize (500, 400);
	
	auto settingsVT = proc.getAPVTS().state.getChildWithName ("Settings");

    using Registry_t = nvs::analysis::modern::AnalyzerSettingsRegistry;
    const Registry_t &reg = processor.getAnalyzer().getAnalyzer().getSettings();

    // For each branch in our specs map, create a tab
    nvs::analysis::modern::constexpr_for<0UL, Registry_t::numGroups>(
        [&reg, &settingsVT, this](auto i) {
            using namespace nvs::analysis::modern;
            const auto &group = reg.get<i>();
            const auto branchName = String(std::string(group.groupName));
            const std::map<String, AnySpec>& specMap = group.getSpecs();
            const auto page = createPageForBranch (settingsVT, branchName, specMap);
            tabs->addTab (branchName,
                          Colours::darkgrey,
                          page,
                          /*takeOwnership*/ true);
        });

	setContentOwned (tabs.get(), true);
}

void SettingsWindow::closeButtonPressed()
{
	delete this;
}

Component* SettingsWindow::createPageForBranch (ValueTree& settingsVT,
									  const String& branchName,
									  const std::map<String, nvs::analysis::modern::AnySpec>& specMap)
{
	struct Page  : public Component
	{
		ValueTree tree;
	    using AnySpec = nvs::analysis::modern::AnySpec;
		std::map<String, AnySpec> specs;

		Page (ValueTree t,
			  const std::map<String, AnySpec>& m)
		   : tree (std::move(t)), specs (m)
		{

			int y = 10;

			for (const auto&[specStr, specVar] : specs)
			{
				const String propName = specStr;
				const AnySpec& anySpec = specVar;

				// Now you can use propName *everywhere* without capture headaches:
				addAndMakeVisible (labels[propName]);
				labels[propName].setText (propName, dontSendNotification);
				labels[propName].setBounds (10, y, 20 * 8, 20); // adjust width

				std::visit ([&]<typename T0>(T0&& spec){
				    using namespace nvs::analysis::modern;

					using SpecT = std::decay_t<T0>;
					using RangeWithDefaultInt = RangedSettingsSpec<int>;
					using RangeWithDefaultFloat = RangedSettingsSpec<float>;
					using RangeWithDefaultDouble = RangedSettingsSpec<double>;

					if constexpr (std::is_same_v<SpecT, RangeWithDefaultInt> ||
								  std::is_same_v<SpecT, RangeWithDefaultFloat> ||
								  std::is_same_v<SpecT, RangeWithDefaultDouble> )
					{
						auto& s = sliders[propName];
						addAndMakeVisible (s);

						s.setNormalisableRange (spec.range);            // range is double
					    if constexpr (std::is_same_v<SpecT, RangeWithDefaultInt>) {
					        s.setNumDecimalPlacesToDisplay(0);
					    } else {
					        s.setNumDecimalPlacesToDisplay(spec.numDecimalPlaces);
					    }

					    if (!spec.tooltip.isEmpty()) {
					        s.setTooltip(spec.tooltip);
					    }
					    if (!spec.unit.isEmpty()) {
					        s.setTextValueSuffix(" " + spec.unit);
					    }

						auto const val = [this, spec, propName](const bool use_default){
							if (use_default){
								return static_cast<double>(spec.defaultValue);
							}
							return static_cast<double>(tree.getPropertyAsValue(propName, nullptr).getValue());
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
					else if constexpr (std::is_same_v<SpecT, ChoiceSettingsSpec>)
					{
						addAndMakeVisible (labels[propName]);
						labels[propName].setText (propName, dontSendNotification);
						labels[propName].setBounds (10, y, 150, 20);

						auto& cb = combos[propName];
						addAndMakeVisible (cb);
						cb.clear();

						// Build the ComboBox items and a helper map
						std::map<String, int> textToId;
						for (int id = 1; auto& opt : spec.options)
						{
							cb.addItem (opt, id);
							textToId[opt] = id;
							++id;
						}

						// Look up the default value’s ID (or fallback)
						const int val = [this, textToId, spec, propName](const bool use_default){
							int id = 1;
						    if (use_default) {
								auto it = textToId.find (spec.defaultValue);
								if (it != textToId.end())
									id = it->second;
							}
							else {
								auto const propVal = tree.getPropertyAsValue(propName, nullptr).getValue();
                                if (auto it = textToId.find(propVal);
                                    it != textToId.end())
                                {
									id = it->second;
								}
							}
							return id;
						}(false);

					    if (!spec.tooltip.isEmpty()) {
					        cb.setTooltip(spec.tooltip);
					    }

						cb.setSelectedId (val, dontSendNotification);
						cb.setBounds (170, y, 200, 20);

						cb.onChange = [this, propName, &cb]()
						{
							tree.setProperty (propName, cb.getText(), nullptr);
						};
					}
					else if constexpr (std::is_same_v<SpecT, BoolSettingsSpec>)
					{
						auto& tb = toggles[propName];
						addAndMakeVisible (tb);

						const bool val = [this, spec, propName](const bool use_default) -> bool {
							if (use_default){
								return static_cast<bool>(spec.defaultValue);
							}
							auto const prop = tree.getPropertyAsValue(propName, nullptr).getValue();
							return static_cast<bool>(prop);
						}(false);
						
						std::unordered_map<bool, String> displayMap {
							{
								false, "Off"
							},
							{
								true, "On"
							}
						};
						tb.setButtonText (displayMap.at(val));

					    if (!spec.tooltip.isEmpty()) {
					        tb.setTooltip(spec.tooltip);
					    }

						tb.setToggleState (val,
										   dontSendNotification);
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
		std::map<String, Label>       labels;
		std::map<String, Slider>      sliders;
		std::map<String, ComboBox>    combos;
		std::map<String, ToggleButton> toggles;
	};

	// get or create the branch VT
    const auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);
	return new Page (branchVT, specMap);
}
