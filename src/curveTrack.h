#ifndef curveTrack_h
#define curveTrack_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"
#include <algorithm>
#include <cmath>

struct CurveControlPoint {
	double beat;
	float value;
	
	bool operator<(const CurveControlPoint& other) const {
		return beat < other.beat;
	}
};

struct CurveTension {
	float inflection;  // p: 0.01 to 0.99
	float steepness;   // k: 0.1 to 5.0
	
	CurveTension() : inflection(0.5f), steepness(1.0f) {}
	CurveTension(float p, float k) : inflection(p), steepness(k) {}
};

class curveTrack : public ofxOceanodeNodeModel, public transportTrack {
public:
	curveTrack() : ofxOceanodeNodeModel("Curve Track") {
		color = ofColor(100, 180, 255);
	}

	~curveTrack() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
	}

	void setup() override {
		refreshTimelineList();
		addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
		addParameter(trackName.set("Track Name", "Curve " + ofToString(getNumIdentifier())));
		addParameter(numCurves.set("Num Curves", 1, 1, 8));
		
		addParameter(minValue.set("Min Value", 0.0f, -10.0f, 10.0f));
		addParameter(maxValue.set("Max Value", 1.0f, -10.0f, 10.0f));
		
		addOutputParameter(curveOutput.set("Curve[]", {0}, {-10}, {10}));
		
		// Update output range when min/max change
		listeners.push(minValue.newListener([this](float &val){
			updateOutputRange();
		}));
		listeners.push(maxValue.newListener([this](float &val){
			updateOutputRange();
		}));

		listeners.push(timelineSelect.newListener([this](int &val){ updateSubscription(); }));
		listeners.push(numCurves.newListener([this](int &val){
			resizeCurves(val);
		}));
		
		curveOutput.setSerializable(false);
		
		// Initialize with one curve with two default points
		resizeCurves(numCurves.get());
		updateOutputRange();
		
		updateSubscription();
	}

	void update(ofEventArgs &args) override {
		if(!currentTimeline) {
			vector<float> outputs(numCurves.get(), minValue.get());
			curveOutput = outputs;
			return;
		}
		
		double currentBeat = currentTimeline->getBeatPosition();
		vector<float> outputs;
		
		for(int curveIdx = 0; curveIdx < numCurves.get(); curveIdx++) {
			float value = evaluateCurveAt(currentBeat, curveIdx);
			// Map from [0,1] to [minValue, maxValue]
			float mappedValue = minValue.get() + value * (maxValue.get() - minValue.get());
			outputs.push_back(mappedValue);
		}
		
		curveOutput = outputs;
	}

	// --- TransportTrack Implementation ---
	std::string getTrackName() override {
		return trackName.get();
	}
	
	float getHeight() const override {
		return trackHeight;
	}
	
	void setHeight(float h) override {
		trackHeight = ofClamp(h, MIN_TRACK_HEIGHT, MAX_TRACK_HEIGHT);
	}
	
	bool isCollapsed() const override {
		return collapsed;
	}
	
	void setCollapsed(bool c) override {
		collapsed = c;
	}

	void drawInTimeline(ImDrawList* dl, ImVec2 pos, ImVec2 sz, double viewStart, double viewEnd) override {
		
		// 0. Draw tab bar if multiple curves
		float tabBarHeight = 0.0f;
		if(numCurves.get() > 1) {
			tabBarHeight = 25.0f;
			ImVec2 tabBarStart = ImGui::GetCursorScreenPos();
			
			// Tab buttons
			for(int i = 0; i < numCurves.get(); i++) {
				ImGui::PushID(i);
				
				char tabLabel[16];
				snprintf(tabLabel, 16, "Curve %d", i + 1);
				
				bool isThisTabActive = (i == activeCurve);
				
				if(isThisTabActive) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 0.8f, 1.0f));
				}
				
				if(ImGui::Button(tabLabel, ImVec2(70, 20))) {
					activeCurve = i;
				}
				
				if(isThisTabActive) {
					ImGui::PopStyleColor();
				}
				
				if(i < numCurves.get() - 1) ImGui::SameLine();
				ImGui::PopID();
			}
			
			// Adjust size for content area
			sz.y -= tabBarHeight;
		}
		
		// 1. Create interaction button
		std::string buttonId = "##trkBtn" + ofToString(getNumIdentifier());
		ImGui::InvisibleButton(buttonId.c_str(), sz);
		
		// 1.5 Get references to active curve data
		auto& activePoints = allCurvePoints[activeCurve];
		auto& activeTensions = allCurveTensions[activeCurve];
		
		// 2. Capture screen rect
		ImVec2 p = ImGui::GetItemRectMin();
		ImVec2 s = ImGui::GetItemRectSize();
		ImVec2 endP = ImGui::GetItemRectMax();
		
		// 3. Get interaction state
		ImVec2 mousePos = ImGui::GetMousePos();
		bool isHovered = ImGui::IsItemHovered();
		bool isLeftClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool isRightClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
		bool isDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
		bool isReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		bool isAltHeld = ImGui::GetIO().KeyAlt;
		
		// 4. Draw background
		dl->AddRectFilled(p, endP, IM_COL32(40, 40, 40, 255));
		dl->AddRect(p, endP, IM_COL32(60, 60, 60, 255));
		
		if(isHovered && !isAltHeld) {
			dl->AddRectFilled(p, endP, IM_COL32(255, 255, 255, 10));
		}
		
		// 5. Setup coordinate helpers
		double visibleLen = viewEnd - viewStart;
		if(visibleLen <= 0.001) return;
		
		int gridTicks = 0;
		double beatsPerBar = 4.0;
		double currentPlayheadBeat = 0.0;
		
		if(currentTimeline) {
			gridTicks = currentTimeline->getGridTicks();
			beatsPerBar = double(currentTimeline->getNumerator()) *
						  (4.0 / double(currentTimeline->getDenominator()));
			currentPlayheadBeat = currentTimeline->getBeatPosition();
		}
		
		auto beatToX = [&](double b) {
			return p.x + float((b - viewStart) / visibleLen) * s.x;
		};
		
		auto xToBeat = [&](float x) {
			return viewStart + ((x - p.x) / s.x) * visibleLen;
		};
		
		auto valueToY = [&](float v) {
			return p.y + (1.0f - v) * s.y; // Inverted: 1.0 at top, 0.0 at bottom
		};
		
		auto yToValue = [&](float y) {
			return 1.0f - ((y - p.y) / s.y);
		};
		
		auto snap = [&](double b) {
			if(gridTicks <= 0) return b;
			double gridBeats = gridTicks / 24.0;
			return std::round(b / gridBeats) * gridBeats;
		};
		
		// 6. Draw grid lines
		int viewStartBar = int(viewStart / beatsPerBar);
		int viewEndBar = int(viewEnd / beatsPerBar) + 1;
		
		for(int bar = viewStartBar; bar <= viewEndBar; ++bar) {
			double barBeat = bar * beatsPerBar;
			float barX = beatToX(barBeat);
			
			if(barX < p.x - 5 || barX > endP.x + 5) continue;
			
			dl->AddLine(
				ImVec2(barX, p.y),
				ImVec2(barX, endP.y),
				IM_COL32(120, 120, 120, 255),
				2.0f
			);
			
			if(gridTicks > 0 && bar < viewEndBar) {
				double gridBeats = gridTicks / 24.0;
				double nextBarBeat = (bar + 1) * beatsPerBar;
				
				for(double b = barBeat + gridBeats; b < nextBarBeat; b += gridBeats) {
					if(b < viewStart || b > viewEnd) continue;
					
					float gridX = beatToX(b);
					dl->AddLine(
						ImVec2(gridX, p.y),
						ImVec2(gridX, endP.y),
						IM_COL32(70, 70, 70, 100),
						0.5f
					);
				}
			}
		}
		
		// Horizontal value grid lines
		for(int i = 0; i <= 4; i++) {
			float val = i / 4.0f;
			float yPos = valueToY(val);
			dl->AddLine(
				ImVec2(p.x, yPos),
				ImVec2(endP.x, yPos),
				IM_COL32(80, 80, 80, 100),
				1.0f
			);
			
			// Value labels
			char buf[16];
			snprintf(buf, 16, "%.2f", minValue.get() + val * (maxValue.get() - minValue.get()));
			dl->AddText(ImVec2(p.x + 2, yPos - 8), IM_COL32(150, 150, 150, 200), buf);
		}
		
		// 6.5 Draw loop region
		if(currentTimeline) {
			double loopStart, loopEnd;
			bool loopEnabled = false;
			
			if(getLoopRegion(loopStart, loopEnd, loopEnabled) && loopEnabled) {
				float lx1 = beatToX(loopStart);
				float lx2 = beatToX(loopEnd);
				
				lx1 = std::max(lx1, p.x);
				lx2 = std::min(lx2, endP.x);
				
				dl->AddRectFilled(
					ImVec2(lx1, p.y),
					ImVec2(lx2, endP.y),
					IM_COL32(80, 80, 160, 50)
				);
				
				dl->AddLine(
					ImVec2(lx1, p.y),
					ImVec2(lx1, endP.y),
					IM_COL32(160, 160, 255, 180),
					2.0f
				);
				dl->AddLine(
					ImVec2(lx2, p.y),
					ImVec2(lx2, endP.y),
					IM_COL32(160, 160, 255, 180),
					2.0f
				);
			}
		}
		
		// 7. Draw curve segments
		// First pass: Draw inactive curves (dimmed)
		for(int curveIdx = 0; curveIdx < numCurves.get(); curveIdx++) {
			if(curveIdx == activeCurve) continue; // Skip active curve for now
			
			auto& curvePoints = allCurvePoints[curveIdx];
			auto& curveTensions = allCurveTensions[curveIdx];
			
			ImU32 curveColor = getCurveColor(curveIdx, 50); // Dimmed
			
			for(size_t i = 0; i < curvePoints.size() - 1; i++) {
				CurveControlPoint& pt1 = curvePoints[i];
				CurveControlPoint& pt2 = curvePoints[i + 1];
				CurveTension& tension = curveTensions[i];
				
				float x1 = beatToX(pt1.beat);
				float x2 = beatToX(pt2.beat);
				
				if(x2 < p.x || x1 > endP.x) continue;
				
				int pixelWidth = std::abs(x2 - x1);
				int numSamples = std::max(50, pixelWidth * 2);
				
				for(int j = 0; j < numSamples; j++) {
					float t1 = j / (float)numSamples;
					float t2 = (j + 1) / (float)numSamples;
					
					float val1 = evaluateSegment(pt1, pt2, tension, t1);
					float val2 = evaluateSegment(pt1, pt2, tension, t2);
					
					float sx1 = beatToX(pt1.beat + t1 * (pt2.beat - pt1.beat));
					float sy1 = valueToY(val1);
					float sx2 = beatToX(pt1.beat + t2 * (pt2.beat - pt1.beat));
					float sy2 = valueToY(val2);
					
					dl->AddLine(
						ImVec2(sx1, sy1),
						ImVec2(sx2, sy2),
						curveColor,
						1.0f
					);
				}
			}
		}
		
		// Second pass: Draw active curve (bright)
		ImU32 activeCurveColor = getCurveColor(activeCurve, 255); // Full opacity
		
		for(size_t i = 0; i < activePoints.size() - 1; i++) {
			CurveControlPoint& pt1 = activePoints[i];
			CurveControlPoint& pt2 = activePoints[i + 1];
			CurveTension& tension = activeTensions[i];
			
			float x1 = beatToX(pt1.beat);
			float x2 = beatToX(pt2.beat);
			
			// Skip if completely outside view
			if(x2 < p.x || x1 > endP.x) continue;
			
			// Increase sampling density significantly, especially near control points
			// Use at least 50 samples or 2 samples per pixel
			int pixelWidth = std::abs(x2 - x1);
			int numSamples = std::max(50, pixelWidth * 2);
			
			for(int j = 0; j < numSamples; j++) {
				float t1 = j / (float)numSamples;
				float t2 = (j + 1) / (float)numSamples;
				
				float val1 = evaluateSegment(pt1, pt2, tension, t1);
				float val2 = evaluateSegment(pt1, pt2, tension, t2);
				
				float sx1 = beatToX(pt1.beat + t1 * (pt2.beat - pt1.beat));
				float sy1 = valueToY(val1);
				float sx2 = beatToX(pt1.beat + t2 * (pt2.beat - pt1.beat));
				float sy2 = valueToY(val2);
				
				dl->AddLine(
					ImVec2(sx1, sy1),
					ImVec2(sx2, sy2),
					activeCurveColor,
					2.0f
				);
			}
		}
		
		// 8. Draw points (only for active curve)
		for(size_t i = 0; i < activePoints.size(); i++) {
			CurveControlPoint& pt = activePoints[i];
			float px = beatToX(pt.beat);
			float py = valueToY(pt.value);
			
			if(px < p.x - 20 || px > endP.x + 20) continue;
			
			bool isPointHovered = (std::abs(mousePos.x - px) < 8 && std::abs(mousePos.y - py) < 8);
			float radius = (isPointHovered || (int)i == selectedPoint) ? 6.0f : 4.0f;
			
			ImU32 pointColor = (int)i == selectedPoint ?
				IM_COL32(255, 200, 100, 255) :
				IM_COL32(255, 255, 255, 255);
			
			dl->AddCircleFilled(ImVec2(px, py), radius, pointColor);
			dl->AddCircle(ImVec2(px, py), radius, IM_COL32(50, 50, 50, 255), 12, 1.5f);
		}
		
		// 9. Draw playhead with all curve values
		float playheadX = beatToX(currentPlayheadBeat);
		if(playheadX >= p.x && playheadX <= endP.x) {
			dl->AddLine(
				ImVec2(playheadX, p.y),
				ImVec2(playheadX, endP.y),
				IM_COL32(255, 80, 80, 255),
				2.5f
			);
			
			// Draw value indicators for all curves
			for(int curveIdx = 0; curveIdx < numCurves.get(); curveIdx++) {
				float currentValue = evaluateCurveAt(currentPlayheadBeat, curveIdx);
				float currentY = valueToY(currentValue);
				
				if(curveIdx == activeCurve) {
					// Active curve: bright indicator
					dl->AddCircleFilled(
						ImVec2(playheadX, currentY),
						5.0f,
						IM_COL32(255, 80, 80, 255)
					);
				} else {
					// Inactive curves: dimmed indicator
					dl->AddCircleFilled(
						ImVec2(playheadX, currentY),
						3.0f,
						IM_COL32(255, 80, 80, 100)
					);
				}
			}
		}
		
		// 10. Handle interactions (on active curve only)
		if(isLeftClick) {
			// Check if clicking on a point
			selectedPoint = -1;
			for(size_t i = 0; i < activePoints.size(); i++) {
				float px = beatToX(activePoints[i].beat);
				float py = valueToY(activePoints[i].value);
				
				if(std::abs(mousePos.x - px) < 8 && std::abs(mousePos.y - py) < 8) {
					selectedPoint = i;
					isDraggingPoint = true;
					break;
				}
			}
			
			// If not clicking on point and Alt not held, add new point
			if(selectedPoint == -1 && !isAltHeld) {
				double newBeat = snap(xToBeat(mousePos.x));
				float newValue = ofClamp(yToValue(mousePos.y), 0.0f, 1.0f);
				
				// Add point and keep sorted
				CurveControlPoint newPt;
				newPt.beat = newBeat;
				newPt.value = newValue;
				activePoints.push_back(newPt);
				std::sort(activePoints.begin(), activePoints.end());
				
				// Rebuild tensions
				rebuildTensions(activeCurve);
			}
			
			// If Alt held, check if clicking on a curve segment
			if(isAltHeld) {
				selectedSegment = findClosestSegment(mousePos, p, beatToX, valueToY, activeCurve);
				if(selectedSegment >= 0) {
					isDraggingTension = true;
					dragStartMouse = mousePos;
					dragStartInflection = activeTensions[selectedSegment].inflection;
					dragStartSteepness = activeTensions[selectedSegment].steepness;
				}
			}
		}
		
		if(isDragging) {
			if(isDraggingPoint && selectedPoint >= 0 && selectedPoint < activePoints.size()) {
				// Drag point
				double newBeat = snap(xToBeat(mousePos.x));
				float newValue = ofClamp(yToValue(mousePos.y), 0.0f, 1.0f);
				
				activePoints[selectedPoint].beat = newBeat;
				activePoints[selectedPoint].value = newValue;
				
				// Re-sort if needed
				std::sort(activePoints.begin(), activePoints.end());
				rebuildTensions(activeCurve);
			}
			else if(isDraggingTension && selectedSegment >= 0) {
				// Drag tension
				float deltaX = mousePos.x - dragStartMouse.x;
				float deltaY = mousePos.y - dragStartMouse.y;
				
				// X controls inflection (normalized to reasonable range)
				float inflectionDelta = deltaX / s.x;
				activeTensions[selectedSegment].inflection = ofClamp(
					dragStartInflection + inflectionDelta,
					0.01f, 0.99f
				);
				
				// Y controls steepness (inverted: up = steeper)
				// Use exponential scaling for better control across the range
				float steepnessDelta = -deltaY / (s.y / 3.0f); // Adjusted sensitivity
				
				// Apply delta with exponential scaling for better feel
				float newSteepness = dragStartSteepness * exp(steepnessDelta * 0.5f);
				
				activeTensions[selectedSegment].steepness = ofClamp(
					newSteepness,
					0.05f, 10.0f  // Wider range with better distribution
				);
			}
		}
		
		if(isReleased) {
			isDraggingPoint = false;
			isDraggingTension = false;
			selectedSegment = -1;
		}
		
		if(isRightClick) {
			// Delete point under cursor
			for(size_t i = 0; i < activePoints.size(); i++) {
				float px = beatToX(activePoints[i].beat);
				float py = valueToY(activePoints[i].value);
				
				if(std::abs(mousePos.x - px) < 8 && std::abs(mousePos.y - py) < 8) {
					if(activePoints.size() > 2) { // Keep at least 2 points
						activePoints.erase(activePoints.begin() + i);
						rebuildTensions(activeCurve);
					}
					break;
				}
			}
		}
		
		// Visual feedback for alt+drag mode
		if(isAltHeld && isHovered) {
			if(isDraggingTension && selectedSegment >= 0) {
				// Show current tension values while dragging
				char tensionInfo[64];
				snprintf(tensionInfo, 64, "Inflection: %.2f  Steepness: %.2f",
					activeTensions[selectedSegment].inflection,
					activeTensions[selectedSegment].steepness);
				dl->AddText(
					ImVec2(mousePos.x + 10, mousePos.y - 20),
					IM_COL32(255, 255, 150, 255),
					tensionInfo
				);
			} else {
				int hoverSegment = findClosestSegment(mousePos, p, beatToX, valueToY, activeCurve);
				if(hoverSegment >= 0) {
					// Show hint and current values
					char tensionInfo[64];
					snprintf(tensionInfo, 64, "Alt+Drag: X=Inflection Y=Steepness\nCurrent: p=%.2f k=%.2f",
						activeTensions[hoverSegment].inflection,
						activeTensions[hoverSegment].steepness);
					dl->AddText(
						ImVec2(mousePos.x + 10, mousePos.y),
						IM_COL32(255, 255, 150, 255),
						tensionInfo
					);
				}
			}
		}
	}
	

	// --- Preset Saving ---
	void presetSave(ofJson &json) override {
		// Save all curves
		std::vector<std::vector<std::vector<float>>> allCurvesPoints;
		for(auto &curvePoints : allCurvePoints) {
			std::vector<std::vector<float>> pointsList;
			for(auto &pt : curvePoints) {
				pointsList.push_back({(float)pt.beat, pt.value});
			}
			allCurvesPoints.push_back(pointsList);
		}
		json["allCurvePoints"] = allCurvesPoints;
		
		std::vector<std::vector<std::vector<float>>> allCurvesTensions;
		for(auto &curveTensions : allCurveTensions) {
			std::vector<std::vector<float>> tensionsList;
			for(auto &t : curveTensions) {
				tensionsList.push_back({t.inflection, t.steepness});
			}
			allCurvesTensions.push_back(tensionsList);
		}
		json["allCurveTensions"] = allCurvesTensions;
		
		json["trackHeight"] = trackHeight;
		json["collapsed"] = collapsed;
		json["numCurves"] = numCurves.get();
		json["activeCurve"] = activeCurve;
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("numCurves") > 0) {
			numCurves = json["numCurves"];
			resizeCurves(numCurves.get());
		}
		
		if(json.count("allCurvePoints") > 0) {
			allCurvePoints.clear();
			for(auto &curveData : json["allCurvePoints"]) {
				std::vector<CurveControlPoint> curvePoints;
				for(auto &ptData : curveData) {
					if(ptData.size() >= 2) {
						CurveControlPoint pt;
						pt.beat = ptData[0];
						pt.value = ptData[1];
						curvePoints.push_back(pt);
					}
				}
				allCurvePoints.push_back(curvePoints);
			}
		}
		
		if(json.count("allCurveTensions") > 0) {
			allCurveTensions.clear();
			for(auto &curveData : json["allCurveTensions"]) {
				std::vector<CurveTension> curveTensions;
				for(auto &tData : curveData) {
					if(tData.size() >= 2) {
						curveTensions.push_back(CurveTension(tData[0], tData[1]));
					}
				}
				allCurveTensions.push_back(curveTensions);
			}
		}
		
		if(json.count("trackHeight") > 0) {
			trackHeight = json["trackHeight"];
			trackHeight = ofClamp(trackHeight, MIN_TRACK_HEIGHT, MAX_TRACK_HEIGHT);
		}
		
		if(json.count("collapsed") > 0) {
			collapsed = json["collapsed"];
		}
		
		if(json.count("activeCurve") > 0) {
			activeCurve = json["activeCurve"];
			activeCurve = ofClamp(activeCurve, 0, numCurves.get() - 1);
		}
	}

private:
	ofParameter<int> timelineSelect;
	ofParameter<std::string> trackName;
	ofParameter<int> numCurves;
	ofParameter<float> minValue;
	ofParameter<float> maxValue;
	ofParameter<vector<float>> curveOutput;

	ppqTimeline* currentTimeline = nullptr;
	
	// Multiple curves data
	std::vector<std::vector<CurveControlPoint>> allCurvePoints;
	std::vector<std::vector<CurveTension>> allCurveTensions;
	int activeCurve = 0;
	
	std::vector<std::string> timelineOptions;
	
	ofEventListeners listeners;
	
	// Interaction state
	int selectedPoint = -1;
	int selectedSegment = -1;
	bool isDraggingPoint = false;
	bool isDraggingTension = false;
	ImVec2 dragStartMouse;
	float dragStartInflection = 0.5f;
	float dragStartSteepness = 1.0f;
	
	// Track height management
	float trackHeight = 100.0f;
	bool collapsed = false;
	static constexpr float MIN_TRACK_HEIGHT = 60.0f;
	static constexpr float MAX_TRACK_HEIGHT = 400.0f;

	void refreshTimelineList() {
		timelineOptions.clear();
		timelineOptions.push_back("None");
		for(auto* tl : ppqTimeline::getTimelines()) {
			timelineOptions.push_back("Timeline " + ofToString(tl->getNumIdentifier()));
		}
		timelineSelect.set("Timeline", 0, 0, (int)timelineOptions.size()-1);
	}

	void updateSubscription() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
		int idx = timelineSelect.get() - 1;
		auto& tls = ppqTimeline::getTimelines();
		if(idx >= 0 && idx < tls.size()) {
			currentTimeline = tls[idx];
			currentTimeline->subscribeTrack(this);
		} else {
			currentTimeline = nullptr;
		}
	}
	
	bool getLoopRegion(double& loopStart, double& loopEnd, bool& enabled) {
		if(!currentTimeline) return false;
		
		enabled = currentTimeline->isLoopEnabled();
		loopStart = currentTimeline->getLoopStart();
		loopEnd = currentTimeline->getLoopEnd();
		
		return true;
	}
	
	// Improved sigmoid function with smooth transition and better edge handling
	float sigmoidFlex(float x, float p, float k) {
		// Clamp inputs
		x = ofClamp(x, 0.0f, 1.0f);
		p = ofClamp(p, 0.01f, 0.99f);
		k = ofClamp(k, 0.05f, 10.0f);
		
		// Handle edge cases with small epsilon to avoid numerical issues
		const float epsilon = 0.0001f;
		if(x < epsilon) return 0.0f;
		if(x > 1.0f - epsilon) return 1.0f;
		
		// Ensure we don't divide by values too close to 0
		float xSafe = ofClamp(x, epsilon, 1.0f - epsilon);
		float pSafe = ofClamp(p, epsilon, 1.0f - epsilon);
		
		// Standard sigmoid formula with safe values
		float a = pow(xSafe / pSafe, k);
		float b = pow((1.0f - xSafe) / (1.0f - pSafe), k);
		
		// Avoid division by very small numbers
		float denominator = a + b;
		if(denominator < epsilon) return 0.5f;
		
		return a / denominator;
	}
	
	// Evaluate curve segment between two points
	float evaluateSegment(const CurveControlPoint& p1, const CurveControlPoint& p2, const CurveTension& tension, float t) {
		// t is normalized [0,1] position between p1 and p2
		float curveValue = sigmoidFlex(t, tension.inflection, tension.steepness);
		
		// Interpolate between p1.value and p2.value using the curve
		return p1.value + curveValue * (p2.value - p1.value);
	}
	
	// Evaluate entire curve at a specific beat for a given curve index
	float evaluateCurveAt(double beat, int curveIdx) {
		if(curveIdx < 0 || curveIdx >= allCurvePoints.size()) return 0.0f;
		
		auto& points = allCurvePoints[curveIdx];
		auto& tensions = allCurveTensions[curveIdx];
		
		if(points.empty()) return 0.0f;
		if(points.size() == 1) return points[0].value;
		
		// Find which segment the beat falls in
		if(beat <= points.front().beat) return points.front().value;
		if(beat >= points.back().beat) return points.back().value;
		
		for(size_t i = 0; i < points.size() - 1; i++) {
			if(beat >= points[i].beat && beat <= points[i + 1].beat) {
				// Found the segment
				double segmentLength = points[i + 1].beat - points[i].beat;
				if(segmentLength < 0.001) return points[i].value;
				
				float t = (beat - points[i].beat) / segmentLength;
				return evaluateSegment(points[i], points[i + 1], tensions[i], t);
			}
		}
		
		return points.back().value;
	}
	
	// Rebuild tensions array when points change for a specific curve
	void rebuildTensions(int curveIdx) {
		if(curveIdx < 0 || curveIdx >= allCurvePoints.size()) return;
		
		auto& points = allCurvePoints[curveIdx];
		auto& tensions = allCurveTensions[curveIdx];
		
		size_t neededTensions = points.size() > 0 ? points.size() - 1 : 0;
		
		// Keep existing tensions where possible, add defaults for new segments
		while(tensions.size() < neededTensions) {
			tensions.push_back(CurveTension());
		}
		while(tensions.size() > neededTensions) {
			tensions.pop_back();
		}
	}
	
	// Find closest curve segment to mouse position for a specific curve
	int findClosestSegment(ImVec2 mouse, ImVec2 trackPos,
						   std::function<float(double)> beatToX,
						   std::function<float(float)> valueToY,
						   int curveIdx) {
		if(curveIdx < 0 || curveIdx >= allCurvePoints.size()) return -1;
		
		auto& points = allCurvePoints[curveIdx];
		auto& tensions = allCurveTensions[curveIdx];
		
		float minDist = 15.0f; // Maximum distance to consider
		int closestSeg = -1;
		
		for(size_t i = 0; i < points.size() - 1; i++) {
			// Sample the segment and find closest point
			CurveControlPoint& pt1 = points[i];
			CurveControlPoint& pt2 = points[i + 1];
			CurveTension& tension = tensions[i];
			
			int numSamples = 20;
			for(int j = 0; j <= numSamples; j++) {
				float t = j / (float)numSamples;
				float val = evaluateSegment(pt1, pt2, tension, t);
				double beat = pt1.beat + t * (pt2.beat - pt1.beat);
				
				float sx = beatToX(beat);
				float sy = valueToY(val);
				
				float dist = std::sqrt(
					(mouse.x - sx) * (mouse.x - sx) +
					(mouse.y - sy) * (mouse.y - sy)
				);
				
				if(dist < minDist) {
					minDist = dist;
					closestSeg = i;
				}
			}
		}
		
		return closestSeg;
	}
	
	// Resize curves when parameter changes
	void resizeCurves(int newNumCurves) {
		// Resize curve data vectors
		while(allCurvePoints.size() < newNumCurves) {
			// Add new curve with default points
			std::vector<CurveControlPoint> newCurve;
			CurveControlPoint p1, p2;
			p1.beat = 0.0;
			p1.value = 0.0f;
			p2.beat = 16.0;
			p2.value = 1.0f;
			newCurve.push_back(p1);
			newCurve.push_back(p2);
			allCurvePoints.push_back(newCurve);
			
			// Add default tension
			std::vector<CurveTension> newTensions;
			newTensions.push_back(CurveTension());
			allCurveTensions.push_back(newTensions);
		}
		
		while(allCurvePoints.size() > newNumCurves) {
			allCurvePoints.pop_back();
			allCurveTensions.pop_back();
		}
		
		// Clamp active curve
		if(activeCurve >= newNumCurves) {
			activeCurve = newNumCurves - 1;
		}
		if(activeCurve < 0) activeCurve = 0;
		
		// Update output range for new number of curves
		updateOutputRange();
	}
	
	// Get color for a specific curve index
	ImU32 getCurveColor(int curveIdx, uint8_t alpha) {
		// Color palette for curves (similar to multiGateTrack but adjusted for curves)
		static const ImVec4 curveColors[] = {
			ImVec4(100.0f/255, 180.0f/255, 255.0f/255, 1.0f),  // Blue (original)
			ImVec4(255.0f/255, 100.0f/255, 100.0f/255, 1.0f),  // Red
			ImVec4(100.0f/255, 255.0f/255, 100.0f/255, 1.0f),  // Green
			ImVec4(255.0f/255, 200.0f/255, 100.0f/255, 1.0f),  // Orange
			ImVec4(200.0f/255, 100.0f/255, 255.0f/255, 1.0f),  // Purple
			ImVec4(100.0f/255, 255.0f/255, 255.0f/255, 1.0f),  // Cyan
			ImVec4(255.0f/255, 255.0f/255, 100.0f/255, 1.0f),  // Yellow
			ImVec4(255.0f/255, 100.0f/255, 200.0f/255, 1.0f),  // Pink
		};
		
		int colorIdx = curveIdx % 8;
		ImVec4 color = curveColors[colorIdx];
		
		return IM_COL32(
			(uint8_t)(color.x * 255),
			(uint8_t)(color.y * 255),
			(uint8_t)(color.z * 255),
			alpha
		);
	}
	
	// Update output parameter range
	void updateOutputRange() {
		// Create a vector with proper min/max
		vector<float> minVals(numCurves.get(), minValue.get());
		vector<float> maxVals(numCurves.get(), maxValue.get());
		curveOutput.setMin(minVals);
		curveOutput.setMax(maxVals);
	}
};

#endif
