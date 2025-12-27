#ifndef vectorOfVectorDisplay_h
#define vectorOfVectorDisplay_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include <functional>

class vectorOfVectorDisplay : public ofxOceanodeNodeModel {
public:
	vectorOfVectorDisplay() : ofxOceanodeNodeModel("Vector of Vector Display") {
		description =
			"Displays vector<vector<float>> data as multiple lines.\n"
			"Supports both a floating window and an embedded node widget.";
	}

	void setup() override {
		// Floating window toggle
		addParameter(showWindow.set("Show", false));

		// Core parameters
		addParameter(input.set("Input",
							  std::vector<std::vector<float>>(1, std::vector<float>(1, 0.0f)),
							  std::vector<std::vector<float>>(1, std::vector<float>(1, -FLT_MAX)),
							  std::vector<std::vector<float>>(1, std::vector<float>(1, FLT_MAX))));

		addParameter(gain.set("Gain", 1.0f, 0.0f, 20.0f));
		addParameter(normalize.set("Normalize", false));

		// Inspector-only UI controls
		addInspectorParameter(drawInNode.set("Draw In Node", false));
		addInspectorParameter(widgetWidth.set("Widget Width", 400.0f, 80.0f, 1200.0f));
		addInspectorParameter(widgetHeight.set("Widget Height", 240.0f, 40.0f, 1200.0f));
		addInspectorParameter(showGrid.set("Grid", true));
		addInspectorParameter(lineThickness.set("Line Thickness", 1.5f, 0.5f, 5.0f));

		// Embedded node GUI region
		addCustomRegion(
			ofParameter<std::function<void()>>().set("VV Display", [this](){ drawWidget(); }),
			ofParameter<std::function<void()>>().set("VV Display", [this](){ drawWidget(); })
		);
	}

	void draw(ofEventArgs &) override {
		if(!showWindow.get()) return;

		std::string title =
			(canvasID == "Canvas" ? "" : canvasID + "/") +
			"Vector of Vector Display " + ofToString(getNumIdentifier());

		if(ImGui::Begin(title.c_str(), (bool*)&showWindow.get())) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			if(avail.x < 40) avail.x = 40;
			if(avail.y < 40) avail.y = 40;

			drawVectorOfVectorAtCursor(avail.x, avail.y, /*showInfo*/true);
		}
		ImGui::End();
	}

private:
	// Embedded widget wrapper
	void drawWidget() {
		if(!drawInNode.get()) return;

		float w = widgetWidth.get();
		float h = widgetHeight.get();

		drawVectorOfVectorAtCursor(w, h, /*showInfo*/false);
		ImGui::Dummy(ImVec2(0, 4));
	}

	// Core renderer
	void drawVectorOfVectorAtCursor(float targetW, float targetH, bool showInfoLine) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();

		const auto &data = input.get();
		if(data.empty()) {
			ImGui::Text("No data");
			return;
		}

		const float g = gain.get();
		const bool norm = normalize.get();

		// Find min/max for normalization
		float minVal = FLT_MAX;
		float maxVal = -FLT_MAX;
		
		if(norm) {
			for(const auto& row : data) {
				for(float val : row) {
					minVal = std::min(minVal, val);
					maxVal = std::max(maxVal, val);
				}
			}
			if(maxVal == minVal) maxVal = minVal + 1.0f;
		} else {
			minVal = 0.0f;
			maxVal = 1.0f;
		}

		ImVec2 start = cursorPos;
		ImVec2 end = ImVec2(start.x + targetW, start.y + targetH);

		// Background + border
		drawList->AddRectFilled(start, end, IM_COL32(15, 15, 15, 255));
		drawList->AddRect(start, end, IM_COL32(100, 100, 100, 255), 0.0f, 0, 1.5f);

		const int numRows = data.size();
		const float rowHeight = targetH / (float)numRows;

		// Draw horizontal grid lines
		if(showGrid.get()) {
			for(int i = 0; i <= numRows; i++) {
				float y = start.y + i * rowHeight;
				drawList->AddLine(
					ImVec2(start.x, y),
					ImVec2(end.x, y),
					IM_COL32(60, 60, 60, 140),
					0.5f
				);
			}
		}

		// Draw each row as a line
		const float thickness = lineThickness.get();
		
		for(int i = 0; i < numRows; i++) {
			if(data[i].empty()) continue;

			const int numCols = data[i].size();
			const float colWidth = targetW / (float)(numCols - 1 > 0 ? numCols - 1 : 1);
			const float baseY = start.y + i * rowHeight;
			
			// Generate color per row
			float hue = (float)i / (float)numRows;
			ImU32 color = ImColor::HSV(hue, 0.7f, 0.9f);

			for(int j = 0; j < numCols - 1; j++) {
				float val1 = std::clamp(data[i][j] * g, minVal, maxVal);
				float val2 = std::clamp(data[i][j + 1] * g, minVal, maxVal);

				float norm1 = (val1 - minVal) / (maxVal - minVal);
				float norm2 = (val2 - minVal) / (maxVal - minVal);

				float x1 = start.x + j * colWidth;
				float x2 = start.x + (j + 1) * colWidth;
				float y1 = baseY + rowHeight - (norm1 * rowHeight);
				float y2 = baseY + rowHeight - (norm2 * rowHeight);

				drawList->AddLine(
					ImVec2(x1, y1),
					ImVec2(x2, y2),
					color,
					thickness
				);
			}
		}

		// Consume layout space
		ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + targetH));
		ImGui::Dummy(ImVec2(targetW, 1.0f));

		if(showInfoLine) {
			ImGui::Separator();
			ImGui::Text("Rows: %d | Gain: %.2f | Range: [%.2f, %.2f]",
					   numRows, g, minVal, maxVal);
		}
	}

private:
	// Parameters
	ofParameter<bool> showWindow;
	ofParameter<std::vector<std::vector<float>>> input;
	ofParameter<float> gain;
	ofParameter<bool> normalize;

	// Inspector-only
	ofParameter<bool> drawInNode;
	ofParameter<float> widgetWidth;
	ofParameter<float> widgetHeight;
	ofParameter<bool> showGrid;
	ofParameter<float> lineThickness;
};

#endif /* vectorOfVectorDisplay_h */
