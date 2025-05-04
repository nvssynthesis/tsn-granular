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
,	backgroundNeedsUpdate(true)
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
	
	// exclusive to TSN
	tabbedPages.addTab ("Navigation LFO", juce::Colours::transparentWhite, new NavLFOPage(audioProcessor.getAPVTS()), true);

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
void drawWaveformMarkers(WaveformComponent &wc, std::vector<float> const &normalizedOnsets, TsnGranularAudioProcessor &p, bool verbose = true){
	if (verbose){
		p.writeToLog("Editor: Drawing waveform markers\n");
	}
	for (auto onset : normalizedOnsets) {
		wc.addMarker(onset);
	}
}

std::vector<float> getHistoEqualizationVec(std::vector<float> const &points){
	std::vector<float> allX, vecOut;
	allX.reserve (points.size());
	vecOut.reserve (points.size());
	for (auto& p : points){
		allX.push_back (p);
	}
	std::sort (allX.begin(), allX.end());
	
	
	for (auto const& p : points)
	{
		// find first index ≥ p.x
		auto idx = (int)(std::lower_bound (allX.begin(),
										   allX.end(),
										   p)
						 - allX.begin());
		float quantile = (float) idx / (float)(allX.size() - 1);
		
		// optional: you can still gamma‑tweak the quantile
		float gamma    = 0.8f;             // <1 stretches mid‑values
		float t        = std::pow (quantile, gamma);
		vecOut.push_back(t);
	}
	return vecOut;
}

void TsnGranularAudioProcessorEditor::updateAndDrawTimbreSpacePoints(std::vector<std::vector<float>> const &timbreSpaceRepresentation, bool verbose)
{	/** to be called when the actual analyzed timbre space changes  */
	if (verbose){
		audioProcessor.writeToLog("Editor: Drawing timbre space points\n");
	}
	
	{
		timbreSpaceNeededData.ranges.clear();
		timbreSpaceNeededData.ranges.reserve(timbreSpaceRepresentation.size());
		for (int i = 0; i < timbreSpaceRepresentation.size(); ++i) {
			timbreSpaceNeededData.ranges[i] = nvs::analysis::calculateRangeOfDimension(timbreSpaceRepresentation, i);
		}
	}
	{
		std::vector<float> allDim0, allDim1;
		allDim0.reserve(timbreSpaceRepresentation.size());
		allDim1.reserve(timbreSpaceRepresentation.size());
		for (auto const& frame : timbreSpaceRepresentation){
			allDim0.push_back(frame[0]);	// e.g. bfcc0
			allDim1.push_back(frame[1]);	// e.g. bfcc1
		}
		timbreSpaceNeededData.histoEqualizedD0 = getHistoEqualizationVec(allDim0);
		timbreSpaceNeededData.histoEqualizedD1 = getHistoEqualizationVec(allDim1);
	}
	timbreSpaceNeededData.timbreSpaceRepresentation = &timbreSpaceRepresentation;
	
	drawTimbreSpacePoints(verbose);
}
void TsnGranularAudioProcessorEditor::drawTimbreSpacePoints(bool verbose)
{	/** to be called when we only want to change the view of the timbre points (which will also need to happen when the timbre space itself changes) */
	if (timbreSpaceNeededData.timbreSpaceRepresentation == nullptr){
		audioProcessor.writeToLog("drawTimbreSpacePoints: timbreSpaceNeededData null, returning...");
		return;
	}
	
	auto normalizer = [](float x, std::pair<float, float> range) -> float
	{
		auto y01 = (x - range.first) / (range.second - range.first);
		return juce::jmap(y01, -1.f, 1.f);
	};
	auto squash = [](float xNorm) -> float { return std::asinh(10.0*xNorm) / (float)(M_PI); };
	auto foo = [&](float x, std::pair<float, float> range) -> float { return squash(normalizer(x, range)); };
	
	constexpr size_t nDim {5};
	std::array<size_t, nDim> constexpr dimensions {
		0,
		1,
		2,
		3,
		4
	};
	
	std::vector<std::vector<float>> const &timbreSpaceRepr = *(timbreSpaceNeededData.timbreSpaceRepresentation);
	for (int i = 0; i < timbreSpaceRepr.size(); ++i) {
		std::vector<float> const &timbreFrame = timbreSpaceRepr[i];
		
		assert (timbreFrame.size() >= nDim);
		

		auto pNL = juce::Point<float>(foo(timbreFrame[dimensions[0]], timbreSpaceNeededData.ranges[0]),
									   foo(timbreFrame[dimensions[1]], timbreSpaceNeededData.ranges[1]));
		// histogram equalization
		float const &equalizedX  = timbreSpaceNeededData.histoEqualizedD0[i];
		float const &equalizedY  = timbreSpaceNeededData.histoEqualizedD1[i];
		
		auto pHE = juce::Point<float>( juce::jmap(equalizedX, -1.f, 1.f),
								juce::jmap(equalizedY, -1.f, 1.f));
	
		float c = 0.5f;
		juce::Point<float> p = c * pNL + (1.f - c) * pHE;
		
		std::array<float, 3> const color {
			( normalizer(timbreFrame[dimensions[2]], timbreSpaceNeededData.ranges[2]) ),
			( normalizer(timbreFrame[dimensions[3]], timbreSpaceNeededData.ranges[3]) ),
			( normalizer(timbreFrame[dimensions[4]], timbreSpaceNeededData.ranges[4]) )
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsets.
		float const padding_scalar = 0.95f;
		timbreSpaceComponent.add5DPoint(p * padding_scalar, color);
		if (verbose){
			fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
		}
	}
}
void TsnGranularAudioProcessorEditor::paintOnsetMarkersAndTimbrePoints(std::vector<float> const &normalizedOnsets,
	std::vector<nvs::analysis::FeatureContainer<nvs::analysis::EventwiseStatistics<float>>> const &timbreSpaceRepresention)
{
	waveformAndPositionComponent.wc.removeMarkers(WaveformComponent::MarkerType::Onset);
	timbreSpaceComponent.clear(); // clearing to make way for points we're about to be adding
	drawWaveformMarkers(waveformAndPositionComponent.wc, normalizedOnsets, audioProcessor);
	
	std::vector<std::vector<float>> eventwiseTimbrePoints;
	eventwiseTimbrePoints.reserve(timbreSpaceRepresention.size());
	
	
	auto featureSet = nvs::analysis::bfccSet;	// may modify this, but it's a good starting point
	for (auto const &t : timbreSpaceRepresention) {
		std::vector<float> v = nvs::analysis::extractFeatures(t, featureSet, nvs::analysis::Statistic::Median);
		eventwiseTimbrePoints.push_back(v);
	}
	updateAndDrawTimbreSpacePoints(eventwiseTimbrePoints);
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

	const int numLayers = 11;
	for (int i = 0; i < numLayers; ++i)
	{
		float x1 = r.nextFloat() * w;
		float y1 = r.nextFloat() * h;
		float x2 = r.nextFloat() * w;
		float y2 = r.nextFloat() * h;
		
		// two random vivid colours
		auto c1 = juce::Colour::greyLevel( r.nextFloat() );
		auto c2 = juce::Colour::greyLevel( r.nextFloat() );

		juce::ColourGradient grad (c1, x1, y1,
								  c2, x2, y2,
								  false);
		
		for (double g = r.nextFloat() * 0.05; g < 1.0; g += r.nextFloat() * 0.09 + 0.03){
			grad.addColour (g, c1.interpolatedWith (c2, r.nextFloat()));
		}
		// draw it at the correct opacity *and* over the full image
		float opacity = 0.1f + r.nextFloat() * 0.2f;  // 0.1–0.3
		tg.setGradientFill (grad);
		tg.setOpacity       (opacity);
		tg.fillRect         (0.0f, 0.0f, w, h);    // ← critical fix!
	}

//	 optional radial vignette to darken the very edges
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
	constrainer.checkComponentBounds(this);
	juce::Rectangle<int> localBounds = getLocalBounds();
//
//	juce::Image const toBeBackground(juce::Image::PixelFormat::RGB,
//								   localBounds.getWidth(),
//								   localBounds.getHeight(),
//								   false);
//	backgroundImage = toBeBackground;
	
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
