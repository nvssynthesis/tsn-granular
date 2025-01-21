/*
  ==============================================================================

    TimbreSpaceComponent.cpp
    Created: 21 Jan 2025 1:29:17am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceComponent.h"

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

auto softclip = [](float const x, float const bias = -0.2f, float const q = 0.2f, float const s = 0.6f, float const scale = 5.f) -> float
{
	float const y = scale * ((q*x - bias) / (s + std::abs(q*x - bias))) + bias/2;
	return y;
};

int findNearestPoint(float x, float y, juce::Array<TimbreSpaceComponent::timbre5DPoint> timbres5D) {
	/*
	 Do consider that we are only using 2 dimenstions to find the nearest point even though they are of greater dimensionality.
	 */
	float diff = 1e15;
	int idx = 0;
	for (int i = 0; i < timbres5D.size(); ++i){
		auto const p2D = timbres5D[i].get2D();
		auto const xx = (p2D.getX() - x);
		auto const yy = (p2D.getY() - y);
		auto const tmp = xx*xx + yy*yy;
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
juce::Colour p3ToColour(timbre3DPoint const &p3, float alpha=1.f){
	return juce::Colour(p3[0], p3[1], p3[2], alpha);
}
}	// anonymous namespace

void TimbreSpaceComponent::paint(juce::Graphics &g) {
	g.fillAll(juce::Colour(juce::Colours::rebeccapurple).withMultipliedLightness(1.6f));

	juce::Rectangle<float> r = g.getClipBounds().toFloat();
	auto const w = r.getWidth();
	auto const h = r.getHeight();
	g.setColour(juce::Colours::blue);

	auto const current_point = timbres5D[currentPointIdx];
	
	for (auto p5 : timbres5D){
#pragma message("here i shall determine if p5 == current point, and do different things depending on if it does")
		timbre2DPoint p2 = p5.get2D();
		auto const p3 = p5.get3D();
		
		auto const uni_p3 = biuni(p3);
		juce::Colour fillColour = p3ToColour(uni_p3);
		
		if (p5 == current_point){
			fmt::print("current point: {}, {}, {}, {}, {}\n", p2.getX(), p2.getY(), p3[0], p3[1], p3[2]);
			fillColour = fillColour.withMultipliedBrightness(2.f).withMultipliedLightness(1.1f);
		}

		g.setColour(fillColour);
		p2 = transformFromZeroOrigin(p2);
		p2 *= timbre2DPoint(w,h);
		float const z_closeness = uni_p3[0] * uni_p3[1] * uni_p3[2] * 10.f;
		auto const rect = pointToRect(p2, softclip(z_closeness));
		g.fillEllipse(rect);
		g.setColour(fillColour.withRotatedHue(0.25f).withMultipliedLightness(2.f));
		g.drawEllipse(rect, 0.5f);
	}
}
void TimbreSpaceComponent::resized() {}

void TimbreSpaceComponent::setCurrentPointFromNearest(juce::Point<float> point, bool verbose) {
	float const x = point.getX();
	float const y = point.getY();
	if (verbose) {fmt::print("incoming point x: {}, y: {}\n", x, y);}
	currentPointIdx = findNearestPoint(x, y, timbres5D);
}
void TimbreSpaceComponent::mouseDown (const juce::MouseEvent &event) {
	auto const lastPointIdx = currentPointIdx;
	juce::Point<float> pNorm = normalizePosition_neg1_pos1(event.getMouseDownPosition());
	setCurrentPointFromNearest(pNorm);
	if (currentPointIdx != lastPointIdx){
		repaint();
	}
}
void TimbreSpaceComponent::mouseDrag(const juce::MouseEvent &event) {
	auto const lastPointIdx = currentPointIdx;
	juce::Point<float> pNorm = normalizePosition_neg1_pos1(event.getPosition());
	setCurrentPointFromNearest(pNorm);
	if (currentPointIdx != lastPointIdx){
		repaint();
	}
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
