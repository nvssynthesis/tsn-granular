/*
  ==============================================================================

    TimbreSpaceComponent.cpp
    Created: 21 Jan 2025 1:29:17am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceComponent.h"
#include "../TsnGranularPluginProcessor.h"
#include "Analysis/ThreadedAnalyzer.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include "StringAxiom.h"

//============================================TimbreSpaceComponent=============================================

namespace {
using namespace nvs::timbrespace;

juce::Point<float> p2DtoJucePoint(Timbre2DPoint p2D) {
    return {p2D[0], p2D[1]};
}

Timbre2DPoint jucePointToTimbre2DPoint(const juce::Point<float> p2D) {
    return Timbre2DPoint {p2D.x, p2D.y};
}

juce::Rectangle<float> pointToRect(const Timbre2DPoint p, float pt_sz) {
	Timbre2DPoint upperLeft{p}, bottomRight{p};
    const float halfDotSize {2.f * pt_sz};
	upperLeft += Timbre2DPoint(-halfDotSize, -halfDotSize);
	bottomRight += Timbre2DPoint(halfDotSize, halfDotSize);
	return {
	    p2DtoJucePoint(upperLeft),
	    p2DtoJucePoint(bottomRight)
	};
}
juce::Rectangle<float> pointToRect(const juce::Point<float> p, const float pt_sz) {
    juce::Point<float> upperLeft{p}, bottomRight{p};
    const float halfDotSize {2.f * pt_sz};
    upperLeft.addXY(-halfDotSize, -halfDotSize);
    bottomRight.addXY(halfDotSize, halfDotSize);
    return {
        upperLeft,
        bottomRight
    };
}

auto transformFromZeroOrigin = [](Timbre2DPoint p) -> Timbre2DPoint
{
//	input bounds:  ([-1..1], [-1..1])
//	output bounds: ([0..1], [0..1])
	p += Timbre2DPoint(1.f, 1.f);	// [0..2]
	p *= 0.5f;			// [-1..1]
	return p;
};

Timbre2DPoint bipolar2dPointToComponentSpace(const Timbre2DPoint& p2D, const float componentWidth, const float componentHeight){
	return Timbre2DPoint(componentWidth, componentHeight).cwiseProduct(transformFromZeroOrigin(p2D));
}

auto softclip = [](const float  x, const float  bias = -0.2f, const float  q = 0.2f, const float  s = 0.6f, const float  scale = 5.f) -> float
{
	const float  y = scale * ((q*x - bias) / (s + std::abs(q*x - bias))) + bias/2;
	return y;
};

Timbre3DPoint biuni(const Timbre3DPoint &bipolar_p3){
	using nvs::memoryless::biuni;
	Timbre3DPoint const uni_pts3 {biuni(bipolar_p3[0]), biuni(bipolar_p3[1]), biuni(bipolar_p3[2])};
	return uni_pts3;
}
auto scale(auto x, auto in_low, auto in_high, auto out_low, auto out_high){
	return out_low + (x - in_low) * (out_high - out_low) / (in_high - in_low);
}
juce::Colour p3ToColour(Timbre3DPoint const &p3, const float alpha=1.f){
	const float  h = p3[0];
	const float  s = p3[1];
	float v = p3[2];
	assert (v >= 0.0);
	assert (v <= 1.0);
	v = scale(v, 0.f, 1.f, 0.45f, 1.f);
	return juce::Colour(h, s, v, alpha);
}
void setLfoOffsetParamsFromPoint(const juce::AudioProcessorValueTreeState &apvts, Timbre2DPoint p2D){
	apvts.getParameterAsValue(nvs::axiom::nav_tendency_x) = p2D(0);
	apvts.getParameterAsValue(nvs::axiom::nav_tendency_y) = p2D(1);
}

template<typename T>
bool containsValue(const std::vector<T>& vec, T value) {
	return std::find(vec.begin(), vec.end(), value) != vec.end();
}
}	// anonymous namespace



//===================================================================================================================
TimbreSpaceComponent::TimbreSpaceComponent(juce::AudioProcessor &proc)
{
	_proc = dynamic_cast<TSNGranularAudioProcessor *>(&proc);
	if (_proc == nullptr) {
		fmt::print("TSComp: dynamic cast failure\n");
	    jassertfalse;
	    return;
	}
    auto &ts = _proc->getTimbreSpace();
    ts.addActionListener(this);
    if (ts.isSavePending()){
        showAnalysisSaveDialog();
    }
}

void TimbreSpaceComponent::showAnalysisSaveDialog() {
    _proc->getTimbreSpace().setSavePending(false);
	const auto callback = new Callback(*this);
	
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
	// keep these out of the following scope as long as i want to reuse them for the navigator
	Rectangle<float> r_bounds = g.getClipBounds().toFloat();
	auto centre = r_bounds.getCentre();
	float radius = juce::jmax (r_bounds.getWidth(), r_bounds.getHeight()) * 0.5f;
	
	ColourGradient radialGrad (
		Colours::white.withAlpha(0.5f),    // centre colour
		centre,                        			 // centre point
		Colours::darkgrey.withAlpha(0.5f), // edge colour
		centre.translated (radius, 0),  		 // a point on the circumference
		true                           			 // isRadial = true
	);
	g.setGradientFill (radialGrad);
	g.fillRect (r_bounds);

    // static const auto snowColour = Colours::snow.withAlpha(0.2f);   // wish it could be constexpr
    static constexpr std::string_view snowColourStr {"33fffafa"};
	g.setColour(Colour::fromString(std::string(snowColourStr)));

	g.drawRect(getLocalBounds(), 1);
	
	auto const w = r_bounds.getWidth();
	auto const h = r_bounds.getHeight();
    {
	    if (tsn_mouse._dragging){
	        // draw orb around mouse
	        const auto uvz = tsn_mouse._uvz;
	        Colour c = p3ToColour(biuni(uvz));

	        Point<int> const xy = getMouseXYRelative();//.transformedBy(AffineTransform::verticalFlip(h));

	        const auto r = pointToRect(xy.toFloat(), 1.f);
	        const float  orbRadius = r.getWidth() * 4.5f;
	        ColourGradient gradient(c,
                                  xy.x, xy.y,
                                  c.withAlpha(0.f),
                                  xy.x, xy.y + orbRadius,
                                  true);
	        g.setGradientFill(gradient);
	        g.fillEllipse(xy.x - orbRadius, xy.y - orbRadius, orbRadius * 2, orbRadius * 2);
	    }
    }
    g.addTransform(AffineTransform::verticalFlip(h));
    {
        auto &timbreSpace = _proc->getTimbreSpace();
        auto const &timbres5D = timbreSpace.getTimbreSpace();
	    if (timbres5D.empty()) {
	        return;
	    }
        std::vector<Timbre5DPoint> current_points;
		current_points.reserve(timbreSpace.getCurrentPointIndices().size());
		for (auto & p : timbreSpace.getCurrentPointIndices()){
			current_points.push_back(timbres5D[p.idx]);
		}

	    setNavigatorPoint(get2D(timbreSpace.getTargetPoint()));
		
		for (const auto& p5 : timbres5D){
			const auto p2 = p2DtoJucePoint(bipolar2dPointToComponentSpace(get2D(p5), w, h));
			const auto p3 = get3D(p5);
			
			const auto uni_p3 = biuni(p3);
			Colour fillColour = p3ToColour(uni_p3);
			
			const float z_closeness = uni_p3[0] * uni_p3[1] * uni_p3[2] * 10.f;
			const auto rect = pointToRect(p2, softclip(z_closeness));
			
			if (containsValue(current_points, p5)){
				// brighter colour for selected point
				fillColour = fillColour.withMultipliedBrightness(1.75f).withMultipliedLightness(1.1f);
				// also draw glowing orb under the point
				const float orb_radius = rect.getWidth() * 1.5f;
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
		for (const auto &p : current_points){
			const auto center = p2DtoJucePoint(bipolar2dPointToComponentSpace(nav._p2D, w, h));
			const auto dest = p2DtoJucePoint(bipolar2dPointToComponentSpace(get2D(p), w, h));
			const auto l = Line(center, dest);
			const auto norm = [l, w, h](){
				const auto a = l.getLength() / std::sqrt( (w * w) + (h * h) );
				const auto b = 2.f * (0.5f - a);
				return jlimit(0.1f, 1.f, b * b * b + 0.1f);
			}();
			g.setColour(juce::Colours::whitesmoke.withAlpha(norm));
			g.drawLine(l, 1.f);
		}
	}
	// draw navigator
    {
        if (const auto& history = nav.getHistory();
            !history.empty())
        {
            // Collect all points
            std::vector<juce::Point<float>> points;
            points.push_back(p2DtoJucePoint(bipolar2dPointToComponentSpace(nav._p2D, w, h)));
            for (const auto& p : history) {
                points.push_back(p2DtoJucePoint(bipolar2dPointToComponentSpace(p, w, h)));
            }

            if (points.size() >= 2) {
                // Draw smooth Catmull-Rom curve segments directly
                int segmentIndex = 0;
                const size_t totalSegments = (points.size() - 1) * 10;  // 10 subdivisions per point pair

                for (size_t i = 0; i < points.size() - 1; ++i) {
                    auto p0 = (i > 0) ? points[i - 1] : points[i];
                    auto p1 = points[i];
                    auto p2 = points[i + 1];
                    auto p3 = (i + 2 < points.size()) ? points[i + 2] : p2;

                    // Subdivide segment for smoothness
                    constexpr int subdiv = 10;
                    for (int j = 0; j < subdiv; ++j) {
                        float t1 = static_cast<float>(j) / subdiv;
                        float t2 = static_cast<float>(j + 1) / subdiv;

                        // Calculate both points on Catmull-Rom curve
                        auto calcPoint = [&](float t) -> juce::Point<float> {
                            float t_sq = t * t;
                            float t_cube = t_sq * t;

                            float x = 0.5f * ((2.0f * p1.x) +
                                              (-p0.x + p2.x) * t +
                                              (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t_sq +
                                              (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t_cube);

                            float y = 0.5f * ((2.0f * p1.y) +
                                              (-p0.y + p2.y) * t +
                                              (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t_sq +
                                              (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t_cube);

                            return {x, y};
                        };

                        auto point1 = calcPoint(t1);
                        auto point2 = calcPoint(t2);

                        // Calculate fade based on position along entire trail
                        const float globalT = static_cast<float>(segmentIndex) / totalSegments;
                        const float alpha = std::pow(1.0f - globalT, 3.f);
                        const float thickness = 2.5f * std::pow(1.0f - globalT, 2.f);

                        g.setColour(juce::Colours::black.withAlpha(alpha));
                        g.drawLine(juce::Line<float>(point1, point2), thickness);

                        segmentIndex++;
                    }
                }
            }
        }

        // Draw current position
        g.setColour(Colours::black);
        g.fillEllipse(pointToRect(p2DtoJucePoint(bipolar2dPointToComponentSpace(nav._p2D, w, h)), 3.f));
    }
}

void TimbreSpaceComponent::mouseDragOrDown (juce::Point<int> mousePos) {
	tsn_mouse._dragging = true;
    mousePos = mousePos.transformedBy(AffineTransform::verticalFlip(static_cast<float>(getHeight())));
	const auto p2D_norm =  jucePointToTimbre2DPoint(normalizePosition_neg1_pos1(mousePos));
	auto &apvts = _proc->getAPVTS();
	setLfoOffsetParamsFromPoint(apvts, p2D_norm);
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
	const auto b = getLocalBounds();
	const auto proportionRect = juce::Rectangle<float> {0.1f, 0.05f,
														0.8f, 0.05f};
	auto progressBounds = b.getProportion(proportionRect);
	progressIndicator.setBounds(progressBounds);
}

void ProgressIndicator::paint(juce::Graphics &g) {
	const auto b = getLocalBounds();
	g.setColour(juce::Colours::whitesmoke);
	g.fillRect(b);
	g.setColour(juce::Colours::black);
	g.drawRect(b, 2.f);

	const auto partialW = static_cast<int>(b.getWidth() * progress);
	const auto progressBar = b.withWidth(partialW);
	g.fillRect(progressBar);
	
	g.setColour(juce::Colours::whitesmoke);
	g.setFont(juce::FontOptions("Courier New", 15.f, juce::Font::FontStyleFlags::plain));
	g.drawText(message, b, juce::Justification::centred);
}
void ProgressIndicator::resized() {}

void TimbreSpaceComponent::changeListenerCallback (juce::ChangeBroadcaster* source) {
	if (auto *rls = dynamic_cast<nvs::analysis::RunLoopStatus*>(source)) {
		std::cout << "timbre space comp: RunLoopStatus: CHANGE listener: SHOWING progress indicator\n";
		addAndMakeVisible(progressIndicator);
		progressIndicator.progress = rls->getProgress();
		progressIndicator.message = rls->getMessage();
		std::cout << "PROGRESS: " << progressIndicator.progress << '\n';
		progressIndicator.repaint();
	}
	else if (dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		std::cout << "timbre space comp: ThreadedAnalyzer: CHANGE listener: hiding progress indicator\n";
		progressIndicator.setVisible(false);
	}
}
void TimbreSpaceComponent::actionListenerCallback (const juce::String &message) {
	if (message == nvs::axiom::saveAnalysis) {
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
	const auto &ts = _proc->getTimbreSpace();
	return ts.getCurrentPointIndices();
}

juce::Point<float> TimbreSpaceComponent::normalizePosition_neg1_pos1(const juce::Point<int> pos) const {
	auto x = static_cast<float>(pos.getX());
	auto y = static_cast<float>(pos.getY());
	auto bounds = getLocalBounds().toFloat();
	x /= bounds.getWidth();
	y /= bounds.getHeight();
	using namespace nvs::memoryless;
	x = clamp(x, 0.0f, 1.0f);
	y = clamp(y, 0.0f, 1.0f);
	x = unibi(x);
	y = unibi(y);
	return {x, y};
}

void TimbreSpaceComponent::setNavigatorPoint(const Timbre2DPoint& p){
    nav.update(p);
}
ProgressIndicator& TimbreSpaceComponent::getProgressIndicator(){
	return progressIndicator;
}

void TimbreSpaceComponent::saveAnalysis(){
	auto analysesDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
		.getChildFile(nvs::axiom::tsn_granular)
		.getChildFile(nvs::axiom::Analyses);

    if (const auto result = analysesDir.createDirectory();
        result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Cannot Save Analysis",
            "Failed to create directory: " + result.getErrorMessage(),
            "OK",
            this);
        return;
    }
	auto absPath = _proc->getTimbreSpace().getAudioAbsolutePath();
	absPath = juce::File(absPath).getFileNameWithoutExtension();
	analysesDir = analysesDir.getChildFile(absPath);
	
	fileChooser = std::make_unique<juce::FileChooser>("Save Timbral Analysis",	//  const String &dialogBoxTitle,
															analysesDir,
															"*.tsb",	//  const String &filePatternsAllowed=String(),
															true,	// bool useOSNativeDialogBox
															false, 	// bool treatFilePackagesAsDirectories=false,
															this 	//  Component *parentComponent=nullptr
														   );

	const auto &timbreSpace = _proc->getTimbreSpace();
	const auto vt = timbreSpace.getTimbreSpaceTree();
	const auto par = vt.getParent();
	fmt::print("par: {}\n", par.getType().toString().toStdString());
	// Show async save dialog
	fileChooser->launchAsync
	(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
	 [this](const juce::FileChooser& fc)
	 {
		 if (auto file = fc.getResult(); file != juce::File{}) {
			// Ensure proper extension
			if (!file.hasFileExtension(".tsb")) {
				file = file.withFileExtension(".tsb");
			}
			_proc->saveAnalysisToFile(file.getFullPathName(), [this](bool success)
			{
				if (!success){
					juce::AlertWindow::showMessageBoxAsync(
						juce::AlertWindow::WarningIcon,	// iconType
						"Save Error",	// title
						"Could not save the timbral analysis file. Please check the file location and try again.",	// message
						"",	// buttonText
						this,	// associatedComponent
					    nullptr	// ModalComponentManager::Callback* callback
					);
				}
			});
		}
	});
}

TimbreSpaceComponent::Callback::Callback(TimbreSpaceComponent &comp)
: _ts_comp(comp)
{}
void TimbreSpaceComponent::Callback::modalStateFinished(const int choice) {
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
