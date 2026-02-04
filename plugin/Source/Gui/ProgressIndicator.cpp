//
// Created by Nicholas Solem on 2/3/26.
//

#include "ProgressIndicator.h"

ProgressIndicator::ProgressIndicator() {
    addAndMakeVisible(stopAnalysisButton);
    stopAnalysisButton.setButtonText("Stop Analysis");
    stopAnalysisButton.onClick = [this]() {

    };
}

void ProgressIndicator::updateFromStatus(const nvs::analysis::RunLoopStatus& status) {
    progress = status.getProgress();
    message = status.getMessage();
    repaint();
}

void ProgressIndicator::paint(Graphics &g) {
    {
        // draw progress bar
        g.setColour(Colours::whitesmoke);
        constexpr auto cornerSize = 8.0f;
        const auto progressBarBoundsF = progressBarBounds.toFloat();
        g.fillRoundedRectangle(progressBarBoundsF, cornerSize);

        g.setColour(Colours::black);
        g.drawRoundedRectangle(progressBarBoundsF, cornerSize, 2.f);

        const auto partialW = static_cast<int>(progressBarBounds.getWidth() * progress);
        const auto progressBarRect = progressBarBounds.withWidth(partialW);
        g.fillRoundedRectangle(progressBarRect.toFloat(), cornerSize);

        g.setColour(Colours::whitesmoke);
        g.setFont(FontOptions("Courier New", 15.f, Font::FontStyleFlags::plain));
        g.drawText(message, progressBarBounds, Justification::centred);
    }
}
void ProgressIndicator::resized() {
    const auto b = getLocalBounds().toFloat();

    progressBarBounds = b.withHeight(b.getHeight() * (5.f/11.f)).toNearestInt();

    auto btnRect = b.withTrimmedTop(b.getHeight() * (6.f/11.f)).toNearestInt();
    btnRect = btnRect.withSizeKeepingCentre(static_cast<int>(static_cast<float>(btnRect.getWidth()) / 5.f),
        btnRect.getHeight());

    stopAnalysisButton.setBounds(btnRect);
}