//
// Created by Nicholas Solem on 2/3/26.
//

#pragma once
#include <JuceHeader.h>


class ProgressIndicator final :	public Component
{
public:
    ProgressIndicator();
    void paint(Graphics &g) override;
    void resized() override;

    String message {""};
    double progress {0.0};

private:
    Rectangle<int> progressBarBounds;
    TextButton stopAnalysisButton {"Stop Analysis"};
};
