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
#include "Analysis/RunLoopStatus.h"

//==============================================================================

TsnGranularAudioProcessorEditor::TsnGranularAudioProcessorEditor (TsnGranularAudioProcessor& p)
:	AudioProcessorEditor (&p)
,	GranularEditorCommon (p)
,	timbreSpaceComponent(p.getAPVTS(), p.getTimbreSpaceHolder())
,	backgroundNeedsUpdate(true)
,	askForAnalysisButton("Calculate Analysis")
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	positionQuantizeStrengthComboBox("Position Quantize Strength")
,	audioProcessor(p)
{
	setSize (680, 950);
	setResizable(true, true);
	
	addAndMakeVisible(grainBusyDisplay);
	addAndMakeVisible(fileComp);
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
	auto onUpdateFn = [&](const std::vector<double>& v){
	// set navigator of timbre space component
		auto const p2 = juce::Point<float>(v[0], v[1]);
		// also attract to nearest point
		auto p5 = nvs::util::Timbre5DPoint {	// needs to handle arbitrary dimensions and just attract based on provided dims
			._p2D{p2},
			._p3D{0.f, 0.f, 0.f}
		};
		auto &apvts = audioProcessor.getAPVTS();
		auto paramName = getParamName(params_e::nav_selection_sharpness);
		
		double const sharpness = (double) *(apvts.getRawParameterValue(paramName));
		
		audioProcessor.getTimbreSpaceHolder().setProbabilisticPointFromTarget(p5, 4, sharpness);
		audioProcessor.setReadBoundsFromChosenPoint();	// needs to affect processor but has final effect on gui
		
//		assert (2 <= v.size());
//		timbreSpaceComponent.setNavigatorPoint(p2);
//		timbreSpaceComponent.repaint();
	};
	auto &nav = audioProcessor.getNavigator();
	nav.addChangeListener(&timbreSpaceComponent);
	tabbedPages.addTab ("Navigation LFO", juce::Colours::transparentWhite, new NavigatorPage(audioProcessor.getAPVTS(), nav), true);

	
	addAndMakeVisible(tabbedPages);
	addAndMakeVisible(waveformAndPositionComponent);
	waveformAndPositionComponent.hideSlider();
	audioProcessor.getTsnGranularSynthesizer()->addChangeListener(&(waveformAndPositionComponent.wc));	// to highlight current event
	
#pragma message("set navigator PANEL to have this update function!!!!!")
	// set navigator PANEL to have this update function!!!!!
	
	addAndMakeVisible(timbreSpaceComponent);
	timbreSpaceComponent.addMouseListener(this, false);
	
#pragma message("need to fix this part based on the new changes. We don't want to do unecessary point calculations on construction, but do want to draw already-stored point data.")
	paintOnsetMarkers();
	
	auto &a = audioProcessor.getAnalyzer();
	a.getStatus().addChangeListener(&timbreSpaceComponent);	// to tell timbre space comp to make progress bar visible
	a.addListener(&timbreSpaceComponent);		// tell timbre space comp to hide progress bar if thread exits early
	a.addChangeListener(this);					// tell me to paint onsets
	a.addChangeListener(&timbreSpaceComponent); // tell timbre space comp to hide progress bar when analysis successfully completes
	
	audioProcessor.getTimbreSpaceNeededData().addChangeListener(this);
	
	for (auto *child : getChildren()){
		child->setLookAndFeel(&laf);
	}
	
	getConstrainer()->setMinimumSize(620, 500);
}

TsnGranularAudioProcessorEditor::~TsnGranularAudioProcessorEditor()
{
	for (auto *child : getChildren()){
		child->setLookAndFeel(nullptr);
	}
	closeAllWindows();
	
	auto &a = audioProcessor.getAnalyzer();
	a.getStatus().removeChangeListener(&timbreSpaceComponent);
	a.removeListener(&timbreSpaceComponent);
	a.removeChangeListener(&timbreSpaceComponent);
	a.removeChangeListener(this);
	
	audioProcessor.getTsnGranularSynthesizer()->removeChangeListener(&(waveformAndPositionComponent.wc));
	audioProcessor.getNavigator().removeChangeListener(&timbreSpaceComponent);
	
	audioProcessor.getTimbreSpaceNeededData().removeChangeListener(this);
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

void TsnGranularAudioProcessorEditor::drawTimbreSpacePoints(bool verbose)
{	/** to be called when we only want to change the view of the timbre points (which will also need to happen when the timbre space itself changes) */
	auto &timbreSpaceNeededData = audioProcessor.getTimbreSpaceNeededData();
	if (timbreSpaceNeededData.eventwiseExtractedTimbrePoints.size() == 0){
		audioProcessor.writeToLog("drawTimbreSpacePoints: timbreSpaceNeededData empty, returning...");
		return;
	}
	std::vector<std::vector<float>> const &timbreSpaceRepr = timbreSpaceNeededData.eventwiseExtractedTimbrePoints;
	std::vector<std::pair<float, float>> const &ranges = timbreSpaceNeededData.ranges;
	if (!(timbreSpaceRepr.size() == ranges.size())){
		audioProcessor.writeToLog("drawTimbreSpacePoints: point size mismatch, exiting early");
	}

	if (verbose){ audioProcessor.writeToLog("Editor: Drawing timbre space points\n"); }
	
	auto normalizer = [](float x, std::pair<float, float> range) -> float
	{
		auto r = (range.second - range.first);
		auto y01 = (x - range.first);
		if (r != 0){
			y01 /= r;
		}
		return juce::jmap(y01, -1.f, 1.f);
	};
	auto squash = [](float xNorm) -> float { return std::asinh(10.0*xNorm) / (float)(M_PI); };
	auto foo = [&](float x, std::pair<float, float> range) -> float { return squash(normalizer(x, range)); };
	
	constexpr size_t nDim {5};
	
	audioProcessor.getTimbreSpaceHolder().clear(); // clearing to make way for points we're about to be adding
	for (size_t i = 0; i < timbreSpaceRepr.size(); ++i) {
		std::vector<float> const &timbreFrame = timbreSpaceRepr[i];
		
		assert (timbreFrame.size() >= nDim);

		auto pNL = juce::Point<float>(foo(timbreFrame[0], timbreSpaceNeededData.ranges[0]),
									   foo(timbreFrame[1], timbreSpaceNeededData.ranges[1]));
		// histogram equalization
		float const &equalizedX  = timbreSpaceNeededData.histoEqualizedD0[i];
		float const &equalizedY  = timbreSpaceNeededData.histoEqualizedD1[i];
		
		auto pHE = juce::Point<float>( juce::jmap(equalizedX, -1.f, 1.f),
								juce::jmap(equalizedY, -1.f, 1.f));
	
		float c = timbreSpaceNeededData.spacialSettings.histogramEqualization;
		jassert (0.0 <= c && c <= 1.0);
		juce::Point<float> p = (1.f - c) * pNL + c * pHE;
		
		std::array<float, 3> const color {
			( normalizer(timbreFrame[2], timbreSpaceNeededData.ranges[2]) ),
			( normalizer(timbreFrame[3], timbreSpaceNeededData.ranges[3]) ),
			( normalizer(timbreFrame[4], timbreSpaceNeededData.ranges[4]) )
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsets.
		float const padding_scalar = 0.95f;
		audioProcessor.getTimbreSpaceHolder().add5DPoint(p * padding_scalar, color);
		if (verbose){
			fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
		}
	}
}

void TsnGranularAudioProcessorEditor::paintOnsetMarkers()
{
	auto onsetsOpt = audioProcessor.getAnalyzer().getOnsets();
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

void TsnGranularAudioProcessorEditor::mouseDown(const juce::MouseEvent &event) {}
void TsnGranularAudioProcessorEditor::mouseDrag(const juce::MouseEvent &event) {}
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
	getConstrainer()->checkComponentBounds(this);
	juce::Rectangle<int> localBounds = getLocalBounds();
//
//	juce::Image const toBeBackground(juce::Image::PixelFormat::RGB,
//								   localBounds.getWidth(),
//								   localBounds.getHeight(),
//								   false);
//	backgroundImage = toBeBackground;
	
	int const smallPad = 12;
	localBounds.reduce(smallPad, smallPad);
	
	int x(localBounds.getX()), y(0);
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
	// 1. make audioProcessor the listener that does this extraction
	// 2. make paintOnsetMarkers happen on editor constructon as well
	
	if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		// onsets are immediately ready to draw
		paintOnsetMarkers();
	}
	else if (auto &timbreData = audioProcessor.getTimbreSpaceNeededData(); &timbreData == source){
		// draw timbre points
		drawTimbreSpacePoints();
	}
	else {
		GranularEditorCommon::changeListenerCallback(source);
	}
}
