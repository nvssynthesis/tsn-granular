//
// Created by Nicholas Solem on 1/25/26.
//

#include "AttachedRangedSlider.h"
#include "../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"

RangeSlider::RangeSlider ()
  : mouseDragBetweenThumbs {false}
{
    setSliderStyle (Slider::TwoValueHorizontal);
}

// To enable the section between the two thumbs to be draggable.
void RangeSlider::mouseDown (const MouseEvent& event)
{
    const float currentMouseX = event.getPosition().getX();
    const int thumbRadius = getLookAndFeel().getSliderThumbRadius (*this);
    xMinAtThumbDown = valueToProportionOfLength (getMinValue()) * getWidth();
    xMaxAtThumbDown = valueToProportionOfLength (getMaxValue()) * getWidth();

    if (currentMouseX > xMinAtThumbDown + thumbRadius && currentMouseX < xMaxAtThumbDown - thumbRadius)
    {
        mouseDragBetweenThumbs = true;
    }
    else
    {
        mouseDragBetweenThumbs = false;
        Slider::mouseDown (event);
    }
}

// To enable the section between the two thumbs to be draggable.
void RangeSlider::mouseDrag (const MouseEvent& event)
{
    const float distanceFromStart = event.getDistanceFromDragStartX();

    if (mouseDragBetweenThumbs)
    {
        const auto lower = nvs::memoryless::clamp((xMinAtThumbDown + distanceFromStart) / getWidth(), 0.f, 1.f);
        const auto upper = nvs::memoryless::clamp((xMaxAtThumbDown + distanceFromStart) / getWidth(), 0.f, 1.f);

        setMinValue (proportionOfLengthToValue (lower));
        setMaxValue (proportionOfLengthToValue (upper));
    }
    else
    {
        Slider::mouseDrag (event);
    }
}

AttachedRangeSlider::AttachedRangeSlider(juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& baseParamID,
                   juce::Slider::SliderStyle style)
: _apvts(apvts)
, _slider()
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
    auto* minParam = _apvts.getParameter(_min_param_ID);
    auto* maxParam = _apvts.getParameter(_max_param_ID);
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
    _slider.addListener(this);

    // Label
    addAndMakeVisible(_label);
    juce::String displayName = baseParamID.replace("_", " ");
    _label.setText(displayName, juce::dontSendNotification);
    _label.setJustificationType(juce::Justification::centred);
    _label.attachToComponent(&_slider, false);
}

void AttachedRangeSlider::sliderValueChanged (Slider *) {
    // Update both parameters
    if (auto* minParam = _apvts.getParameter(_min_param_ID))
    {
        const float minVal = _slider.getMinValue();
        DBG(minVal);
        minParam->setValueNotifyingHost(minVal);
    }

    if (auto* maxParam = _apvts.getParameter(_max_param_ID))
    {
        const float maxVal = _slider.getMaxValue();
        DBG(maxVal);
        maxParam->setValueNotifyingHost(maxVal);
    }
}

void AttachedRangeSlider::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Denormalize from 0-1 to slider range
    auto denormalised = newValue * (_slider.getMaximum() - _slider.getMinimum()) + _slider.getMinimum();

    if (parameterID == _min_param_ID)
    {
        _slider.setMinValue(denormalised, juce::dontSendNotification);
    }
    else if (parameterID == _max_param_ID)
    {
        _slider.setMaxValue(denormalised, juce::dontSendNotification);
    }
}
