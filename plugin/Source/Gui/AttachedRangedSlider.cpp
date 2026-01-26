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
        // )
        const auto lower = nvs::memoryless::clamp((xMinAtThumbDown + distanceFromStart) / getWidth(), 0.f, 1.f);
        setMinValue (proportionOfLengthToValue (lower));
        const auto upper = nvs::memoryless::clamp((xMaxAtThumbDown + distanceFromStart) / getWidth(), 0.f, 1.f);
        setMaxValue (proportionOfLengthToValue (upper));
    }
    else
    {
        Slider::mouseDrag (event);
    }
}

// This makes one thumb slide if the other is moved against it.
void RangeSlider::valueChanged()
{
    if (getMinValue() == getMaxValue())
    {
        constexpr int minimalIntervalBetweenMinAndMax = 1;
        if (getMaxValue() + minimalIntervalBetweenMinAndMax <= getMaximum())
        {
            setMaxValue(getMaxValue() + minimalIntervalBetweenMinAndMax);
        }
        else
        {
            setMinValue(getMinValue() - minimalIntervalBetweenMinAndMax);
        }
    }
}

