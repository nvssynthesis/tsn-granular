/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Gui/SettingsWindow.h"

//==============================================================================

TsaraGranularAudioProcessorEditor::TsaraGranularAudioProcessorEditor (TsaraGranularAudioProcessor& p)
	: AudioProcessorEditor (&p)
,	fileComp(juce::File(), "*.wav;*.aif;*.aiff", "", "Select file to open")
,	mainParamsComp(p.apvts)
,	waveformAndPositionComponent(512, p.getAudioFormatManager(), p.apvts)
,	triggeringButton("Manual Trigger")	// unused
,	askForAnalysisButton("Calculate Analysis")
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	audioProcessor (p)
{
	addAndMakeVisible (fileComp);
	fileComp.addListener (this);
	fileComp.getRecentFilesFromUserApplicationDataDirectory();
	
	addAndMakeVisible(askForAnalysisButton);
	askForAnalysisButton.onClick = [this]{
		askForAnalysis();
	};
	
	addAndMakeVisible(writeWavsButton);
	writeWavsButton.onClick = [&p]{p.writeEvents();};
	
	addAndMakeVisible(settingsButton);
	settingsButton.onClick = [this]{
		closeAllWindows();
		popupSettings(false);
	};
	
	addAndMakeVisible(triggeringButton);
	triggeringButton.onClick = [this, &p]{ updateToggleState(&triggeringButton, "Trigger", p.triggerValFromEditor);	};
	triggeringButton.setClickingTogglesState(true);
	
	addAndMakeVisible(mainParamsComp);
	
	addAndMakeVisible(waveformAndPositionComponent);
	
	addAndMakeVisible(timbreSpaceComponent);
	timbreSpaceComponent.addMouseListener(this, false);
	
	getLookAndFeel().setColour(juce::Slider::thumbColourId, juce::Colours::purple);
	
	constrainer.setMinimumSize(620, 500);
	setSize (800, 550);
	setResizable(true, true);
}

TsaraGranularAudioProcessorEditor::~TsaraGranularAudioProcessorEditor()
{
	closeAllWindows();

	fileComp.pushRecentFilesToFile();
}
//==============================================================================
void TsaraGranularAudioProcessorEditor::closeAllWindows()
{
	for (auto& window : windows)
		window.deleteAndZero();

	windows.clear();
}
//==============================================================================
void TsaraGranularAudioProcessorEditor::updateToggleState (juce::Button* button, juce::String name, bool &valToAffect)
{
	bool state = button->getToggleState();
	valToAffect = state;
	juce::String stateString = state ? "ON" : "OFF";

	juce::Logger::outputDebugString (name + " Button changed to " + stateString);
}
void TsaraGranularAudioProcessorEditor::popupSettings(bool native){
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
void TsaraGranularAudioProcessorEditor::askForAnalysis(){
	if (TsaraGranularAudioProcessor* a = dynamic_cast<TsaraGranularAudioProcessor*>(&processor)){
		fmt::print("success dynamic casting");
		a->askForAnalysis();
	}
}
void TsaraGranularAudioProcessorEditor::paintMarkers(std::vector<float> onsetsInSeconds,
													 std::vector<std::vector<float>> PCA){
	waveformAndPositionComponent.wc.removeMarkers();
	timbreSpaceComponent.clear(); // clearing to make way for points we're about to be adding
	
	//=========================draw waveform markers====================
	fmt::print("Editor: Drawing waveform markers\n");
	double const sr = audioProcessor.getAnalysisSettings().sampleRate;
	size_t nSamps = audioProcessor.getCurrentWaveSize();
	for (auto onset : onsetsInSeconds){
		onset *= sr;
		onset /= static_cast<double>(nSamps);
		waveformAndPositionComponent.wc.addMarker(onset);
	}
	  
	//=====================draw PCA timbre space points=================
	fmt::print("Editor: Drawing PCA points\n");
	size_t constexpr nDim {5};
	std::array<size_t, nDim> constexpr dimensions {
		0,
		1,
		2,
		3,
		4
	};
	std::array<float, nDim> normalizers;
	for (int i = 0; i < nDim; ++i){
		auto const range = nvs::analysis::getRangeOfDimension(PCA, dimensions[i]);
		normalizers[i] = nvs::analysis::getNormalizationMultiplier(range);
	}
	for (std::vector<float> const &pcaFrame : PCA){
		juce::Point<float> p;
		
		// just some bull as a placeholder for actual timbral analysis
		float val = pcaFrame[dimensions[0]];
		float val2 = pcaFrame[dimensions[1]];
#pragma message("need proper normalization")
		p.setX(val * normalizers[0]);
		p.setY(val2 * normalizers[1]);
		std::array<float, 3> color;
		color = {
			( pcaFrame[dimensions[2]] * normalizers[2]),
			( pcaFrame[dimensions[3]] * normalizers[3]),
			( pcaFrame[dimensions[4]] * normalizers[4])
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsetsSeconds.
		timbreSpaceComponent.add5DPoint(p, color);
		fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
	}
	repaint();
}
void TsaraGranularAudioProcessorEditor::mouseDown(const juce::MouseEvent &event) {
	auto pIdx = timbreSpaceComponent.getCurrentPoint();

	if ( auto const onsetsInSeconds = audioProcessor.getOnsets() ){
		float const onsetSeconds = onsetsInSeconds.value()[pIdx];
		double const sr = audioProcessor.getAnalysisSettings().sampleRate;
		double const onsetSamps = static_cast<size_t>(onsetSeconds * sr);
		double const lengthSamps = static_cast<double>( audioProcessor.getCurrentWaveSize() );
		double const onsetNormalized = onsetSamps / lengthSamps;
		mainParamsComp.setSliderParam(params_e::position, onsetNormalized);
	}
	if ( auto const bfcc = audioProcessor.getOnsetwiseBFCCs() ){
		std::vector<float> thisBfccSet = bfcc.value()[pIdx]; 
		
		fmt::print("BFCC: \t{:.2f},\t{:.2f},\t{:.2f},\t{:.2f}\t{:.2f}\n", thisBfccSet[1],thisBfccSet[2],thisBfccSet[3],thisBfccSet[4],thisBfccSet[5]);
	}
}
void TsaraGranularAudioProcessorEditor::mouseDrag(const juce::MouseEvent &event) {
	auto pIdx = timbreSpaceComponent.getCurrentPoint();
//	fmt::print("editor mouse drag: {}\n", pIdx);
}
//==============================================================================
void TsaraGranularAudioProcessorEditor::paint (juce::Graphics& g)
{
	juce::Image image(juce::Image::ARGB, getWidth(), getHeight(), true);
	juce::Graphics tg(image);

	juce::Colour upperLeftColour  = gradientColors[(colourOffsetIndex + 0) % gradientColors.size()];
	upperLeftColour = upperLeftColour.interpolatedWith(juce::Colours::darkred, 0.7f);
	juce::Colour lowerRightColour = gradientColors[(colourOffsetIndex + gradientColors.size()-1) % gradientColors.size()];
	lowerRightColour = lowerRightColour.interpolatedWith(juce::Colours::darkred, 0.7f);
	juce::ColourGradient cg(upperLeftColour, 0, 0, lowerRightColour, getWidth(), getHeight(), true);
	cg.addColour(0.5, gradientColors[(colourOffsetIndex + 1) % gradientColors.size()]);
	tg.setGradientFill(cg);
	tg.fillAll();

	g.drawImage(image, getLocalBounds().toFloat());
	
	{
		auto bounds = getLocalBounds();

		juce::int64 seed = bounds.getWidth() + bounds.getHeight();
		juce::Random rng(seed);
		
		const auto dotDim = 2;
		for (auto i = 0; i < bounds.getWidth(); i += 2){
			for (auto j = 0; j < bounds.getHeight(); j += 3){
				
				float val = rng.nextFloat();
				if (val > 0.7f){
					auto colour = juce::Colour(juce::uint8(rng.nextInt()), 0, 0,
												   rng.nextFloat() * 0.3f);	// alpha
					g.setColour(colour);
					g.fillEllipse(i, j, dotDim, dotDim);
//					g.drawEllipse(i, j, dotDim, dotDim, dotDim);
				}
				
			}
		}
	}
	// fixes bug where dots drawn within range of juce logo would disappear:
	timbreSpaceComponent.repaint();
}

void TsaraGranularAudioProcessorEditor::resized()
{
	constrainer.checkComponentBounds(this);
	juce::Rectangle<int> localBounds = getLocalBounds();
	
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
		triggeringButton.setBounds(x, y, buttonWidth, buttonHeight);
		y += buttonHeight;
//		y += smallPad;
	}
	auto const mainParamsRemainingHeightRatio  = 0.6f;
	auto const waveformCompRemainingHeightRatio = 0.15f;
	auto const timbreSpaceRemainingHeightRatio = 0.25f;
	auto const totalRemainingHeightRatiosSummed = (mainParamsRemainingHeightRatio + waveformCompRemainingHeightRatio + timbreSpaceRemainingHeightRatio) ;
	assert(totalRemainingHeightRatiosSummed >= 0.999f);
	assert(totalRemainingHeightRatiosSummed <= 1.001f);
	{
		auto const mainParamsRemainingHeight = mainParamsRemainingHeightRatio * localBounds.getHeight();

		int const alottedMainParamsHeight = mainParamsRemainingHeight - y + smallPad;
		int const alottedMainParamsWidth = localBounds.getWidth();
		juce::Rectangle<int> mainParamsBounds(localBounds.getX(), y, alottedMainParamsWidth, alottedMainParamsHeight);
		mainParamsComp.setBounds(mainParamsBounds);
		y += mainParamsComp.getHeight();
	}
	{
		auto const waveformComponentHeight = waveformCompRemainingHeightRatio * localBounds.getHeight();
		waveformAndPositionComponent.setBounds(localBounds.getX(), y, localBounds.getWidth(), waveformComponentHeight);
		y += waveformAndPositionComponent.getHeight();
	}
	{
		auto const timbreSpaceComponentHeight = timbreSpaceRemainingHeightRatio * localBounds.getHeight();
		timbreSpaceComponent.setBounds(localBounds.getX(), y, localBounds.getWidth(), timbreSpaceComponentHeight);
		y += timbreSpaceComponent.getHeight();
	}
}

void TsaraGranularAudioProcessorEditor::readFile (const juce::File& fileToRead)
{
	if (! fileToRead.existsAsFile()){
		return;
	}

	juce::String fn = fileToRead.getFullPathName();
	std::string st_str = fn.toStdString();
	
	audioProcessor.writeToLog(st_str);
	audioProcessor.loadAudioFile(fileToRead, waveformAndPositionComponent.wc.getThumbnail());
	askForAnalysis();
	
	// redundant:
	// fileComp.setCurrentFile(fileToRead, true);
}

void TsaraGranularAudioProcessorEditor::filenameComponentChanged (juce::FilenameComponent* fileComponentThatHasChanged)
{
	if (fileComponentThatHasChanged == &fileComp){
		readFile (fileComp.getCurrentFile());
//		doOnsetAnalysisAndPaintMarkers();
		
	}
}
void TsaraGranularAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*  source) {
	fmt::print("editor: change message received\n");
	if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		fmt::print("editor: dynamic cast to threaded analyzer ptr successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		std::vector<float> onsetsInSeconds = a->getOnsetsInSeconds();
		std::vector<std::vector<float>> PCA = a->getPCA();
		
		paintMarkers(onsetsInSeconds, PCA);
		fmt::print("editor: change listener callback: got things\n");
		// paint markers (replace function doOnsetAnalysisAndPaintMarkers() with just paintMarkers()?)
	}
}
