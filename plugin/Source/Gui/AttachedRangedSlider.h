//
// Created by Nicholas Solem on 1/25/26.
//

#pragma once
#include <JuceHeader.h>
#include "../slicer_granular/Source/Params/params.h"

class AttachedRangeSlider final : public juce::Component,
                                  private juce::Slider::Listener,
                                  private juce::AudioProcessorValueTreeState::Listener
{
    using ParameterDef = nvs::param::ParameterDef;
    using Slider = juce::Slider;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using Label = juce::Label;
    using String = juce::String;

public:
    AttachedRangeSlider(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& baseParamID,
                       juce::Slider::SliderStyle style)
        : apvts(apvts)
        , minParamID(baseParamID + "_min")
        , maxParamID(baseParamID + "_max")
    {
        // Verify both parameters exist
        auto* minParam = apvts.getParameter(minParamID);
        auto* maxParam = apvts.getParameter(maxParamID);
        DBG("Looking for parameters: " << minParamID << " and " << maxParamID);

        jassert(minParam != nullptr && maxParam != nullptr);

        // Setup slider as two-value slider
        jassert (style == juce::Slider::TwoValueVertical || style == juce::Slider::TwoValueHorizontal);
        slider.setSliderStyle(style);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Get the range from the parameter
        auto minParamRange = minParam->getNormalisableRange();

        DBG("Setting slider range: " << minParamRange.start << " to " << minParamRange.end);

        // Set the range
        slider.setRange(minParamRange.start, minParamRange.end, minParamRange.interval);

        // Get normalized values from parameters (0-1)
        float minNormalized = minParam->getValue();
        float maxNormalized = maxParam->getValue();

        DBG("Min param normalized value: " << minNormalized);
        DBG("Max param normalized value: " << maxNormalized);

        // Denormalize to actual slider range
        float minActual = minNormalized * (minParamRange.end - minParamRange.start) + minParamRange.start;
        float maxActual = maxNormalized * (minParamRange.end - minParamRange.start) + minParamRange.start;

        DBG("Min actual value: " << minActual);
        DBG("Max actual value: " << maxActual);

        // IMPORTANT: Set these AFTER setting the range
        slider.setMinValue(minActual, juce::dontSendNotification);
        slider.setMaxValue(maxActual, juce::dontSendNotification);

        DBG("Slider min value after set: " << slider.getMinValue());
        DBG("Slider max value after set: " << slider.getMaxValue());

        // Add listeners
        slider.addListener(this);
        apvts.addParameterListener(minParamID, this);
        apvts.addParameterListener(maxParamID, this);

        // Label
        juce::String displayName = baseParamID.replace("_", " ");
        label.setText(displayName, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&slider, false);

        addAndMakeVisible(slider);
        addAndMakeVisible(label);
    }

    ~AttachedRangeSlider() override
    {
        apvts.removeParameterListener(minParamID, this);
        apvts.removeParameterListener(maxParamID, this);
        slider.removeListener(this);
    }

    void resized() override
    {
        slider.setBounds(getLocalBounds());
    }

private:
    void sliderValueChanged(juce::Slider* s) override
    {
        // Normalize slider value to 0-1 for APVTS parameter
        auto normalizeValue = [this](double sliderValue) {
            return (sliderValue - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum());
        };

        // Update both parameters
        if (auto* minParam = apvts.getParameter(minParamID))
        {
            float normalised = normalizeValue(s->getMinValue());
            minParam->setValueNotifyingHost(normalised);
        }

        if (auto* maxParam = apvts.getParameter(maxParamID))
        {
            float normalised = normalizeValue(s->getMaxValue());
            maxParam->setValueNotifyingHost(normalised);
        }
    }

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        // Denormalize from 0-1 to slider range
        auto denormalised = newValue * (slider.getMaximum() - slider.getMinimum()) + slider.getMinimum();

        if (parameterID == minParamID)
        {
            // slider.setMinValue(denormalised, juce::dontSendNotification);
            slider.setMinValue(0.f, juce::dontSendNotification);
        }
        else if (parameterID == maxParamID)
        {
            // slider.setMaxValue(denormalised, juce::dontSendNotification);
            slider.setMaxValue(1.f, juce::dontSendNotification);
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::Slider slider;
    juce::Label label;

    juce::String minParamID;
    juce::String maxParamID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttachedRangeSlider)
};