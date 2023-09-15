/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
//==============================================================================

TsaraGranularAudioProcessorEditor::TsaraGranularAudioProcessorEditor (TsaraGranularAudioProcessor& p)
	: AudioProcessorEditor (&p)
,	fileComp(juce::File(), "*.wav;*.aif;*.aiff", "", "Select file to open")
,	triggeringButton("hi")
,	calculateOnsetsButton("Calculate Onsets")
,	writeWavsButton("Write Wavs")
,	settingsButton("Settings...")
,	audioProcessor (p)
, 	attachedSliderColumnArray{
		SliderColumn(audioProcessor.apvts, params_e::transpose),
		SliderColumn(audioProcessor.apvts, params_e::position),
		SliderColumn(audioProcessor.apvts, params_e::speed),
		SliderColumn(audioProcessor.apvts, params_e::duration),
		SliderColumn(audioProcessor.apvts, params_e::skew),
		SliderColumn(audioProcessor.apvts, params_e::pan)
	}
{
	addAndMakeVisible (fileComp);
	fileComp.addListener (this);
	fileComp.getRecentFilesFromUserApplicationDataDirectory();
	
	addAndMakeVisible(calculateOnsetsButton);
	calculateOnsetsButton.onClick = [&p]{p.calculateOnsets();};
	
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
	
	for (auto &s : attachedSliderColumnArray){
		addAndMakeVisible( s );
	}
	
	constrainer.setMinimumSize(620, 400);
	setSize (800, 500);
	setResizable(true, true);
}

TsaraGranularAudioProcessorEditor::~TsaraGranularAudioProcessorEditor()
{
	/*for (auto i = 0; i < windows.size(); ++i){
		windows[i]->userTriedToCloseWindow();
	}*/
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
//std::make_unique<SettingsWindow>(juce::Colours::darkmagenta)
	int const intensity = 70;
	auto* settingsWindow = new SettingsWindow (juce::Colour(intensity, intensity, intensity));
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
//==============================================================================
void TsaraGranularAudioProcessorEditor::paint (juce::Graphics& g)
{
	juce::Image image(juce::Image::ARGB, getWidth(), getHeight(), true);
	juce::Graphics tg(image);

	juce::Colour upperLeftColour  = gradientColors[(colourOffsetIndex + 0) % gradientColors.size()];
	juce::Colour lowerRightColour = gradientColors[(colourOffsetIndex + 4) % gradientColors.size()];
	juce::ColourGradient cg(upperLeftColour, 0, 0, lowerRightColour, getWidth(), getHeight(), true);
	cg.addColour(0.3, gradientColors[(colourOffsetIndex + 1) % gradientColors.size()]);
	cg.addColour(0.5, gradientColors[(colourOffsetIndex + 2) % gradientColors.size()]);
	cg.addColour(0.7, gradientColors[(colourOffsetIndex + 3) % gradientColors.size()]);
	tg.setGradientFill(cg);
	tg.fillAll();

	g.drawImage(image, getLocalBounds().toFloat());

	g.setColour (juce::Colours::white);
	g.setFont (15.0f);
	g.drawFittedText ("tsaaaarrraaaaa grrrraaaaaanuuuuuulaaaaaate", getLocalBounds(), juce::Justification::centred, 1);
}

void TsaraGranularAudioProcessorEditor::resized()
{
	
	constrainer.checkComponentBounds(this);
	
	juce::Rectangle<int> localBounds = getLocalBounds();
	
//	std::cout << "x: " << localBounds.getX() << " y: " << localBounds.getY() <<
//			" w: " << localBounds.getWidth() << " h: " << localBounds.getHeight() << '\n';

	
	int const smallPad = 10;
	localBounds.reduce(smallPad, smallPad);
	
	int x(0), y(0);
	{
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
		calculateOnsetsButton.setBounds(x, y, buttonWidth, buttonHeight);
		x += buttonWidth;
		writeWavsButton.setBounds(x, y, buttonWidth, buttonHeight);
		x += buttonWidth;
		settingsButton.setBounds(x, y, buttonWidth, buttonHeight);
		x += buttonWidth;
		buttonWidth = buttonHeight;
		triggeringButton.setBounds(x, y, buttonWidth, buttonHeight);
		y += buttonHeight;
		y += smallPad;
	}
	
	int const alottedCompHeight = localBounds.getHeight() - y + smallPad;
	int const alottedCompWidth = localBounds.getWidth() / attachedSliderColumnArray.size();
	
	for (int i = 0; i < attachedSliderColumnArray.size(); ++i){
		int left = i * alottedCompWidth + localBounds.getX();
		attachedSliderColumnArray[i].setBounds(left, y, alottedCompWidth, alottedCompHeight);
	}
}

//void TsaraGranularAudioProcessorEditor::sliderValueChanged(juce::Slider* sliderThatWasMoved)
//{
//	// nothing needed, everything changed is a parameter
//}
void TsaraGranularAudioProcessorEditor::readFile (const juce::File& fileToRead)
{
	if (! fileToRead.existsAsFile())
		return;

	juce::String fn = fileToRead.getFullPathName();
	std::string st_str = fn.toStdString();
	
	audioProcessor.writeToLog(st_str);
	audioProcessor.loadAudioFile(fileToRead);
	
	fileComp.setCurrentFile(fileToRead, true);
}

void TsaraGranularAudioProcessorEditor::filenameComponentChanged (juce::FilenameComponent* fileComponentThatHasChanged)
{
	if (fileComponentThatHasChanged == &fileComp){
		readFile (fileComp.getCurrentFile());
	}
}
