/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "Gui/SettingsWindow.h"

//==============================================================================

TsnGranularAudioProcessorEditor::TsnGranularAudioProcessorEditor (TsnGranularAudioProcessor& p)
: 	Slicer_granularAudioProcessorEditor (p)
,	askForAnalysisButton("Calculate Analysis")
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	positionQuantizeStrengthComboBox("Position Quantize Strength")
,	audioProcessor(p)
{
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
	
	addAndMakeVisible(tabbedPages);
	addAndMakeVisible(waveformAndPositionComponent);
	
	addAndMakeVisible(timbreSpaceComponent);
	timbreSpaceComponent.addMouseListener(this, false);
	
	constrainer.setMinimumSize(620, 500);
	setSize (680, 950);
	setResizable(true, true);
}

TsnGranularAudioProcessorEditor::~TsnGranularAudioProcessorEditor()
{
	closeAllWindows();
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
void TsnGranularAudioProcessorEditor::displayGrainDescriptions() {
	audioProcessor.readGrainDescriptionData(grainDescriptions);
	waveformAndPositionComponent.wc.removeMarkers(WaveformComponent::MarkerType::CurrentPosition);
	for (auto &gd : grainDescriptions){
		waveformAndPositionComponent.wc.addMarker(gd);
#pragma message("fill this in")
		// light up corresponding timbre space point
		
	}
}
void drawWaveformMarkers(WaveformComponent &wc, std::vector<float> const &onsetsInSeconds, TsnGranularAudioProcessor &p, bool verbose = true){
	if (verbose){
		p.writeToLog("Editor: Drawing waveform markers\n");
	}
	double const sr = p.getAnalysisSettings().sampleRate;
	size_t nSamps = p.getCurrentWaveSize();
	for (auto onset : onsetsInSeconds){
		onset *= sr;
		onset /= static_cast<double>(nSamps);
		wc.addMarker(onset);
	}
}
void drawTimbreSpacePoints(TimbreSpaceComponent &timbreSpaceComponent, std::vector<std::vector<float>> const &PCA, Slicer_granularAudioProcessor &p, bool verbose = true){
	if (verbose){
		p.writeToLog("Editor: Drawing PCA points\n");
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
	auto const range = nvs::analysis::getRangeOfDimension(PCA, dimensions[i]);
		normalizers[i] = nvs::analysis::getNormalizationMultiplier(range);
	}
	for (std::vector<float> const &pcaFrame : PCA) {
		// just some bull as a placeholder for actual timbral analysis
		#pragma message("need proper normalization")
		juce::Point<float> const p(pcaFrame[dimensions[0]] * normalizers[0],
								   pcaFrame[dimensions[1]] * normalizers[1]);
		std::array<float, 3> const color {
			( pcaFrame[dimensions[2]] * normalizers[2] ),
			( pcaFrame[dimensions[3]] * normalizers[3] ),
			( pcaFrame[dimensions[4]] * normalizers[4])
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsetsSeconds.
		float const padding_scalar = 0.95f;
		timbreSpaceComponent.add5DPoint(p * padding_scalar, color);
		fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
	}
}
void TsnGranularAudioProcessorEditor::paintMarkers(std::vector<float> &onsetsInSeconds,
													 std::vector<std::vector<float>> &PCA){
	waveformAndPositionComponent.wc.removeMarkers(WaveformComponent::MarkerType::Onset);
	timbreSpaceComponent.clear(); // clearing to make way for points we're about to be adding
	drawWaveformMarkers(waveformAndPositionComponent.wc, onsetsInSeconds, audioProcessor);
	drawTimbreSpacePoints(timbreSpaceComponent, PCA, GranularEditorCommon::audioProcessor);
	repaint();
}
void TsnGranularAudioProcessorEditor::setPositionSliderFromChosenPoint() {
	audioProcessor.writeToLog("editor: setPositionSliderFromChosenPoint");
	auto const pIdx = timbreSpaceComponent.getCurrentPointIdx();

	if (audioProcessor.getOnsets().size()){
		float const onsetSeconds = audioProcessor.getOnsets()[pIdx];
		double const sr = audioProcessor.getAnalysisSettings().sampleRate;
		double const onsetSamps = static_cast<size_t>(onsetSeconds * sr);
		double const lengthSamps = static_cast<double>( audioProcessor.getCurrentWaveSize() );
		double const onsetNormalized = onsetSamps / lengthSamps;
		std::string const a = getParamElement<params_e::position, param_elem_e::name>();
		auto posParam = audioProcessor.getAPVTS().getParameter(a);
		posParam->setValueNotifyingHost(onsetNormalized);
	}
	if (audioProcessor.getOnsetwiseBFCCs().size()){
		std::vector<float> const thisBfccSet = audioProcessor.getOnsetwiseBFCCs()[pIdx];
		fmt::print("BFCC: \t{:.2f},\t{:.2f},\t{:.2f},\t{:.2f}\t{:.2f}\n", thisBfccSet[1],thisBfccSet[2],thisBfccSet[3],thisBfccSet[4],thisBfccSet[5]);
	}
}

void TsnGranularAudioProcessorEditor::mouseDown(const juce::MouseEvent &event) {
	setPositionSliderFromChosenPoint();
}
void TsnGranularAudioProcessorEditor::mouseDrag(const juce::MouseEvent &event) {
	setPositionSliderFromChosenPoint();
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

	g.drawImage(backgroundImage, getLocalBounds().toFloat());
	
	{
		auto const bounds = getLocalBounds();

		juce::int64 const seed = bounds.getWidth() + bounds.getHeight();
		juce::Random rng(seed);
		
		for (auto i = 0; i < bounds.getWidth(); i += 2){
			for (auto j = 0; j < bounds.getHeight(); j += 3){
				
				float const val = rng.nextFloat();
				if (val > 0.7f){
					auto const colour = juce::Colour(juce::uint8(rng.nextInt()), 0, 0,
												   rng.nextFloat() * 0.3f);	// alpha
					g.setColour(colour);
//					g.fillEllipse(i, j, dotDim, dotDim);
//					g.drawEllipse(i, j, dotDim, dotDim, dotDim);
				}
			}
		}
	}
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
	
	int const smallPad = 10;
	localBounds.reduce(smallPad, smallPad);
	
	int x(0), y(0);
	{	// just some scopes for temporaries
		int fileCompWidth = localBounds.getWidth();
		int fileCompHeight = 20;
		x = localBounds.getX();
		y = localBounds.getY();
		fileComp.setBounds(x, y, fileCompWidth, fileCompHeight);
		y += fileCompHeight;
		y += smallPad;
	}
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
		std::vector<float> onsetsInSeconds = a->getOnsetsInSeconds();
		std::vector<std::vector<float>> PCA = a->getPCA();
		
		paintMarkers(onsetsInSeconds, PCA);
		audioProcessor.writeToLog("editor: change listener callback: got timbre data to paint\n");
	}
	else {
		GranularEditorCommon::changeListenerCallback(source);
	}
}
