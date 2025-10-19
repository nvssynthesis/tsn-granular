/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "Gui/SettingsWindow.h"
#include "../slicer_granular/Source/algo_util.h"
#include "Gui/NavigatorPage.h"
#include "Analysis/RunLoopStatus.h"

//==============================================================================

TsnGranularAudioProcessorEditor::TsnGranularAudioProcessorEditor (TSNGranularAudioProcessor& p)
:	AudioProcessorEditor (&p)
,	GranularEditorCommon (p)
,	timbreSpaceComponent(p)
,	backgroundNeedsUpdate(true)
,	askForAnalysisButton("Calculate Analysis")
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	audioProcessor(p)
{
	setSize (680, 950);
	setResizable(true, true);
	
	addAndMakeVisible(grainBusyDisplay);
	addAndMakeVisible(presetPanel);
	addAndMakeVisible(askForAnalysisButton);
	askForAnalysisButton.onClick = [this]{
		if (TSNGranularAudioProcessor* a = dynamic_cast<TSNGranularAudioProcessor*>(&processor)){
			audioProcessor.writeToLog("success dynamic casting");
			a->askForAnalysis();
		}
	};
	
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
	addAndMakeVisible(waveformAndPositionComponent);
	waveformAndPositionComponent.hideSlider();

	// audioProcessor.getTsnGranularSynthesizer()->addChangeListener(&(waveformAndPositionComponent.wc));	// to highlight current event

	addAndMakeVisible(timbreSpaceComponent);
	timbreSpaceComponent.addMouseListener(this, false);
	
#pragma message("need to fix this part based on the new changes. We don't want to do unecessary point calculations on construction, but do want to draw already-stored point data.")
	paintOnsetMarkers();
	
	auto &a = audioProcessor.getAnalyzer();
	a.getStatus().addChangeListener(&timbreSpaceComponent);	// to tell timbre space comp to make progress bar visible
	a.addListener(&timbreSpaceComponent);		// tell timbre space comp to hide progress bar if thread exits early
	a.addChangeListener(&timbreSpaceComponent); // tell timbre space comp to hide progress bar when analysis successfully completes
	
	auto &ts = audioProcessor.getTimbreSpace();
	ts.addActionListener(this); // tell me to paint onsets
	for (auto *child : getChildren()){
		child->setLookAndFeel(&laf);
	}

    startTimerHz(30);

	getConstrainer()->setMinimumSize(240, 360);
}

TsnGranularAudioProcessorEditor::~TsnGranularAudioProcessorEditor()
{
    stopTimer();

	for (auto *child : getChildren()){
		child->setLookAndFeel(nullptr);
	}
	closeAllWindows();
	
	auto &a = audioProcessor.getAnalyzer();
	a.getStatus().removeChangeListener(&timbreSpaceComponent);
	a.removeListener(&timbreSpaceComponent);
	a.removeChangeListener(&timbreSpaceComponent);
	a.removeChangeListener(this);
	
	// audioProcessor.getTsnGranularSynthesizer()->removeChangeListener(&(waveformAndPositionComponent.wc));
	audioProcessor.getTimbreSpace().removeActionListener(this);
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
void TsnGranularAudioProcessorEditor::popupSettings(bool native){
	int const intensity = 70;
	auto* settingsWindow = new SettingsWindow (audioProcessor, juce::Colour(intensity, intensity, intensity));
	windows.add (settingsWindow);

	juce::Rectangle<int> area (0, 0, 500, 400);

	juce::RectanglePlacement placement (juce::RectanglePlacement::xMid
										| juce::RectanglePlacement::yTop
										| juce::RectanglePlacement::doNotResize);

	auto result = placement.appliedTo (area, juce::Desktop::getInstance().getDisplays()
												 .getPrimaryDisplay()->userArea.reduced (20));
	settingsWindow->setBounds (result);

	settingsWindow->setResizable (true, ! native);
	settingsWindow->setUsingNativeTitleBar (native);
	settingsWindow->setVisible (true);
}

void TsnGranularAudioProcessorEditor::paintOnsetMarkers()
{
    const auto onsetsOpt = audioProcessor.getTimbreSpace().getOnsets();
	if (!onsetsOpt.has_value()){
		audioProcessor.writeToLog("TsnGranularAudioProcessorEditor::paintOnsetMarkers : Onsets had no value; returning\n");
		return;
	}
	
	auto &wc = waveformAndPositionComponent.wc;

	audioProcessor.writeToLog("Editor: Drawing waveform markers\n");

	wc.removeMarkers(WaveformComponent::MarkerType::Onset);
	for (auto onset : onsetsOpt.value()) {
		wc.addMarker(onset);
	}
	wc.repaint();
}

void TsnGranularAudioProcessorEditor::mouseDown(const juce::MouseEvent &) {}
void TsnGranularAudioProcessorEditor::mouseDrag(const juce::MouseEvent &) {}
//==============================================================================
void TsnGranularAudioProcessorEditor::timerCallback() {
    timbreSpaceComponent.repaint();
    waveformAndPositionComponent.wc.highlightOnsets(audioProcessor.getTsnGranularSynthesizer()->getTimbreSpace().getCurrentPointIndices());
}

//==============================================================================
void TsnGranularAudioProcessorEditor::drawBackground()
{
	float const w = (float)getWidth();
	float const h = (float)getHeight();

	// rebuild our off‑screen image
	backgroundImage = juce::Image (juce::Image::ARGB, (int)w, (int)h, true);
	juce::Graphics tg (backgroundImage);

	// start with solid black
	tg.fillAll (juce::Colours::black);

	auto& r = juce::Random::getSystemRandom();

	const int numLayers = int(r.nextFloat() * 15) + 1;
	jassert(numLayers > 0);
	for (int i = 0; i < numLayers; ++i)
	{
		float x1 = r.nextFloat() * w;
		float y1 = r.nextFloat() * h;
		float x2 = r.nextFloat() * w;
		float y2 = r.nextFloat() * h;
		
		// two random shades
		auto c1 = juce::Colour::greyLevel( r.nextFloat() );
		auto c2 = juce::Colour::greyLevel( r.nextFloat() );

		juce::ColourGradient grad (c1, x1, y1,
								  c2, x2, y2,
								  false);
		
		for (double g = r.nextFloat() * 0.01; g < 1.0; g += r.nextFloat() * 0.03 + 0.02){
			grad.addColour (g, c1.interpolatedWith (c2, r.nextFloat()));
		}
		float opacity = 0.1f + r.nextFloat() * 0.2f;
		tg.setGradientFill (grad);
		tg.setOpacity       (opacity);
		tg.fillRect         (0.0f, 0.0f, w, h);
	}

//	optional radial vignette to darken the very edges
	if (false)
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
	y = placeFileCompAndGrainBusyDisplay(localBounds, smallPad, grainBusyDisplay, presetPanel, y);

	{
		int buttonWidth = 90;
		int buttonHeight = 25;
		askForAnalysisButton.setBounds(x, y, buttonWidth, buttonHeight);
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
	auto const mainParamsRemainingHeightRatio  = 0.37f;
	auto const waveformCompRemainingHeightRatio = 0.12f;
	auto const timbreSpaceRemainingHeightRatio = 0.51f;
	auto const totalRemainingHeightRatiosSummed = mainParamsRemainingHeightRatio + waveformCompRemainingHeightRatio + timbreSpaceRemainingHeightRatio;
	jassert((0.999f <= totalRemainingHeightRatiosSummed) && (totalRemainingHeightRatiosSummed <= 1.001f));
	
	{
		auto const mainParamsRemainingHeight = mainParamsRemainingHeightRatio * localBounds.getHeight();

		int const alottedMainParamsHeight = mainParamsRemainingHeight - y + smallPad;
		
		int const alottedMainParamsWidth = localBounds.getWidth();
		tabbedPages.setBounds(localBounds.getX(), y, alottedMainParamsWidth, alottedMainParamsHeight);
		y += tabbedPages.getHeight();
	}
	{
		auto const waveformComponentHeight = waveformCompRemainingHeightRatio * localBounds.getHeight();
		waveformAndPositionComponent.setBounds(localBounds.getX(), y, localBounds.getWidth(), waveformComponentHeight);
		y += waveformAndPositionComponent.getHeight();
	}
	{
		auto const timbreSpaceComponentHeight = timbreSpaceRemainingHeightRatio * localBounds.getHeight();
		auto const timbreSpaceComponentWidth = localBounds.getWidth();
		timbreSpaceComponent.setBounds(localBounds.getX(), y, timbreSpaceComponentWidth, timbreSpaceComponentHeight);
		y += timbreSpaceComponent.getHeight();
	}
}

void TsnGranularAudioProcessorEditor::actionListenerCallback(juce::String const &message) {
	if (message.compare("reportAvailability") == 0){
		paintOnsetMarkers();
	}
}
