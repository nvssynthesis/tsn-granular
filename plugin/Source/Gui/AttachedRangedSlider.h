//
// Created by Nicholas Solem on 1/25/26.
//

#pragma once
#include <JuceHeader.h>
#include "../slicer_granular/Source/Params/params.h"

class RangeSlider final : public Slider
{
public:
    RangeSlider();
private:
    void mouseDown (const MouseEvent& event) override;
    void mouseDrag (const MouseEvent& event) override;
    void valueChanged() override;

    bool mouseDragBetweenThumbs;
    float xMinAtThumbDown;
    float xMaxAtThumbDown;
};

class AttachedRangeSlider final : public juce::Component
{
    using ParameterDef = nvs::param::ParameterDef;
    using Slider = juce::Slider;
    using Label = juce::Label;
    using String = juce::String;

public:
    AttachedRangeSlider(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& baseParamID,
                       juce::Slider::SliderStyle style)
    : _slider()
    , _min_param_ID(baseParamID + "_min")
    , _max_param_ID(baseParamID + "_max")
    {
        const ParameterDef minParamDef = *std::ranges::find(nvs::param::ALL_PARAMETERS, _min_param_ID,
            [](const auto& x){
                return x.ID;
            });
        const ParameterDef maxParamDef = *std::ranges::find(nvs::param::ALL_PARAMETERS, _max_param_ID,
        [](const auto& x){
            return x.ID;
        });
        _min_param_name = minParamDef.displayName;
        _max_param_name = maxParamDef.displayName;
        
        // Verify both parameters exist
        auto* minParam = apvts.getParameter(_min_param_ID);
        auto* maxParam = apvts.getParameter(_max_param_ID);
        jassert(minParam != nullptr && maxParam != nullptr);

        // Setup _slider as two-value _slider
        jassert (style == Slider::TwoValueVertical || style == juce::Slider::TwoValueHorizontal);

        addAndMakeVisible(_slider);
        _slider.setSliderStyle(style);
        _slider.setNormalisableRange(minParamDef.createNormalisableRange<double>());
        _slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);

        _slider.setColour(Slider::ColourIds::thumbColourId, juce::Colours::palevioletred);
        _slider.setColour(Slider::ColourIds::textBoxTextColourId, juce::Colours::lightgrey);

        _slider.setLookAndFeel(&lookAndFeel);

        // Label
        addAndMakeVisible(_label);
        juce::String displayName = baseParamID.replace("_", " ");
        _label.setText(displayName, juce::dontSendNotification);
        _label.setJustificationType(juce::Justification::centred);
        _label.attachToComponent(&_slider, false);
    }

    void resized() override
    {
        _slider.setBounds(getLocalBounds());
    }

private:
    juce::LookAndFeel_V4 lookAndFeel;

    RangeSlider _slider;

    juce::String _min_param_ID;
    juce::String _max_param_ID;

    juce::Label _label;

    String _min_param_name;
    String _max_param_name;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttachedRangeSlider)
};
