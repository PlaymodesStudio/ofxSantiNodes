#ifndef matrixDisplay_h
#define matrixDisplay_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include <functional>

class matrixDisplay : public ofxOceanodeNodeModel {
public:
	matrixDisplay() : ofxOceanodeNodeModel("Matrix Display") {
		description =
			"Displays a W×H grayscale matrix from a float vector input (0..1).\n"
			"Supports both a floating window and an embedded node widget.";
	}

	void setup() override {
		// Floating window toggle (normal parameter, like your wavescope)
		addParameter(showWindow.set("Show", false));

		// Core parameters (normal)
		addParameter(in.set("In",
							std::vector<float>(1, 0.0f),
							std::vector<float>(1, 0.0f),
							std::vector<float>(1, 1.0f)));

		addParameter(W.set("W", 8, 1, 256));
		addParameter(H.set("H", 8, 1, 256));

		addParameter(gain.set("Gain", 1.0f, 0.0f, 20.0f));

		// Inspector-only UI controls (like scVUMeter)
		addInspectorParameter(drawInNode.set("Draw In Node", false));
		addInspectorParameter(widgetWidth.set("Widget Width", 240.0f, 80.0f, 1200.0f));
		addInspectorParameter(widgetHeight.set("Widget Height", 240.0f, 40.0f, 1200.0f));
		addInspectorParameter(showGrid.set("Grid", true));
		addInspectorParameter(padding.set("Padding", 1.0f, 0.0f, 8.0f));

		// Embedded node GUI region (same pattern as VUMeter)
		addCustomRegion(
			ofParameter<std::function<void()>>().set("Matrix Display", [this](){ drawMatrixWidget(); }),
			ofParameter<std::function<void()>>().set("Matrix Display", [this](){ drawMatrixWidget(); })
		);
	}

	void draw(ofEventArgs &) override {
		if(!showWindow.get()) return;

		std::string title =
			(canvasID == "Canvas" ? "" : canvasID + "/") +
			"Matrix Display " + ofToString(getNumIdentifier());

		if(ImGui::Begin(title.c_str(), (bool*)&showWindow.get())) {
			// In floating window we size to available area (not inspector widget size)
			ImVec2 avail = ImGui::GetContentRegionAvail();
			if(avail.x < 40) avail.x = 40;
			if(avail.y < 40) avail.y = 40;

			// Draw matrix using same renderer
			drawMatrixAtCursor(avail.x, avail.y, /*showInfo*/true);
		}
		ImGui::End();
	}

private:
	// Embedded widget wrapper (respects Draw In Node + inspector W/H)
	void drawMatrixWidget() {

		float w = widgetWidth.get();
		float h = widgetHeight.get();

		// Use exact inspector widget size (like VU meter)
		drawMatrixAtCursor(w, h, /*showInfo*/false);

		// Small spacer after widget (keeps node layout clean)
		ImGui::Dummy(ImVec2(0, 4));
	}

	// Core renderer used by BOTH contexts
	// Draws into current ImGui cursor area, consuming layout space.
	void drawMatrixAtCursor(float targetW, float targetH, bool showInfoLine) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();

		const int wCells = std::max(1, W.get());
		const int hCells = std::max(1, H.get());
		const int totalCells = wCells * hCells;

		const float g = gain.get();
		const auto &v = in.get();

		// Fit cells inside target rectangle
		float cellSize = std::min(targetW / (float)wCells, targetH / (float)hCells);
		cellSize = std::max(1.0f, cellSize);

		const float drawW = cellSize * wCells;
		const float drawH = cellSize * hCells;

		ImVec2 start = cursorPos;
		ImVec2 end   = ImVec2(start.x + drawW, start.y + drawH);

		// Background + border
		drawList->AddRectFilled(start, end, IM_COL32(15, 15, 15, 255));
		drawList->AddRect(start, end, IM_COL32(100, 100, 100, 255), 0.0f, 0, 1.5f);

		const float pad = padding.get();
		const bool grid = showGrid.get();

		for(int yy = 0; yy < hCells; ++yy){
			for(int xx = 0; xx < wCells; ++xx){
				const int idx = yy * wCells + xx;

				float val = (idx < (int)v.size()) ? v[idx] : 0.0f;
				val = std::clamp(val * g, 0.0f, 1.0f);
				const unsigned char gg = (unsigned char)std::round(val * 255.0f);

				ImU32 col = IM_COL32(gg, gg, gg, 255);

				ImVec2 p0(start.x + xx * cellSize + pad,
						  start.y + yy * cellSize + pad);
				ImVec2 p1(start.x + (xx + 1) * cellSize - pad,
						  start.y + (yy + 1) * cellSize - pad);

				if(p1.x < p0.x) std::swap(p0.x, p1.x);
				if(p1.y < p0.y) std::swap(p0.y, p1.y);

				drawList->AddRectFilled(p0, p1, col);

				if(grid){
					ImVec2 g0(start.x + xx * cellSize,       start.y + yy * cellSize);
					ImVec2 g1(start.x + (xx + 1) * cellSize, start.y + (yy + 1) * cellSize);
					drawList->AddRect(g0, g1, IM_COL32(60, 60, 60, 140));
				}
			}
		}

		// Consume layout space for the widget
		ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + drawH));
		ImGui::Dummy(ImVec2(drawW, 1.0f));

		if(showInfoLine){
			ImGui::Separator();
			ImGui::Text("Input: %d | Matrix: %dx%d (%d) | Gain: %.2f",
						(int)v.size(), wCells, hCells, totalCells, g);
		}
	}

private:
	// Parameters
	ofParameter<bool> showWindow;

	ofParameter<std::vector<float>> in;
	ofParameter<int> W, H;
	ofParameter<float> gain;

	// Inspector-only
	ofParameter<bool> drawInNode;
	ofParameter<float> widgetWidth;
	ofParameter<float> widgetHeight;
	ofParameter<bool> showGrid;
	ofParameter<float> padding;
};

#endif /* matrixDisplay_h */
