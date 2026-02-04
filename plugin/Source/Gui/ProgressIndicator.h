//
// Created by Nicholas Solem on 2/3/26.
//

#pragma once
#include <JuceHeader.h>
#include "RunLoopStatus.h"

class ProgressIndicator final :	public Component
{
public:
    ProgressIndicator();
    void paint(Graphics &g) override;
    void resized() override;

    void updateFromStatus(const nvs::analysis::RunLoopStatus& status);

private:
    Rectangle<int> progressBarBounds;
    TextButton stopAnalysisButton {"Stop Analysis"};

    String message {""};
    double progress {0.0};
};
