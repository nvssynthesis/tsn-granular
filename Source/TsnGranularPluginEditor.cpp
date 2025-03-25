/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "Gui/SettingsWindow.h"
#include "../slicer_granular/Source/algo_util.h"
#include "Gui/NavLFOPage.h"

//==============================================================================

TsnGranularAudioProcessorEditor::TsnGranularAudioProcessorEditor (TsnGranularAudioProcessor& p)
:	AudioProcessorEditor (&p)
,	GranularEditorCommon (p)
,	gui_lfo(p.getGUILFO())	// processor owns it, but editor facilitates communication from lfo > timbre space
,	timbreSpaceComponent(p.getAPVTS())
,	askForAnalysisButton("Calculate Analysis")
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	positionQuantizeStrengthComboBox("Position Quantize Strength")
,	audioProcessor(p)
{
	addAndMakeVisible(grainBusyDisplay);
	addAndMakeVisible (fileComp);
	addAndMakeVisible(askForAnalysisButton);
	askForAnalysisButton.onClick = [this]{
		if (TsnGranularAudioProcessor* a = dynamic_cast<TsnGranularAudioProcessor*>(&processor)){
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
	
	addAndMakeVisible(positionQuantizeStrengthComboBox);
	positionQuantizeStrengthComboBox.addItem("None", 1);
	positionQuantizeStrengthComboBox.addItem("Lax", 2);
	positionQuantizeStrengthComboBox.addItem("Strict", 3);
	positionQuantizeStrengthComboBox.setSelectedId(1);
	positionQuantizeStrengthComboBox.onChange = [this] {
		auto id = positionQuantizeStrengthComboBox.getSelectedId();
		fmt::print("current id: {}", id);
	};
	
	tabbedPages.addTab ("Navigation LFO Parameters", juce::Colours::transparentWhite, new NavLFOPage(audioProcessor.getAPVTS()), true);

	addAndMakeVisible(tabbedPages);
	addAndMakeVisible(waveformAndPositionComponent);
	waveformAndPositionComponent.hideSlider();
	
	gui_lfo.setOnUpdateCallback([&](double x, double y){
		// set navigator of timbre space component
		auto const p2 = juce::Point<float>(x, y);
		timbreSpaceComponent.setNavigatorPoint(p2);
		// also attract to nearest point
		auto p5 = TimbreSpaceComponent::timbre5DPoint {
			._p2D{p2},
			._p3D{0.f, 0.f, 0.f}
		};
		timbreSpaceComponent.setCurrentPointFromNearest(p5);
		setReadBoundsFromChosenPoint();
		timbreSpaceComponent.repaint();
	});
	gui_lfo.start();
	addAndMakeVisible(timbreSpaceComponent);
	timbreSpaceComponent.addMouseListener(this, false);
	{	// on initial construction, this should do nothing. however, it should also take care of the case where you close and open the plugin window.
		auto const &a = audioProcessor.getAnalyzer();
		paintOnsetMarkersAndTimbrePoints(a.getOnsets(), a.getTimbreSpaceRepresentation());	// is this really the best place for it though? think about in resized() maybe.
	}
	
	constrainer.setMinimumSize(620, 500);
	setSize (680, 950);
	setResizable(true, true);
}

TsnGranularAudioProcessorEditor::~TsnGranularAudioProcessorEditor()
{
	closeAllWindows();
	gui_lfo.stop();
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
	auto* settingsWindow = new SettingsWindow (audioProcessor, *this, juce::Colour(intensity, intensity, intensity));
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
void drawWaveformMarkers(WaveformComponent &wc, std::vector<float> const &normalizedOnsets, TsnGranularAudioProcessor &p, bool verbose = true){
	if (verbose){
		p.writeToLog("Editor: Drawing waveform markers\n");
	}
	double const sr = p.getAnalyzer().getAnalysisSettings().sampleRate;
	for (auto onset : normalizedOnsets) {
		wc.addMarker(onset);
	}
}
void drawTimbreSpacePoints(TimbreSpaceComponent &timbreSpaceComponent, std::vector<std::vector<float>> const &timbreSpaceRepresentation, Slicer_granularAudioProcessor &p, bool verbose = true){
	if (verbose){
		p.writeToLog("Editor: Drawing timbre space points\n");
	}
	size_t constexpr nDim {5};
	std::array<size_t, nDim> constexpr dimensions {
		0,
		1,
		2,
		3,
		4
	};
	std::array<float, nDim> normalizers;
	for (int i = 0; i < nDim; ++i) {
	auto const range = nvs::analysis::calculateRangeOfDimension(timbreSpaceRepresentation, dimensions[i]);
		normalizers[i] = nvs::analysis::calculateNormalizationMultiplier(range);
	}
	for (std::vector<float> const &timbreFrame : timbreSpaceRepresentation) {
		assert (timbreFrame.size() >= nDim);
		
		// just some bull as a placeholder for actual timbral analysis
		#pragma message("need proper normalization")
		juce::Point<float> const p(timbreFrame[dimensions[0]] * normalizers[0],
								   timbreFrame[dimensions[1]] * normalizers[1]);
		std::array<float, 3> const color {
			( timbreFrame[dimensions[2]] * normalizers[2] ),
			( timbreFrame[dimensions[3]] * normalizers[3] ),
			( timbreFrame[dimensions[4]] * normalizers[4] )
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsets.
		float const padding_scalar = 0.95f;
		timbreSpaceComponent.add5DPoint(p * padding_scalar, color);
		fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
	}
}
void TsnGranularAudioProcessorEditor::paintOnsetMarkersAndTimbrePoints(std::vector<float> const &normalizedOnsets,
													 std::vector<std::vector<float>> const &timbreSpaceRepresention) {
	waveformAndPositionComponent.wc.removeMarkers(WaveformComponent::MarkerType::Onset);
	timbreSpaceComponent.clear(); // clearing to make way for points we're about to be adding
	drawWaveformMarkers(waveformAndPositionComponent.wc, normalizedOnsets, audioProcessor);
	drawTimbreSpacePoints(timbreSpaceComponent, timbreSpaceRepresention, GranularEditorCommon::audioProcessor);
	repaint();
}

void TsnGranularAudioProcessorEditor::setReadBoundsFromChosenPoint() {
	auto const pIdx = timbreSpaceComponent.getCurrentPointIdx();
	if (audioProcessor.getAnalyzer().getOnsets().size()){
		audioProcessor.setWaveEvent(pIdx);
	}
}

void TsnGranularAudioProcessorEditor::mouseDown(const juce::MouseEvent &event) {
	setReadBoundsFromChosenPoint();
}
void TsnGranularAudioProcessorEditor::mouseDrag(const juce::MouseEvent &event) {
	setReadBoundsFromChosenPoint();
}
//==============================================================================
void TsnGranularAudioProcessorEditor::paint (juce::Graphics& g)
{
	if (true){
		juce::Graphics tg(backgroundImage);
		
		juce::Colour upperLeftColour  = gradientColors[(colourOffsetIndex + 0) % gradientColors.size()];
		upperLeftColour = upperLeftColour.interpolatedWith(juce::Colours::darkred, 0.7f);
		juce::Colour lowerRightColour = gradientColors[(colourOffsetIndex + gradientColors.size()-1) % gradientColors.size()];
		lowerRightColour = lowerRightColour.interpolatedWith(juce::Colours::darkred, 0.7f);
		juce::ColourGradient cg(upperLeftColour, 0, 0, lowerRightColour, getWidth(), getHeight(), true);
		cg.addColour(0.5, gradientColors[(colourOffsetIndex + 1) % gradientColors.size()]);
		tg.setGradientFill(cg);
		tg.fillAll();
	}

	auto const b = getLocalBounds();
	g.drawImage(backgroundImage, b.toFloat());
	displayName(g, b);
}

void TsnGranularAudioProcessorEditor::resized()
{
	constrainer.checkComponentBounds(this);
	juce::Rectangle<int> localBounds = getLocalBounds();
	
	juce::Image const toBeBackground(juce::Image::PixelFormat::RGB,
								   localBounds.getWidth(),
								   localBounds.getHeight(),
								   false);
	backgroundImage = toBeBackground;
	
	int const smallPad = 12;
	localBounds.reduce(smallPad, smallPad);
	
	int x(0), y(0);
	y = placeFileCompAndGrainBusyDisplay(localBounds, smallPad, grainBusyDisplay, fileComp, y);

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
		positionQuantizeStrengthComboBox.setBounds(x, y, 200, buttonHeight);
		y += buttonHeight;
//		y += smallPad;
	}
	auto const mainParamsRemainingHeightRatio  = 0.37f;
	auto const waveformCompRemainingHeightRatio = 0.12f;
	auto const timbreSpaceRemainingHeightRatio = 0.51f;
	auto const totalRemainingHeightRatiosSummed = (mainParamsRemainingHeightRatio + waveformCompRemainingHeightRatio + timbreSpaceRemainingHeightRatio) ;
	assert(totalRemainingHeightRatiosSummed >= 0.999f);
	assert(totalRemainingHeightRatiosSummed <= 1.001f);
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

void TsnGranularAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		// now we can simply check on our own analyzer, don't even need to use source qua source
		paintOnsetMarkersAndTimbrePoints(a->getOnsets(), a->getTimbreSpaceRepresentation());
		audioProcessor.writeToLog("editor: change listener callback: got timbre data to paint\n");
	}
	else {
		GranularEditorCommon::changeListenerCallback(source);
	}
}
