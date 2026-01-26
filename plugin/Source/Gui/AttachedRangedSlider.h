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

    bool mouseDragBetweenThumbs;
    float xMinAtThumbDown {0.f};
    float xMaxAtThumbDown {0.f};
};

class AttachedRangeSlider final : public juce::Component
,                                private juce::Slider::Listener
,                                private juce::AudioProcessorValueTreeState::Listener
{
    using ParameterDef = nvs::param::ParameterDef;
    using Slider = juce::Slider;
    using Label = juce::Label;
    using String = juce::String;

public:
    AttachedRangeSlider(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& baseParamID,
                       juce::Slider::SliderStyle style);

    void resized() override
    {
        _slider.setBounds(getLocalBounds());
    }


private:
    juce::LookAndFeel_V4 lookAndFeel;
    AudioProcessorValueTreeState& _apvts;
    RangeSlider _slider;

    juce::String _min_param_ID;
    juce::String _max_param_ID;

    juce::Label _label;

    String _min_param_name;
    String _max_param_name;

    void sliderValueChanged (Slider *) override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttachedRangeSlider)
};
