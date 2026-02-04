/*
  ==============================================================================

    TimbreSpaceComponent.h
    Created: 28 Oct 2023 4:24:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../TsnGranularPluginProcessor.h"
#include "../TimbreSpace/TimbreSpace.h"
#include "ProgressIndicator.h"

/**
 TODO:
 -use legitimate 5D (or N-D) point class without mismatched smaller dimension subtypes. will need this to perform e.g. rotations
*/

class TimbreSpaceComponent final :	public Component
, 								public ChangeListener
, 								public Thread::Listener
,								private ActionListener
{
public:
	using Timbre2DPoint = nvs::timbrespace::Timbre2DPoint;
	using Timbre3DPoint = nvs::timbrespace::Timbre3DPoint;
	using TimbreSpace = nvs::timbrespace::TimbreSpace;

    explicit TimbreSpaceComponent(TSNGranularAudioProcessor& proc);
    ~TimbreSpaceComponent() override;
	//==========================================================================================
	void changeListenerCallback (ChangeBroadcaster* source) override;
	void actionListenerCallback (const String &message) override;
	void exitSignalSent() override;
	//==========================================================================================
	void paint(Graphics &g) override;
	void resized() override;
	void mouseDown (const MouseEvent &event) override;
	void mouseUp (const MouseEvent &event) override;
	void mouseDrag (const MouseEvent &event) override;
	void mouseDragOrDown(Point<int> mousePos);
	void mouseEnter(const MouseEvent &event) override;
	void mouseExit (const MouseEvent &event) override;
	void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;
	//==========================================================================================

	std::vector<nvs::timbrespace::WeightedIdx> getCurrentPointIndices() const;
	
	void setNavigatorPoint(const Timbre2DPoint& p);
	ProgressIndicator& getProgressIndicator();
	

private:
	TSNGranularAudioProcessor *_proc {nullptr};
	
	ProgressIndicator progressIndicator;
	
	struct TSNMouse
	{
		void createMouseImage();
		TSNMouse() {
			createMouseImage();
		}
		Timbre3DPoint _uvz {0.f, 0.6f, 0.f};
		Image _image;
		bool _dragging {false};
	};
	void updateCursor();
	
	TSNMouse tsn_mouse;
	
	class NavigatorPoint {
	    static constexpr size_t TRAIL_LENGTH {120};
	    std::deque<Timbre2DPoint> _history;
	public:
	    Timbre2DPoint _p2D;
        void update(const Timbre2DPoint &newPosition) {
            _history.push_front(_p2D);
            if (_history.size() > TRAIL_LENGTH) {
                _history.pop_back();
            }
            _p2D = newPosition;
        }

	    const std::deque<Timbre2DPoint>& getHistory() const {
            return _history;
        }
	} nav;
	
	void showAnalysisSaveDialog();
	class Callback final :	public ModalComponentManager::Callback
	{
	public:
        explicit Callback(TimbreSpaceComponent &comp);
	private:
		void modalStateFinished(int choice) override;
		TimbreSpaceComponent &_ts_comp;
	};

	void saveAnalysis();

	std::unique_ptr<FileChooser> fileChooser;
	Point<float> normalizePosition_neg1_pos1(Point<int> pos) const;

    class InfoBox {
        Rectangle<int> positionRect;
        const String numPointsBaseTxt {"num points: "};
        int _numPoints{0};
    public:
        void setNumPoints(const int numPoints) { _numPoints = numPoints; }
        void setBounds (Rectangle<int> bounds) {
            positionRect = bounds;
        }
        void paint(Graphics &g) const {
            g.setColour(Colours::grey.withAlpha(0.85f));
            const auto pRectF = positionRect.toFloat();
            // dont draw whole rectangle, just top vertical + left vertical.
            // otherwise there can be e.g. 2 vertical lines drawn 1 pixel apart probably due to roundoff.
            g.drawLine(pRectF.getX(), pRectF.getY(), pRectF.getRight(), pRectF.getY());
            g.drawLine(pRectF.getX(), pRectF.getY(), pRectF.getX(), pRectF.getBottom());

            const auto previousFont = g.getCurrentFont();
            const auto newFont = previousFont.withHeight(previousFont.getHeight() * 0.75f);
            g.setFont(newFont);
            auto text = numPointsBaseTxt + String(_numPoints);
            auto textWidth = GlyphArrangement::getStringWidth(newFont, text);

            const auto textRect = positionRect.withTrimmedRight(2);

            if (textWidth > textRect.getWidth()) {
                text = "num: " + std::to_string(_numPoints);
                textWidth = GlyphArrangement::getStringWidth(newFont, text);
                if (textWidth > textRect.getWidth()) {
                    text = std::to_string(_numPoints);
                }
            }
            g.drawText(text,
                        textRect,
                        Justification::topRight);
            g.setFont(previousFont);
        }
    } _infoBox;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent)
};
