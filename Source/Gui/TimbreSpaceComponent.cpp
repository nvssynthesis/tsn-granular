/*
  ==============================================================================

    TimbreSpaceComponent.cpp
    Created: 21 Jan 2025 1:29:17am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceComponent.h"
#include "../slicer_granular/Source/params.h"
#include "Analysis/ThreadedAnalyzer.h"

//===============================================timbre5DPoint=================================================
bool TimbreSpaceComponent::timbre5DPoint::operator==(timbre5DPoint const &other) const {
	if (other.get2D() != _p2D){
		return false;
	}
	auto const other3 = other.get3D();
	for (int i = 0; i < _p3D.size(); ++i){
		if (_p3D[i] != other3[i]){
			return false;
		}
	}
	return true;
}

std::array<juce::uint8, 3> TimbreSpaceComponent::timbre5DPoint::toUnsigned() const {
	for (auto p : _p3D){
		assert(inRangeM1_1(p));
	}
	using namespace nvs::memoryless;
	std::array<juce::uint8, 3> u {
		static_cast<juce::uint8>(biuni(_p3D[0]) * 255.f),
		static_cast<juce::uint8>(biuni(_p3D[1]) * 255.f),
		static_cast<juce::uint8>(biuni(_p3D[2]) * 255.f)
	};
	return u;
}

//============================================TimbreSpaceComponent=============================================
void TimbreSpaceComponent::add5DPoint(timbre2DPoint p2D, std::array<float, 3> p3D){
	assert(inRangeM1_1(p2D.getX()) && inRangeM1_1(p2D.getY())
		   && inRangeM1_1(p3D[0]) && inRangeM1_1(p3D[1]) && inRangeM1_1(p3D[2])
		   );
	timbres5D.add({
		._p2D{p2D},
		._p3D{p3D}
	});
}
void TimbreSpaceComponent::add5DPoint(float x, float y, float z, float w, float v){
	timbre2DPoint p2D(x,y);
	std::array<float, 3> p3D{z, w, v};
	add5DPoint(p2D, p3D);
}
void TimbreSpaceComponent::clear() {
	timbres5D.clear();
}
void TimbreSpaceComponent::add2DPoint(float x, float y){
	timbre2DPoint const p(x, y);
	add2DPoint(p);
}
void TimbreSpaceComponent::add2DPoint(timbre2DPoint p){
	assert(inRangeM1_1(p.getX()) && inRangeM1_1(p.getY()));
	timbres5D.add({
		._p2D{p},
		._p3D{1.f, 1.f, 1.f}
	});
}
namespace {
using timbre2DPoint = TimbreSpaceComponent::timbre2DPoint;
using timbre3DPoint = TimbreSpaceComponent::timbre3DPoint;
using timbre5DPoint = TimbreSpaceComponent::timbre5DPoint;

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

int findNearestPoint(timbre5DPoint p5D, juce::Array<TimbreSpaceComponent::timbre5DPoint> timbres5D, float higher_3D_weight = 0.01f) {
	/*
	 Do consider that we are only using 2 dimenstions to find the nearest point even though they are of greater dimensionality.
	 */

	auto const px = p5D.get2D().getX();
	auto const py = p5D.get2D().getY();
	auto const pu = p5D.get3D()[0];
	auto const pv = p5D.get3D()[1];
	auto const pz = p5D.get3D()[2];
	
	float diff = 1e15;
	int idx = 0;
	for (int i = 0; i < timbres5D.size(); ++i){
		auto const current_x = timbres5D[i].get2D().getX();
		auto const current_y = timbres5D[i].get2D().getY();
		auto const current_u = timbres5D[i].get3D()[0];
		auto const current_v = timbres5D[i].get3D()[1];
		auto const current_z = timbres5D[i].get3D()[2];

		auto const xx = (current_x - px);
		auto const yy = (current_y - py);
		auto const uu = (current_u - pu);
		auto const vv = (current_v - pv);
		auto const zz = (current_z - pz);

		auto const tmp = xx*xx + yy*yy + higher_3D_weight*(uu*uu + vv*vv + zz*zz);
		if (tmp < diff){
			diff = tmp;
			idx = i;
			if (diff == 0.f){
				goto findNearestPointDone;
			}
		}
	}
	findNearestPointDone:
	return idx;
}
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
	apvts.getParameterAsValue(getParamName(params_e::nav_lfo_2d_offset_x)) = p2D.getX();
	apvts.getParameterAsValue(getParamName(params_e::nav_lfo_2d_offset_y)) = p2D.getY();
}
}	// anonymous namespace

void TimbreSpaceComponent::paint(juce::Graphics &g) {
	g.fillAll(juce::Colour(juce::Colours::black).withMultipliedLightness(1.6f).withAlpha(0.f));
	g.setColour(juce::Colours::transparentWhite);
	g.drawRect(getLocalBounds(), 1);
			   
	// keep these out of the following scope as long as i want to reuse them for the navigator
	juce::Rectangle<float> r_bounds = g.getClipBounds().toFloat();
	auto const w = r_bounds.getWidth();
	auto const h = r_bounds.getHeight();
	{
		auto const current_point = timbres5D[currentPointIdx];
		
		for (auto p5 : timbres5D){
			timbre2DPoint p2 = bipolar2dPointToComponentSpace(p5.get2D(), w, h);
			auto const p3 = p5.get3D();
			
			auto const uni_p3 = biuni(p3);
			juce::Colour fillColour = p3ToColour(uni_p3);
			
			float const z_closeness = uni_p3[0] * uni_p3[1] * uni_p3[2] * 10.f;
			auto const rect = pointToRect(p2, softclip(z_closeness));
			
			if (p5 == current_point){
				// brighter colour for selected point
				fillColour = fillColour.withMultipliedBrightness(1.75f).withMultipliedLightness(1.1f);
				// also draw glowing orb under the point
				float orb_radius = rect.getWidth() * 1.5f;
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
	}
	// draw navigator
	{
		g.setColour(juce::Colours::black);
		g.fillEllipse(pointToRect(bipolar2dPointToComponentSpace(nav._p2D, w, h), 2.f));
	}
}

void TimbreSpaceComponent::setCurrentPointFromNearest(timbre5DPoint p5D, bool verbose) {
	if (verbose) {
		fmt::print("incoming point x: {}, y: {}, u: {}, v: {}, z: {}\n",
					p5D.get2D().getX(),
					p5D.get2D().getY(),
					p5D.get3D()[0],
					p5D.get3D()[1],
					p5D.get3D()[2]
				   );
	}
	currentPointIdx = findNearestPoint(p5D, timbres5D);
}

void TimbreSpaceComponent::setCurrentPointIdx(int newIdx){
	currentPointIdx = newIdx;
#pragma message("Deal with both position slider and LFO position choosing point")
	// get corresponding point to set x/y offsets
	// this leads to controls fighting each other. need to think of better way to deal with this.
//	auto const p5D = timbres5D[currentPointIdx];
//	setLfoOffsetParamsFromPoint(_apvts, p5D.get2D());
}



void TimbreSpaceComponent::mouseDragOrDown (juce::Point<int> mousePos) {
	tsn_mouse._dragging = true;
	auto const lastPointIdx = currentPointIdx;
	juce::Point<float> p2D_norm = normalizePosition_neg1_pos1(mousePos);
	timbre5DPoint p5D {
		._p2D{p2D_norm},
		._p3D{tsn_mouse._uvz[0], tsn_mouse._uvz[2], tsn_mouse._uvz[2]}
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
	g.setColour(juce::Colours::black);
	auto const b = getLocalBounds();
	g.drawRect(b, 2.f);
	
	int partialW = b.getWidth() * progress;
	auto progressBar = b.withWidth(partialW);
	g.fillRect(progressBar);
	
	g.setColour(juce::Colours::whitesmoke);
	g.setFont(juce::Font("Courier New", 15.f, juce::Font::FontStyleFlags::plain));
	g.drawText(message, b, juce::Justification::centred);
}
void ProgressIndicator::resized() {
	
}
void TimbreSpaceComponent::changeListenerCallback (juce::ChangeBroadcaster* source) {
	if (auto *a = dynamic_cast<nvs::analysis::RunLoopStatus*>(source)) {
		std::cout << "timbre space comp: RunLoopStatus: CHANGE listener: SHOWING progress indicator\n";
		addAndMakeVisible(progressIndicator);
		progressIndicator.progress = a->getProgress();
		progressIndicator.message = a->getMessage();
		std::cout << "PROGRESS: " << progressIndicator.progress << '\n';
		progressIndicator.repaint();
	}
	else if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		std::cout << "timbre space comp: ThreadedAnalyzer: CHANGE listener: hiding progress indicator\n";
		progressIndicator.setVisible(false);
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
	auto x3 = x0 + b.getWidth();

	auto y0 = b.getY();
	auto y1 = y0 + (0.86 * b.getHeight());
	auto y2 = y0 +  b.getHeight();
	
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

int TimbreSpaceComponent::getCurrentPointIdx() const {
	return currentPointIdx;
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
