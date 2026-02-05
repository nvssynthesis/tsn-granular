/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "Gui/SettingsWindow.h"
#include "Gui/SegmentedWaveformComponent.h"
#include "Gui/NavigatorPage.h"
#include "RunLoopStatus.h"

//==============================================================================

TsnGranularAudioProcessorEditor::TsnGranularAudioProcessorEditor (TSNGranularAudioProcessor& p)
:	AudioProcessorEditor (&p)
,	GranularEditorCommon (p)
,	timbreSpaceComponent(p)
,	backgroundNeedsUpdate(true)
,	analysisButton()
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	TSNaudioProcessor(p)
{
    waveformComponent = std::make_unique<SegmentedWaveformComponent>(TSNaudioProcessor);

	setSize (680, 950);
	setResizable(true, true);

	addAndMakeVisible(grainBusyDisplay);
	addAndMakeVisible(presetPanel);

    setAnalysisButtonFunctionality();
	addAndMakeVisible(analysisButton);
	
	addAndMakeVisible(writeWavsButton);
	writeWavsButton.onClick = [&p]{p.writeEvents();};
	
	addAndMakeVisible(settingsButton);
	settingsButton.onClick = [this]{
		closeAllWindows();
		popupSettings(false);
	};
	
	tabbedPages.addTab("TSN", juce::Colours::transparentWhite, new NavigatorPage(audioProcessor.getAPVTS()), true);
	tabbedPages.moveTab(3, -1);	// fx, which is added in basic TabbedPages constructor (used already in slicer version), should still be last tab
	
	addAndMakeVisible(tabbedPages);

    jassert (waveformComponent != nullptr);
	addAndMakeVisible(*waveformComponent);

	addAndMakeVisible(timbreSpaceComponent);
	timbreSpaceComponent.addMouseListener(this, false);
	
#pragma message("need to fix this part based on the new changes. We don't want to do unecessary point calculations on construction, but do want to draw already-stored point data.")

	auto &a = TSNaudioProcessor.getAnalyzer();
	a.getStatus().addChangeListener(&timbreSpaceComponent);	// to tell timbre space comp to make progress bar visible
	a.addListener(&timbreSpaceComponent);		// tell timbre space comp to hide progress bar if thread exits early
	a.addChangeListener(&timbreSpaceComponent); // tell timbre space comp to hide progress bar when analysis successfully completes
    a.addChangeListener(waveformComponent.get());
    a.addChangeListener(this);  // to change the functionality of analysisButton

    if (const auto x = dynamic_cast<juce::ActionListener*>(waveformComponent.get())) {
        auto &ts = TSNaudioProcessor.getTimbreSpace();
        ts.addActionListener(x);
    }

    setLookAndFeel(&laf);

    startTimerHz(33);

	getConstrainer()->setMinimumSize(240, 360);
}

TsnGranularAudioProcessorEditor::~TsnGranularAudioProcessorEditor()
{
    stopTimer();

    setLookAndFeel(nullptr);

	closeAllWindows();
	
	auto &a = TSNaudioProcessor.getAnalyzer();
	a.getStatus().removeChangeListener(&timbreSpaceComponent);
	a.removeListener(&timbreSpaceComponent);
	a.removeChangeListener(&timbreSpaceComponent);
	a.removeChangeListener(this);
    jassert (waveformComponent != nullptr);
    if (const auto x = dynamic_cast<juce::ActionListener*>(waveformComponent.get())) {
        auto &ts = TSNaudioProcessor.getTimbreSpace();
        ts.removeActionListener(x);
    }
    a.removeChangeListener(waveformComponent.get());
}
//==============================================================================
void TsnGranularAudioProcessorEditor::closeAllWindows()
{
	for (auto& window : windows){
		window.deleteAndZero();
	}
	windows.clear();
}
//==============================================================================
void TsnGranularAudioProcessorEditor::popupSettings(const bool native){
    constexpr int intensity = 70;
	auto* settingsWindow = new SettingsWindow (TSNaudioProcessor, juce::Colour(intensity, intensity, intensity));
	windows.add (settingsWindow);

	const juce::Rectangle<int> area (0, 0, 500, 400);

	const juce::RectanglePlacement placement (juce::RectanglePlacement::xMid
										| juce::RectanglePlacement::yTop
										| juce::RectanglePlacement::doNotResize);

	auto result = placement.appliedTo (area, juce::Desktop::getInstance().getDisplays()
												 .getPrimaryDisplay()->userArea.reduced (20));
	settingsWindow->setBounds (result);

	settingsWindow->setResizable (true, ! native);
	settingsWindow->setUsingNativeTitleBar (native);
	settingsWindow->setVisible (true);
}

void TsnGranularAudioProcessorEditor::mouseDown(const juce::MouseEvent &) {}
void TsnGranularAudioProcessorEditor::mouseDrag(const juce::MouseEvent &) {}
//==============================================================================
void TsnGranularAudioProcessorEditor::timerCallback() {
    timbreSpaceComponent.repaint(); // EXPENSIVE
    jassert (waveformComponent != nullptr);
    waveformComponent->highlightOnsets(TSNaudioProcessor.getTsnGranularSynthesizer()->getTimbreSpacePointSelector().getCurrentPointIndices());
}

void TsnGranularAudioProcessorEditor::setAnalysisButtonFunctionality() {
    const auto state = TSNaudioProcessor.getAnalyzer().getState();
    using State = nvs::analysis::ThreadedAnalyzer::State;
    switch (state) {
        case State::Idle: {
            analysisButton.setButtonText("Calculate Analysis");
            analysisButton.onClick = [this]{
                TSNaudioProcessor.askForAnalysis();
            };
            break;
        }
        case State::Failed: {
            analysisButton.setButtonText("Calculate Analysis");
            analysisButton.onClick = [this]{
                TSNaudioProcessor.askForAnalysis();
            };
            break;
        }
        case State::Complete: {
            analysisButton.setButtonText("Calculate Analysis");
            analysisButton.onClick = [this]{
                TSNaudioProcessor.askForAnalysis();
            };
            break;
        }
        case State::Analyzing: {
            analysisButton.setButtonText("Stop Analysis");
            analysisButton.onClick = [this] {
                TSNaudioProcessor.stopAnalysis();
            };
            break;
        }
    }
}
void TsnGranularAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster *source) {
    if (const auto *a = &TSNaudioProcessor.getAnalyzer();
        source == a)
    {
        setAnalysisButtonFunctionality();
    }

    GranularEditorCommon::changeListenerCallback(source);
}

//==============================================================================
void TsnGranularAudioProcessorEditor::drawBackground()
{
	auto const w = static_cast<float>(getWidth());
	auto const h = static_cast<float>(getHeight());

	// rebuild our off‑screen image
	backgroundImage = juce::Image (juce::Image::ARGB, static_cast<int>(w), static_cast<int>(h), true);
	juce::Graphics tg (backgroundImage);

	// start with solid black
	tg.fillAll (juce::Colours::black);

	auto& r = juce::Random::getSystemRandom();

	const int numLayers = static_cast<int>(r.nextFloat() * 15) + 1;
	jassert(numLayers > 0);
	for (int i = 0; i < numLayers; ++i)
	{
		const float x1 = r.nextFloat() * w;
		const float y1 = r.nextFloat() * h;
		const float x2 = r.nextFloat() * w;
		const float y2 = r.nextFloat() * h;
		
		// two random shades
		const auto c1 = juce::Colour::greyLevel( r.nextFloat() );
		const auto c2 = juce::Colour::greyLevel( r.nextFloat() );

		juce::ColourGradient grad (c1, x1, y1,
								  c2, x2, y2,
								  false);
		
		for (double g = r.nextFloat() * 0.01; g < 1.0; g += r.nextFloat() * 0.03 + 0.02){
			grad.addColour (g, c1.interpolatedWith (c2, r.nextFloat()));
		}
		const float opacity = 0.1f + r.nextFloat() * 0.2f;
		tg.setGradientFill (grad);
		tg.setOpacity       (opacity);
		tg.fillRect         (0.0f, 0.0f, w, h);
	}

//	optional radial vignette to darken the very edges
	if constexpr (false)
	{
		juce::Colour inner = juce::Colour::fromHSV (r.nextFloat(), 1.0f, 1.0f, 0.07f);
		juce::Colour outer = juce::Colours::black;

		auto cx = w * 0.5f;
		auto cy = h * 0.5f;
		juce::ColourGradient rad (inner, cx, cy,
								 outer, cx + w*0.7f, cy + h*0.7f,
								 true);

		tg.setOpacity (0.2f);
		tg.setGradientFill (rad);
		tg.fillEllipse (-w*0.3f, -h*0.3f, w*1.6f, h*1.6f);
	}
}
void TsnGranularAudioProcessorEditor::paint (juce::Graphics& g)
{
	if ( backgroundNeedsUpdate
	  || backgroundImage.getWidth()  != getWidth()
	  || backgroundImage.getHeight() != getHeight() )
	{
		drawBackground();
		backgroundNeedsUpdate = false;
	}

	// draw cached image
	g.drawImage (backgroundImage, getLocalBounds().toFloat());
	displayName (g, getLocalBounds());
}

void TsnGranularAudioProcessorEditor::resized()
{
	backgroundNeedsUpdate = true;
	getConstrainer()->checkComponentBounds(this);
	juce::Rectangle<int> localBounds = getLocalBounds();
	int const smallPad = 12;
	localBounds.reduce(smallPad, smallPad);
	
	int x(localBounds.getX()), y(0);
	y = placeFileCompAndGrainBusyDisplay(localBounds, 2, grainBusyDisplay, presetPanel, y);

	{
		int buttonWidth = 90;
		const int buttonHeight = 25;
		analysisButton.setBounds(x, y, buttonWidth, buttonHeight);
		x += buttonWidth;
		writeWavsButton.setBounds(x, y, buttonWidth, buttonHeight);
		x += buttonWidth;
		settingsButton.setBounds(x, y, buttonWidth, buttonHeight);
		x += buttonWidth;
		buttonWidth = buttonHeight;
		x += buttonWidth;
		y += buttonHeight;
//		y += smallPad;
	}
    constexpr auto mainParamsRemainingHeightRatio  = 0.37f;
    constexpr auto waveformCompRemainingHeightRatio = 0.12f;
    constexpr auto timbreSpaceRemainingHeightRatio = 0.51f;
    constexpr auto totalRemainingHeightRatiosSummed = mainParamsRemainingHeightRatio + waveformCompRemainingHeightRatio + timbreSpaceRemainingHeightRatio;
	jassert(0.999f <= totalRemainingHeightRatiosSummed && (totalRemainingHeightRatiosSummed <= 1.001f));
	
	{
		auto const mainParamsRemainingHeight = mainParamsRemainingHeightRatio * localBounds.toFloat().getHeight();

		int const allottedMainParamsHeight = static_cast<int>(mainParamsRemainingHeight) - y + smallPad;
		
		int const allottedMainParamsWidth = localBounds.getWidth();
		tabbedPages.setBounds(localBounds.getX(), y, allottedMainParamsWidth, allottedMainParamsHeight);
		y += tabbedPages.getHeight();
	}
	{
	    jassert (waveformComponent != nullptr);

		auto const waveformComponentHeight = waveformCompRemainingHeightRatio * localBounds.toFloat().getHeight();
		waveformComponent->setBounds(localBounds.getX(), y, localBounds.getWidth(), static_cast<int>(waveformComponentHeight));
		y += waveformComponent->getHeight();
	}
	{
		auto const timbreSpaceComponentHeight = timbreSpaceRemainingHeightRatio * localBounds.toFloat().getHeight();
		auto const timbreSpaceComponentWidth = localBounds.getWidth();
		timbreSpaceComponent.setBounds(localBounds.getX(), y, timbreSpaceComponentWidth, static_cast<int>(timbreSpaceComponentHeight));
		y += timbreSpaceComponent.getHeight();
	}
}

