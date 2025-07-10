/*
  ==============================================================================

    TimbreSpaceComponent.cpp
    Created: 21 Jan 2025 1:29:17am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceComponent.h"
#include "../slicer_granular/Source/Params/params.h"
#include "Analysis/ThreadedAnalyzer.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include "../Navigation/LFO.h"

//============================================TimbreSpaceComponent=============================================

namespace {
using timbre2DPoint = nvs::timbrespace::timbre2DPoint;
using timbre3DPoint = nvs::timbrespace::timbre3DPoint;
using Timbre5DPoint = nvs::timbrespace::Timbre5DPoint;

auto pointToRect = [](timbre2DPoint p, float pt_sz) -> juce::Rectangle<float> {
	timbre2DPoint upperLeft{p}, bottomRight{p};
	float halfDotSize {2.f * pt_sz};
	upperLeft.addXY(-halfDotSize, -halfDotSize);
	bottomRight.addXY(halfDotSize, halfDotSize);
	return juce::Rectangle<float>(upperLeft, bottomRight);
};

auto transformFromZeroOrigin = [](timbre2DPoint p) -> timbre2DPoint
{
//	input bounds:  ([-1..1], [-1..1])
//	output bounds: ([0..1], [0..1])
	p.addXY(1.f, 1.f);	// [0..2]
	p *= 0.5f;			// [-1..1]
	return p;
};

timbre2DPoint bipolar2dPointToComponentSpace(timbre2DPoint p2D, float componentWidth, float componentHeight){
	return transformFromZeroOrigin(p2D) * timbre2DPoint(componentWidth, componentHeight);
}

auto softclip = [](float const x, float const bias = -0.2f, float const q = 0.2f, float const s = 0.6f, float const scale = 5.f) -> float
{
	float const y = scale * ((q*x - bias) / (s + std::abs(q*x - bias))) + bias/2;
	return y;
};

timbre3DPoint biuni(timbre3DPoint const &bipolar_p3){
	using nvs::memoryless::biuni;
	timbre3DPoint const uni_pts3 {biuni(bipolar_p3[0]), biuni(bipolar_p3[1]), biuni(bipolar_p3[2])};
	return uni_pts3;
}
auto scale(auto x, auto in_low, auto in_high, auto out_low, auto out_high){
	return out_low + (x - in_low) * (out_high - out_low) / (in_high - in_low);
}
juce::Colour p3ToColour(timbre3DPoint const &p3, float alpha=1.f){
	float const h = p3[0];
	float const s = p3[1];
	float v = p3[2];
	assert (v >= 0.0);
	assert (v <= 1.0);
	v = scale(v, 0.f, 1.f, 0.45f, 1.f);
	return juce::Colour(h, s, v, alpha);
}
void setLfoOffsetParamsFromPoint(juce::AudioProcessorValueTreeState &apvts, timbre2DPoint p2D){
	apvts.getParameterAsValue("nav_tendency_x") = p2D.getX();
	apvts.getParameterAsValue("nav_tendency_y") = p2D.getY();
}

template<typename T>
bool containsValue(const std::vector<T>& vec, T value) {
	return std::find(vec.begin(), vec.end(), value) != vec.end();
}
}	// anonymous namespace

TimbreSpaceComponent::TimbreSpaceComponent(juce::AudioProcessorValueTreeState &apvts, TimbreSpace &timbreSpace)
:	_apvts{apvts} , _timbreSpace(timbreSpace)
{
	_timbreSpace.addChangeListener(this);
	if (_timbreSpace.isSavePending()){
		showAnalysisSaveDialog();
	}
}
void TimbreSpaceComponent::showAnalysisSaveDialog() {
	callback = new Callback(*this);
	
	auto const result = juce::AlertWindow::showYesNoCancelBox(
								  juce::MessageBoxIconType::QuestionIcon, // MessageBoxIconType iconType
								  "Save Analysis?", // const String &title
								  "Would you like to save the current timbral analysis for future use?", // const String &message
								  "Save", // const String &button1Text
								  "Don't save", // const String &button2Text
								  "Always save [not implemented yet, falls back to Save]", // const String &button3Text
								  this, // Component *associatedComponent
								callback); // ModalComponentManager::Callback *callback
}

void TimbreSpaceComponent::paint(juce::Graphics &g) {
	using juce::Colour;
	using Point = juce::Point<float>;
	
	// keep these out of the following scope as long as i want to reuse them for the navigator
	juce::Rectangle<float> r_bounds = g.getClipBounds().toFloat();
	auto centre = r_bounds.getCentre();
	float radius = juce::jmax (r_bounds.getWidth(), r_bounds.getHeight()) * 0.5f;
	
	juce::ColourGradient radialGrad (
		juce::Colours::white.withAlpha(0.5f),    // centre colour
		centre,                        			 // centre point
		juce::Colours::darkgrey.withAlpha(0.5f), // edge colour
		centre.translated (radius, 0),  		 // a point on the circumference
		true                           			 // isRadial = true
	);
	g.setGradientFill (radialGrad);
	g.fillRect (r_bounds);

	
	g.setColour(juce::Colours::snow.withAlpha(0.2f));
	g.drawRect(getLocalBounds(), 1);
	
	auto const w = r_bounds.getWidth();
	auto const h = r_bounds.getHeight();
	auto const &timbres5D = _timbreSpace.getTimbreSpace();
	{
		std::vector<Timbre5DPoint> current_points;
		current_points.reserve(_timbreSpace.getCurrentPointIndices().size());
		for (auto & p : _timbreSpace.getCurrentPointIndices()){
			current_points.push_back(timbres5D[p.idx]);
		}
		
		for (auto p5 : timbres5D){
			timbre2DPoint p2 = bipolar2dPointToComponentSpace(p5.get2D(), w, h);
			auto const p3 = p5.get3D();
			
			auto const uni_p3 = biuni(p3);
			juce::Colour fillColour = p3ToColour(uni_p3);
			
			float const z_closeness = uni_p3[0] * uni_p3[1] * uni_p3[2] * 10.f;
			auto const rect = pointToRect(p2, softclip(z_closeness));
			
			if (containsValue(current_points, p5)){
				// brighter colour for selected point
				fillColour = fillColour.withMultipliedBrightness(1.75f).withMultipliedLightness(1.1f);
				// also draw glowing orb under the point
				float const orb_radius = rect.getWidth() * 1.5f;
				juce::ColourGradient gradient(
					fillColour.withAlpha(1.0f),	// Center color (fully opaque white)
					p2.x, p2.y,					// Center position
					fillColour.withAlpha(0.0f),	// Edge color (fully transparent)
					p2.x, p2.y + orb_radius,	// Radius for the gradient
					true						// Radial gradient
				);
				g.setGradientFill(gradient);
				g.fillEllipse(p2.x - orb_radius, p2.y - orb_radius, orb_radius * 2, orb_radius * 2);
			}
			// draw a filled circle with a different coloured outline
			g.setColour(fillColour);
			g.fillEllipse(rect);
			g.setColour(fillColour.withRotatedHue(0.25f).withMultipliedLightness(2.f));
			g.drawEllipse(rect, 0.5f);
		}
		// draw lines from target to nearby (selected, current) points 
		for (auto const &p : current_points){
			auto const center = bipolar2dPointToComponentSpace(nav._p2D, w, h);
			auto const dest = bipolar2dPointToComponentSpace(p.get2D(), w, h);
			auto const l = juce::Line<float>(center, dest);
			auto const norm = [l, w, h](){
				auto const a = l.getLength() / std::sqrt( (w * w) + (h * h) );
				auto const b = 2.f * (0.5f - a);
				return juce::jlimit(0.1f, 1.f, b * b * b + 0.1f);
			}();
			g.setColour(juce::Colours::whitesmoke.withAlpha(norm));
			g.drawLine(l, 1.f);
		}
	}
	{
		if (tsn_mouse._dragging){
			// draw orb around mouse
			auto const uvz = tsn_mouse._uvz;
			juce::Colour c = p3ToColour(biuni(uvz));
			juce::Point<int> const xy = getMouseXYRelative();
			auto const r = pointToRect(xy.toFloat(), 1.f);
			float const orbRadius = r.getWidth() * 4.5f;
			juce::ColourGradient gradient(c,
										  xy.x, xy.y,
										  c.withAlpha(0.f),
										  xy.x, xy.y + orbRadius,
										  true);
			g.setGradientFill(gradient);
			g.fillEllipse(xy.x - orbRadius, xy.y - orbRadius, orbRadius * 2, orbRadius * 2);
		}
	}
	// draw navigator
	{
		g.setColour(juce::Colours::black);
		g.fillEllipse(pointToRect(bipolar2dPointToComponentSpace(nav._p2D, w, h), 2.f));
	}
}

void TimbreSpaceComponent::mouseDragOrDown (juce::Point<int> mousePos) {
	tsn_mouse._dragging = true;
	juce::Point<float> p2D_norm = normalizePosition_neg1_pos1(mousePos);
	Timbre5DPoint p5D {
		._p2D{p2D_norm},
		._p3D{tsn_mouse._uvz[0], tsn_mouse._uvz[1], tsn_mouse._uvz[2]}
	};
	
	setLfoOffsetParamsFromPoint(_apvts, p2D_norm);
}

void TimbreSpaceComponent::mouseDrag(const juce::MouseEvent &event) {
	mouseDragOrDown(event.getPosition());
}
void TimbreSpaceComponent::mouseDown (const juce::MouseEvent &event) {
	mouseDragOrDown(event.getMouseDownPosition());
}
void TimbreSpaceComponent::mouseUp (const juce::MouseEvent &event) {
	tsn_mouse._dragging = false;
}

void TimbreSpaceComponent::mouseEnter(const juce::MouseEvent &event) {
	juce::MouseCursor newCursor(tsn_mouse._image, 0, 0);
	setMouseCursor(newCursor);
}

void TimbreSpaceComponent::mouseExit(const juce::MouseEvent &event) {
	setMouseCursor(juce::MouseCursor::NormalCursor);
}
void TimbreSpaceComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
	if (tsn_mouse._dragging) {
		return;
	}
	float &u = tsn_mouse._uvz[0];
	u = juce::jlimit(0.f, 1.f, u + wheel.deltaY);
		
	updateCursor();
}
void TimbreSpaceComponent::updateCursor() {
	tsn_mouse.createMouseImage();
	auto mouseImage = tsn_mouse._image;
	juce::MouseCursor customCursor(mouseImage, mouseImage.getWidth() / 2, mouseImage.getHeight() / 2);
	setMouseCursor(customCursor);
}

void TimbreSpaceComponent::resized() {
	auto const b = getLocalBounds();
	auto const proportionRect = juce::Rectangle<float> {0.1f, 0.05f,
														0.8f, 0.05f};
	auto progressBounds = b.getProportion(proportionRect);
	progressIndicator.setBounds(progressBounds);
}

void ProgressIndicator::paint(juce::Graphics &g) {
	auto const b = getLocalBounds();
	g.setColour(juce::Colours::whitesmoke);
	g.fillRect(b);
	g.setColour(juce::Colours::black);
	g.drawRect(b, 2.f);
	
	int partialW = b.getWidth() * progress;
	auto progressBar = b.withWidth(partialW);
	g.fillRect(progressBar);
	
	g.setColour(juce::Colours::whitesmoke);
	g.setFont(juce::Font("Courier New", 15.f, juce::Font::FontStyleFlags::plain));
	g.drawText(message, b, juce::Justification::centred);
}
void ProgressIndicator::resized() {}

void TimbreSpaceComponent::changeListenerCallback (juce::ChangeBroadcaster* source) {
	if (auto *a = dynamic_cast<nvs::analysis::RunLoopStatus*>(source)) {
		std::cout << "timbre space comp: RunLoopStatus: CHANGE listener: SHOWING progress indicator\n";
		addAndMakeVisible(progressIndicator);
		progressIndicator.progress = a->getProgress();
		progressIndicator.message = a->getMessage();
		std::cout << "PROGRESS: " << progressIndicator.progress << '\n';
		progressIndicator.repaint();
	}
	else if (dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		std::cout << "timbre space comp: ThreadedAnalyzer: CHANGE listener: hiding progress indicator\n";
		progressIndicator.setVisible(false);
	}
	else if (auto const *n = dynamic_cast<nvs::nav::Navigator*>(source)){
		// move navigation indicator
		auto p = n->storedPoint;
		setNavigatorPoint(juce::Point<float>(p[0], p[1]));
		repaint();
	}
	else if (source == &_timbreSpace){
		showAnalysisSaveDialog();
	}
}
void TimbreSpaceComponent::exitSignalSent() {
	std::cout << "timbre space comp: ThreadedAnalyzer: THREAD listener: hiding progress indicator\n";
	progressIndicator.setVisible(false);
}


void TimbreSpaceComponent::TSNMouse::createMouseImage() {
	juce::Image image(juce::Image::ARGB, 16, 16, true);
	juce::Graphics g(image);
	
	juce::Path triPath;
	auto b = image.getBounds();
	
	auto x0 = b.getX();
	auto x1 = x0 + (0.05 * b.getWidth());
	auto x2 = x0 + (0.45 * b.getWidth());
	auto x3 = b.getRight();

	auto y0 = b.getY();
	auto y1 = y0 + (0.86 * b.getHeight());
	auto y2 = b.getBottom();
	
	using Line = juce::Line<float>;
	using Point = juce::Point<float>;
	triPath.startNewSubPath (Point(x1, y0));	// A
	triPath.lineTo        	(Point(x0, y2));	// B
	triPath.lineTo        	(Point(x2, y1));	// C
	triPath.closeSubPath();						// back to A
	
	g.setColour(p3ToColour(biuni(_uvz)));
	g.fillPath  (triPath);
	
	_image = image;
}

std::vector<nvs::util::WeightedIdx> TimbreSpaceComponent::getCurrentPointIndices() const {
	return _timbreSpace.getCurrentPointIndices();
}

juce::Point<float> TimbreSpaceComponent::normalizePosition_neg1_pos1(juce::Point<int> pos){
	float x = static_cast<float>(pos.getX());
	float y = static_cast<float>(pos.getY());
	auto bounds = getLocalBounds().toFloat();
	x /= bounds.getWidth();
	y /= bounds.getHeight();
	using namespace nvs::memoryless;
	x = unibi(x);
	y = unibi(y);
	return juce::Point<float>(x,y);
}

void TimbreSpaceComponent::setNavigatorPoint(timbre2DPoint p){
	nav._p2D = p;
}
ProgressIndicator& TimbreSpaceComponent::getProgressIndicator(){
	return progressIndicator;
}

namespace {
void showSaveErrorAlert() {
	juce::AlertWindow::showMessageBoxAsync(
		juce::AlertWindow::WarningIcon,
		"Save Error",
		"Could not save the timbral analysis file. Please check the file location and try again."
	);
}
}

void TimbreSpaceComponent::saveAnalysis(){
	juce::File analysesDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
		.getChildFile("tsn_granular")
		.getChildFile("Analyses");

	analysesDir.createDirectory();	// create directory if it doesn't exist yet
	
	fileChooser = std::make_unique<juce::FileChooser>("Save Timbral Analysis",	//  const String &dialogBoxTitle,
															analysesDir,
															"*.tsan",	//  const String &filePatternsAllowed=String(),
															true,	// bool useOSNativeDialogBox
															false, 	// bool treatFilePackagesAsDirectories=false,
															this 	//  Component *parentComponent=nullptr
														   );

	auto const vt = _timbreSpace.getTimbreSpaceTree();
	// Show async save dialog
	fileChooser->launchAsync( juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
		[vt](const juce::FileChooser& fc) {
			auto file = fc.getResult();
			if (file != juce::File{}) {
				// Ensure proper extension
				if (!file.hasFileExtension(".tsb")) {
					file = file.withFileExtension(".tsb");
				}
				// Save the ValueTree to file
				if (nvs::util::saveValueTreeToBinary(vt, file)) {
					// Success - maybe update TimbreSpace with the saved path
//									_ts.setCurrentAnalysisPath(file.getFullPathName());
					std::cout << "should update other state with analysis file name\n";
#pragma message("update other state with analysis file name")
				}
				else {
					// Handle save error
					showSaveErrorAlert();
				}
			}
		}
	);
}

TimbreSpaceComponent::Callback::Callback(TimbreSpaceComponent &comp)
: _ts_comp(comp)
{}
void TimbreSpaceComponent::Callback::modalStateFinished(int choice) {
	switch (choice) {
		case 1:
		{
			_ts_comp.saveAnalysis();
			break;
		}
		case 2:
			break;
		case 0:
			// always save functionality will be implemented later
#pragma message("implement 'always save' functionality")
		{
			_ts_comp.saveAnalysis();
			break;
		}
		default:
			break;
	}
}
