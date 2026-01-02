#ifndef sigmoidCurve_h
#define sigmoidCurve_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

struct SigmoidControlPoint {
	double position;  // 0.0 to 1.0 (normalized position in curve)
	float value;
	
	bool operator<(const SigmoidControlPoint& other) const {
		return position < other.position;
	}
};

struct SigmoidTension {
	float inflection;  // p: 0.01 to 0.99
	float steepness;   // k: 0.1 to 5.0
	
	SigmoidTension() : inflection(0.5f), steepness(1.0f) {}
	SigmoidTension(float p, float k) : inflection(p), steepness(k) {}
};

class sigmoidCurve : public ofxOceanodeNodeModel {
public:
	sigmoidCurve() : ofxOceanodeNodeModel("Sigmoid Curve") {
		color = ofColor(100, 180, 255);
	}

	void setup() override {
		addParameter(phasorInput.set("Phasor", {0}, {0}, {1}));
		addParameter(numCurves.set("Num Curves", 1, 1, 8));
		addParameter(gridDivisions.set("Grid Div", 16, 1, 64));
		
		addParameter(minValue.set("Min Value", 0.0f, -10.0f, 10.0f));
		addParameter(maxValue.set("Max Value", 1.0f, -10.0f, 10.0f));
		
		addParameter(showEditor.set("Show Editor", false));
		
		addOutputParameter(curveOutput.set("Curve[]", {0}, {-10}, {10}));
		
		// Update output range when min/max change
		listeners.push(minValue.newListener([this](float &val){
			updateOutputRange();
		}));
		listeners.push(maxValue.newListener([this](float &val){
			updateOutputRange();
		}));
		listeners.push(numCurves.newListener([this](int &val){
			resizeCurves(val);
		}));
		listeners.push(phasorInput.newListener([this](vector<float> &val){
			updateCurveOutput();
		}));
		
		curveOutput.setSerializable(false);
		
		// Initialize with one curve with two default points
		resizeCurves(numCurves.get());
		updateOutputRange();
	}

	void update(ofEventArgs &args) override {
		// Output is updated via phasor listener
	}
	
	void draw(ofEventArgs &e) override {
		if(!showEditor.get()) return;
		
		ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
		
		std::string windowName = "Curve Editor##" + ofToString(getNumIdentifier());
		bool show = showEditor.get();
		
		if(ImGui::Begin(windowName.c_str(), &show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			drawCurveEditor();
		}
		ImGui::End();
		
		if(show != showEditor.get()) showEditor = show;
	}

private:
	ofParameter<vector<float>> phasorInput;
	ofParameter<int> numCurves;
	ofParameter<int> gridDivisions;
	ofParameter<float> minValue;
	ofParameter<float> maxValue;
	ofParameter<bool> showEditor;
	ofParameter<vector<float>> curveOutput;
	
	// Multiple curves data
	std::vector<std::vector<SigmoidControlPoint>> allCurvePoints;
	std::vector<std::vector<SigmoidTension>> allSigmoidTensions;
	int activeCurve = 0;
	
	ofEventListeners listeners;
	
	// Interaction state
	int selectedPoint = -1;
	int selectedSegment = -1;
	bool isDraggingPoint = false;
	bool isDraggingTension = false;
	ImVec2 dragStartMouse;
	float dragStartInflection = 0.5f;
	float dragStartSteepness = 1.0f;

	void drawCurveEditor() {
		// Tab bar if multiple curves
		if(numCurves.get() > 1) {
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
		}
		
		// Get available space
		ImVec2 avail = ImGui::GetContentRegionAvail();
		
		// Create interaction button
		std::string buttonId = "##curveEdit" + ofToString(getNumIdentifier());
		ImGui::InvisibleButton(buttonId.c_str(), avail);
		
		// Get references to active curve data
		auto& activePoints = allCurvePoints[activeCurve];
		auto& activeTensions = allSigmoidTensions[activeCurve];
		
		// Capture screen rect
		ImVec2 p = ImGui::GetItemRectMin();
		ImVec2 s = ImGui::GetItemRectSize();
		ImVec2 endP = ImGui::GetItemRectMax();
		
		ImDrawList* dl = ImGui::GetWindowDrawList();
		
		// Get interaction state
		ImVec2 mousePos = ImGui::GetMousePos();
		bool isHovered = ImGui::IsItemHovered();
		bool isLeftClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool isRightClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
		bool isDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
		bool isReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		bool isAltHeld = ImGui::GetIO().KeyAlt;
		
		// Draw background
		dl->AddRectFilled(p, endP, IM_COL32(40, 40, 40, 255));
		dl->AddRect(p, endP, IM_COL32(60, 60, 60, 255));
		
		if(isHovered && !isAltHeld) {
			dl->AddRectFilled(p, endP, IM_COL32(255, 255, 255, 10));
		}
		
		// Coordinate helpers
		auto posToX = [&](double pos) {
			return p.x + float(pos) * s.x;
		};
		
		auto xToPos = [&](float x) {
			return double((x - p.x) / s.x);
		};
		
		auto valueToY = [&](float v) {
			return p.y + (1.0f - v) * s.y;
		};
		
		auto yToValue = [&](float y) {
			return 1.0f - ((y - p.y) / s.y);
		};
		
		auto snap = [&](double pos) {
			int divs = gridDivisions.get();
			if(divs <= 1) return pos;
			double snapInterval = 1.0 / double(divs);
			return std::round(pos / snapInterval) * snapInterval;
		};
		
		// Draw grid lines
		int divs = gridDivisions.get();
		for(int i = 0; i <= divs; i++) {
			double gridPos = i / double(divs);
			float gridX = posToX(gridPos);
			
			bool isMajor = (i % 4 == 0);
			
			dl->AddLine(
				ImVec2(gridX, p.y),
				ImVec2(gridX, endP.y),
				isMajor ? IM_COL32(120, 120, 120, 255) : IM_COL32(70, 70, 70, 100),
				isMajor ? 2.0f : 0.5f
			);
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
		
		// Draw curve segments - inactive curves first
		for(int curveIdx = 0; curveIdx < numCurves.get(); curveIdx++) {
			if(curveIdx == activeCurve) continue;
			
			auto& curvePoints = allCurvePoints[curveIdx];
			auto& curveTensions = allSigmoidTensions[curveIdx];
			
			ImU32 curveColor = getCurveColor(curveIdx, 50);
			
			for(size_t i = 0; i < curvePoints.size() - 1; i++) {
				SigmoidControlPoint& pt1 = curvePoints[i];
				SigmoidControlPoint& pt2 = curvePoints[i + 1];
				SigmoidTension& tension = curveTensions[i];
				
				float x1 = posToX(pt1.position);
				float x2 = posToX(pt2.position);
				
				int pixelWidth = std::abs(x2 - x1);
				int numSamples = std::max(50, pixelWidth * 2);
				
				for(int j = 0; j < numSamples; j++) {
					float t1 = j / (float)numSamples;
					float t2 = (j + 1) / (float)numSamples;
					
					float val1 = evaluateSegment(pt1, pt2, tension, t1);
					float val2 = evaluateSegment(pt1, pt2, tension, t2);
					
					float sx1 = posToX(pt1.position + t1 * (pt2.position - pt1.position));
					float sy1 = valueToY(val1);
					float sx2 = posToX(pt1.position + t2 * (pt2.position - pt1.position));
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
		
		// Draw active curve
		ImU32 activeCurveColor = getCurveColor(activeCurve, 255);
		
		for(size_t i = 0; i < activePoints.size() - 1; i++) {
			SigmoidControlPoint& pt1 = activePoints[i];
			SigmoidControlPoint& pt2 = activePoints[i + 1];
			SigmoidTension& tension = activeTensions[i];
			
			float x1 = posToX(pt1.position);
			float x2 = posToX(pt2.position);
			
			int pixelWidth = std::abs(x2 - x1);
			int numSamples = std::max(50, pixelWidth * 2);
			
			for(int j = 0; j < numSamples; j++) {
				float t1 = j / (float)numSamples;
				float t2 = (j + 1) / (float)numSamples;
				
				float val1 = evaluateSegment(pt1, pt2, tension, t1);
				float val2 = evaluateSegment(pt1, pt2, tension, t2);
				
				float sx1 = posToX(pt1.position + t1 * (pt2.position - pt1.position));
				float sy1 = valueToY(val1);
				float sx2 = posToX(pt1.position + t2 * (pt2.position - pt1.position));
				float sy2 = valueToY(val2);
				
				dl->AddLine(
					ImVec2(sx1, sy1),
					ImVec2(sx2, sy2),
					activeCurveColor,
					2.0f
				);
			}
		}
		
		// Draw points (only for active curve)
		for(size_t i = 0; i < activePoints.size(); i++) {
			SigmoidControlPoint& pt = activePoints[i];
			float px = posToX(pt.position);
			float py = valueToY(pt.value);
			
			bool isPointHovered = (std::abs(mousePos.x - px) < 8 && std::abs(mousePos.y - py) < 8);
			float radius = (isPointHovered || (int)i == selectedPoint) ? 6.0f : 4.0f;
			
			ImU32 pointColor = (int)i == selectedPoint ?
				IM_COL32(255, 200, 100, 255) :
				IM_COL32(255, 255, 255, 255);
			
			dl->AddCircleFilled(ImVec2(px, py), radius, pointColor);
			dl->AddCircle(ImVec2(px, py), radius, IM_COL32(50, 50, 50, 255), 12, 1.5f);
		}
		
		// Draw phasor indicator(s)
		vector<float> phasors = phasorInput.get();
		int nCurves = numCurves.get();
		
		if(nCurves == 1) {
			// One curve: draw all phasor positions
			for(size_t i = 0; i < phasors.size(); i++) {
				float phasor = ofClamp(phasors[i], 0.0f, 1.0f);
				float phasorX = posToX(phasor);
				
				if(phasorX >= p.x && phasorX <= endP.x) {
					// Slightly different colors for multiple readers
					uint8_t hue = (i * 40) % 255;
					dl->AddLine(
						ImVec2(phasorX, p.y),
						ImVec2(phasorX, endP.y),
						IM_COL32(255, 80, 80, 200),
						1.5f
					);
					
					float currentValue = evaluateCurveAt(phasor, 0);
					float currentY = valueToY(currentValue);
					dl->AddCircleFilled(
						ImVec2(phasorX, currentY),
						4.0f,
						IM_COL32(255, 80, 80, 255)
					);
				}
			}
		} else {
			// Multiple curves: draw phasor for active curve only
			int phasorIdx = std::min(activeCurve, (int)phasors.size() - 1);
			if(phasors.size() > 0) {
				float phasor = ofClamp(phasors[phasorIdx], 0.0f, 1.0f);
				float phasorX = posToX(phasor);
				
				if(phasorX >= p.x && phasorX <= endP.x) {
					dl->AddLine(
						ImVec2(phasorX, p.y),
						ImVec2(phasorX, endP.y),
						IM_COL32(255, 80, 80, 255),
						2.5f
					);
					
					// Draw value indicators for all curves at their respective phasors
					for(int curveIdx = 0; curveIdx < nCurves; curveIdx++) {
						int phIdx = std::min(curveIdx, (int)phasors.size() - 1);
						float ph = phasors[phIdx];
						float currentValue = evaluateCurveAt(ph, curveIdx);
						float currentY = valueToY(currentValue);
						float phX = posToX(ph);
						
						if(curveIdx == activeCurve) {
							dl->AddCircleFilled(
								ImVec2(phX, currentY),
								5.0f,
								IM_COL32(255, 80, 80, 255)
							);
						} else {
							dl->AddCircleFilled(
								ImVec2(phX, currentY),
								3.0f,
								IM_COL32(255, 80, 80, 100)
							);
						}
					}
				}
			}
		}
		
		// Handle interactions
		if(isLeftClick) {
			// Check if clicking on a point
			selectedPoint = -1;
			for(size_t i = 0; i < activePoints.size(); i++) {
				float px = posToX(activePoints[i].position);
				float py = valueToY(activePoints[i].value);
				
				if(std::abs(mousePos.x - px) < 8 && std::abs(mousePos.y - py) < 8) {
					selectedPoint = i;
					isDraggingPoint = true;
					break;
				}
			}
			
			// If not clicking on point and Alt not held, add new point
			if(selectedPoint == -1 && !isAltHeld) {
				double newPos = snap(xToPos(mousePos.x));
				float newValue = ofClamp(yToValue(mousePos.y), 0.0f, 1.0f);
				
				SigmoidControlPoint newPt;
				newPt.position = ofClamp(newPos, 0.0, 1.0);
				newPt.value = newValue;
				activePoints.push_back(newPt);
				std::sort(activePoints.begin(), activePoints.end());
				
				rebuildTensions(activeCurve);
			}
			
			// If Alt held, check if clicking on a curve segment
			if(isAltHeld) {
				selectedSegment = findClosestSegment(mousePos, p, posToX, valueToY, activeCurve);
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
				double newPos = snap(xToPos(mousePos.x));
				float newValue = ofClamp(yToValue(mousePos.y), 0.0f, 1.0f);
				
				activePoints[selectedPoint].position = ofClamp(newPos, 0.0, 1.0);
				activePoints[selectedPoint].value = newValue;
				
				std::sort(activePoints.begin(), activePoints.end());
				rebuildTensions(activeCurve);
			}
			else if(isDraggingTension && selectedSegment >= 0) {
				float deltaX = mousePos.x - dragStartMouse.x;
				float deltaY = mousePos.y - dragStartMouse.y;
				
				float inflectionDelta = deltaX / s.x;
				activeTensions[selectedSegment].inflection = ofClamp(
					dragStartInflection + inflectionDelta,
					0.01f, 0.99f
				);
				
				float steepnessDelta = -deltaY / (s.y / 3.0f);
				float newSteepness = dragStartSteepness * exp(steepnessDelta * 0.5f);
				
				activeTensions[selectedSegment].steepness = ofClamp(
					newSteepness,
					0.05f, 10.0f
				);
			}
		}
		
		if(isReleased) {
			isDraggingPoint = false;
			isDraggingTension = false;
			selectedSegment = -1;
			
			// Update output after any change
			updateCurveOutput();
		}
		
		if(isRightClick) {
			for(size_t i = 0; i < activePoints.size(); i++) {
				float px = posToX(activePoints[i].position);
				float py = valueToY(activePoints[i].value);
				
				if(std::abs(mousePos.x - px) < 8 && std::abs(mousePos.y - py) < 8) {
					if(activePoints.size() > 2) {
						activePoints.erase(activePoints.begin() + i);
						rebuildTensions(activeCurve);
						updateCurveOutput();
					}
					break;
				}
			}
		}
		
		// Visual feedback for alt+drag mode
		if(isAltHeld && isHovered) {
			if(isDraggingTension && selectedSegment >= 0) {
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
				int hoverSegment = findClosestSegment(mousePos, p, posToX, valueToY, activeCurve);
				if(hoverSegment >= 0) {
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
	
	void updateCurveOutput() {
		vector<float> phasors = phasorInput.get();
		vector<float> outputs;
		
		int numPhasors = phasors.size();
		int nCurves = numCurves.get();
		
		if(nCurves == 1) {
			// One curve: each phasor is a different reader for the same curve
			for(int i = 0; i < numPhasors; i++) {
				float value = evaluateCurveAt(phasors[i], 0);
				float mappedValue = minValue.get() + value * (maxValue.get() - minValue.get());
				outputs.push_back(mappedValue);
			}
		} else {
			// Multiple curves: each phasor index controls corresponding curve
			for(int curveIdx = 0; curveIdx < nCurves; curveIdx++) {
				// Use phasor at same index, or last phasor if we run out
				int phasorIdx = std::min(curveIdx, numPhasors - 1);
				float phasor = (numPhasors > 0) ? phasors[phasorIdx] : 0.0f;
				
				float value = evaluateCurveAt(phasor, curveIdx);
				float mappedValue = minValue.get() + value * (maxValue.get() - minValue.get());
				outputs.push_back(mappedValue);
			}
		}
		
		curveOutput = outputs;
	}
	
	float sigmoidFlex(float x, float p, float k) {
		x = ofClamp(x, 0.0f, 1.0f);
		p = ofClamp(p, 0.01f, 0.99f);
		k = ofClamp(k, 0.05f, 10.0f);
		
		const float epsilon = 0.0001f;
		if(x < epsilon) return 0.0f;
		if(x > 1.0f - epsilon) return 1.0f;
		
		float xSafe = ofClamp(x, epsilon, 1.0f - epsilon);
		float pSafe = ofClamp(p, epsilon, 1.0f - epsilon);
		
		float a = pow(xSafe / pSafe, k);
		float b = pow((1.0f - xSafe) / (1.0f - pSafe), k);
		
		float denominator = a + b;
		if(denominator < epsilon) return 0.5f;
		
		return a / denominator;
	}
	
	float evaluateSegment(const SigmoidControlPoint& p1, const SigmoidControlPoint& p2, const SigmoidTension& tension, float t) {
		float curveValue = sigmoidFlex(t, tension.inflection, tension.steepness);
		return p1.value + curveValue * (p2.value - p1.value);
	}
	
	float evaluateCurveAt(double position, int curveIdx) {
		if(curveIdx < 0 || curveIdx >= allCurvePoints.size()) return 0.0f;
		
		auto& points = allCurvePoints[curveIdx];
		auto& tensions = allSigmoidTensions[curveIdx];
		
		if(points.empty()) return 0.0f;
		if(points.size() == 1) return points[0].value;
		
		if(position <= points.front().position) return points.front().value;
		if(position >= points.back().position) return points.back().value;
		
		for(size_t i = 0; i < points.size() - 1; i++) {
			if(position >= points[i].position && position <= points[i + 1].position) {
				double segmentLength = points[i + 1].position - points[i].position;
				if(segmentLength < 0.001) return points[i].value;
				
				float t = (position - points[i].position) / segmentLength;
				return evaluateSegment(points[i], points[i + 1], tensions[i], t);
			}
		}
		
		return points.back().value;
	}
	
	void rebuildTensions(int curveIdx) {
		if(curveIdx < 0 || curveIdx >= allCurvePoints.size()) return;
		
		auto& points = allCurvePoints[curveIdx];
		auto& tensions = allSigmoidTensions[curveIdx];
		
		size_t neededTensions = points.size() > 0 ? points.size() - 1 : 0;
		
		while(tensions.size() < neededTensions) {
			tensions.push_back(SigmoidTension());
		}
		while(tensions.size() > neededTensions) {
			tensions.pop_back();
		}
	}
	
	int findClosestSegment(ImVec2 mouse, ImVec2 trackPos,
						   std::function<float(double)> posToX,
						   std::function<float(float)> valueToY,
						   int curveIdx) {
		if(curveIdx < 0 || curveIdx >= allCurvePoints.size()) return -1;
		
		auto& points = allCurvePoints[curveIdx];
		auto& tensions = allSigmoidTensions[curveIdx];
		
		float minDist = 15.0f;
		int closestSeg = -1;
		
		for(size_t i = 0; i < points.size() - 1; i++) {
			SigmoidControlPoint& pt1 = points[i];
			SigmoidControlPoint& pt2 = points[i + 1];
			SigmoidTension& tension = tensions[i];
			
			int numSamples = 20;
			for(int j = 0; j <= numSamples; j++) {
				float t = j / (float)numSamples;
				float val = evaluateSegment(pt1, pt2, tension, t);
				double pos = pt1.position + t * (pt2.position - pt1.position);
				
				float sx = posToX(pos);
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
	
	void resizeCurves(int newNumCurves) {
		while(allCurvePoints.size() < newNumCurves) {
			std::vector<SigmoidControlPoint> newCurve;
			SigmoidControlPoint p1, p2;
			p1.position = 0.0;
			p1.value = 0.0f;
			p2.position = 1.0;
			p2.value = 1.0f;
			newCurve.push_back(p1);
			newCurve.push_back(p2);
			allCurvePoints.push_back(newCurve);
			
			std::vector<SigmoidTension> newTensions;
			newTensions.push_back(SigmoidTension());
			allSigmoidTensions.push_back(newTensions);
		}
		
		while(allCurvePoints.size() > newNumCurves) {
			allCurvePoints.pop_back();
			allSigmoidTensions.pop_back();
		}
		
		if(activeCurve >= newNumCurves) {
			activeCurve = newNumCurves - 1;
		}
		if(activeCurve < 0) activeCurve = 0;
		
		updateOutputRange();
		updateCurveOutput();
	}
	
	ImU32 getCurveColor(int curveIdx, uint8_t alpha) {
		static const ImVec4 curveColors[] = {
			ImVec4(100.0f/255, 180.0f/255, 255.0f/255, 1.0f),
			ImVec4(255.0f/255, 100.0f/255, 100.0f/255, 1.0f),
			ImVec4(100.0f/255, 255.0f/255, 100.0f/255, 1.0f),
			ImVec4(255.0f/255, 200.0f/255, 100.0f/255, 1.0f),
			ImVec4(200.0f/255, 100.0f/255, 255.0f/255, 1.0f),
			ImVec4(100.0f/255, 255.0f/255, 255.0f/255, 1.0f),
			ImVec4(255.0f/255, 255.0f/255, 100.0f/255, 1.0f),
			ImVec4(255.0f/255, 100.0f/255, 200.0f/255, 1.0f),
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
	
	void updateOutputRange() {
		vector<float> minVals(numCurves.get(), minValue.get());
		vector<float> maxVals(numCurves.get(), maxValue.get());
		curveOutput.setMin(minVals);
		curveOutput.setMax(maxVals);
	}
	
	// Preset save/load
	void presetSave(ofJson &json) override {
		std::vector<std::vector<std::vector<float>>> allCurvesPoints;
		for(auto &curvePoints : allCurvePoints) {
			std::vector<std::vector<float>> pointsList;
			for(auto &pt : curvePoints) {
				pointsList.push_back({(float)pt.position, pt.value});
			}
			allCurvesPoints.push_back(pointsList);
		}
		json["allCurvePoints"] = allCurvesPoints;
		
		std::vector<std::vector<std::vector<float>>> allCurvesTensions;
		for(auto &curveTensions : allSigmoidTensions) {
			std::vector<std::vector<float>> tensionsList;
			for(auto &t : curveTensions) {
				tensionsList.push_back({t.inflection, t.steepness});
			}
			allCurvesTensions.push_back(tensionsList);
		}
		json["allSigmoidTensions"] = allCurvesTensions;
		
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
				std::vector<SigmoidControlPoint> curvePoints;
				for(auto &ptData : curveData) {
					if(ptData.size() >= 2) {
						SigmoidControlPoint pt;
						pt.position = ptData[0];
						pt.value = ptData[1];
						curvePoints.push_back(pt);
					}
				}
				allCurvePoints.push_back(curvePoints);
			}
		}
		
		if(json.count("allSigmoidTensions") > 0) {
			allSigmoidTensions.clear();
			for(auto &curveData : json["allSigmoidTensions"]) {
				std::vector<SigmoidTension> curveTensions;
				for(auto &tData : curveData) {
					if(tData.size() >= 2) {
						curveTensions.push_back(SigmoidTension(tData[0], tData[1]));
					}
				}
				allSigmoidTensions.push_back(curveTensions);
			}
		}
		
		if(json.count("activeCurve") > 0) {
			activeCurve = json["activeCurve"];
			activeCurve = ofClamp(activeCurve, 0, numCurves.get() - 1);
		}
		
		updateCurveOutput();
	}
};

#endif
