#ifndef vectorToCoordinates_h
#define vectorToCoordinates_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>

class vectorToCoordinates : public ofxOceanodeNodeModel {
public:
	vectorToCoordinates() : ofxOceanodeNodeModel("Vector to Coordinates") {
		description =
			"Converts linear indices to normalized 2D coordinates (0..1).\n"
			"Given W×H matrix dimensions, transforms index vector into X and Y coordinate vectors.";
	}

	void setup() override {
		// Core parameters
		addParameter(indices.set("Indices",
								std::vector<int>(1, 0),
								std::vector<int>(1, 0),
								std::vector<int>(1, 1000)));
		
		addParameter(W.set("W", 21, 1, 256));
		addParameter(H.set("H", 6, 1, 256));
		
		// Outputs
		addOutputParameter(outX.set("X[]",
								   std::vector<float>(1, 0.0f),
								   std::vector<float>(1, 0.0f),
								   std::vector<float>(1, 1.0f)));
		
		addOutputParameter(outY.set("Y[]",
								   std::vector<float>(1, 0.0f),
								   std::vector<float>(1, 0.0f),
								   std::vector<float>(1, 1.0f)));
		
		// Inspector-only display controls
		addInspectorParameter(showDisplay.set("Show Display", true));
		addInspectorParameter(displayWidth.set("Display Width", 320.0f, 100.0f, 800.0f));
		addInspectorParameter(displayHeight.set("Display Height", 120.0f, 50.0f, 400.0f));
		
		// Listeners
		indicesListener = indices.newListener([this](std::vector<int> &v){
			computeCoordinates();
		});
		
		wListener = W.newListener([this](int &w){
			computeCoordinates();
		});
		
		hListener = H.newListener([this](int &h){
			computeCoordinates();
		});
		
		// Embedded display
		addCustomRegion(
			ofParameter<std::function<void()>>().set("Coord Display", [this](){ drawDisplay(); }),
			ofParameter<std::function<void()>>().set("Coord Display", [this](){ drawDisplay(); })
		);
		
		// Initial computation
		computeCoordinates();
	}

private:
	void computeCoordinates() {
		const auto &idxVec = indices.get();
		const int w = std::max(1, W.get());
		const int h = std::max(1, H.get());
		const int totalCells = w * h;
		
		std::vector<float> xCoords, yCoords;
		xCoords.reserve(idxVec.size());
		yCoords.reserve(idxVec.size());
		
		for(int idx : idxVec) {
			// Clamp index to valid range
			idx = std::clamp(idx, 0, totalCells - 1);
			
			// Convert linear index to 2D coordinates
			int x = idx % w;
			int y = idx / w;
			
			// Normalize to 0..1
			// Center of cell: (x + 0.5) / w
			float normX = (x + 0.5f) / (float)w;
			float normY = (y + 0.5f) / (float)h;
			
			xCoords.push_back(normX);
			yCoords.push_back(normY);
		}
		
		outX = xCoords;
		outY = yCoords;
	}
	
	void drawDisplay() {
		if(!showDisplay.get()) return;
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		
		const float dispW = displayWidth.get();
		const float dispH = displayHeight.get();
		
		const int w = std::max(1, W.get());
		const int h = std::max(1, H.get());
		
		ImVec2 start = cursorPos;
		ImVec2 end = ImVec2(start.x + dispW, start.y + dispH);
		
		// Background
		drawList->AddRectFilled(start, end, IM_COL32(20, 20, 20, 255));
		drawList->AddRect(start, end, IM_COL32(100, 100, 100, 255), 0.0f, 0, 1.5f);
		
		// Draw grid
		const float cellW = dispW / (float)w;
		const float cellH = dispH / (float)h;
		
		for(int y = 0; y <= h; y++) {
			ImVec2 p0(start.x, start.y + y * cellH);
			ImVec2 p1(start.x + dispW, start.y + y * cellH);
			drawList->AddLine(p0, p1, IM_COL32(50, 50, 50, 255));
		}
		
		for(int x = 0; x <= w; x++) {
			ImVec2 p0(start.x + x * cellW, start.y);
			ImVec2 p1(start.x + x * cellW, start.y + dispH);
			drawList->AddLine(p0, p1, IM_COL32(50, 50, 50, 255));
		}
		
		// Draw indices as colored dots
		const auto &idxVec = indices.get();
		const int totalCells = w * h;
		
		for(size_t i = 0; i < idxVec.size(); i++) {
			int idx = std::clamp(idxVec[i], 0, totalCells - 1);
			
			int x = idx % w;
			int y = idx / w;
			
			// Position at center of cell
			ImVec2 center(start.x + (x + 0.5f) * cellW,
						 start.y + (y + 0.5f) * cellH);
			
			// Replace the color conversion section in drawDisplay() with this:

			// Color based on index in input vector (hue rotation)
			float hue = (i * 137.5f); // Golden angle for good distribution
			hue = fmodf(hue, 360.0f);

			// ColorConvertHSVtoRGB needs separate float outputs
			float r, g, b;
			ImGui::ColorConvertHSVtoRGB(hue / 360.0f, 0.8f, 0.9f, r, g, b);
			ImU32 col = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
			
			// Draw dot
			float radius = std::min(cellW, cellH) * 0.3f;
			drawList->AddCircleFilled(center, radius, col);
			
			// Draw index number if cells are large enough
			if(cellW > 25 && cellH > 25) {
				char label[16];
				snprintf(label, sizeof(label), "%d", (int)i);
				ImVec2 textSize = ImGui::CalcTextSize(label);
				ImVec2 textPos(center.x - textSize.x * 0.5f,
							  center.y - textSize.y * 0.5f);
				drawList->AddText(textPos, IM_COL32(0, 0, 0, 255), label);
			}
		}
		
		// Consume layout space
		ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + dispH));
		ImGui::Dummy(ImVec2(dispW, 1.0f));
		
		// Info line
		ImGui::Separator();
		ImGui::Text("Matrix: %dx%d (%d cells) | Indices: %d elements",
					w, h, w * h, (int)idxVec.size());
	}

private:
	ofParameter<std::vector<int>> indices;
	ofParameter<int> W, H;
	
	ofParameter<std::vector<float>> outX;
	ofParameter<std::vector<float>> outY;
	
	ofParameter<bool> showDisplay;
	ofParameter<float> displayWidth;
	ofParameter<float> displayHeight;
	
	ofEventListener indicesListener;
	ofEventListener wListener;
	ofEventListener hListener;
};

#endif /* vectorToCoordinates_h */
