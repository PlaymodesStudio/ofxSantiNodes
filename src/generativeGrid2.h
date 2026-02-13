#ifndef generativeGrid2_h
#define generativeGrid2_h

#include "ofxOceanodeNodeModel.h"
#include <set>

class generativeGrid2 : public ofxOceanodeNodeModel {
public:
	generativeGrid2() : ofxOceanodeNodeModel("Generative Grid 2"), focusedCell(-1) {
		description = "Generates vector graphics in an irregular grid defined by vertex points. "
					 "GridX/GridY define the intersection points of grid lines. "
					 "Click cells in GUI to select them. "
					 "Shape types: 0=cross, 1=diagonal cross, 2=ellipse, 3=dot, 4=central horizontal, 5=central vertical. "
					 "Per-cell control of shapeType, opacity, scale (centered), and color.";
	}
	
	~generativeGrid2() {}
	
	void setup() override {
		// Grid definition inputs - normalized coordinates of grid lines
		addParameter(gridX.set("GridX", {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}, {0.0f}, {1.0f}));
		addParameter(gridY.set("GridY", {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}, {0.0f}, {1.0f}));
		
		// GUI control
		addParameter(showWindow.set("Show GUI", false));
		
		// Shape type per cell (0-5), indexed by selection order
		addParameter(shapeType.set("ShapeType", {0}, {0}, {5}));
		
		// Option to add dots at segment endpoints
		addParameter(endpointDots.set("Endpoint Dots", false));
		
		// Per-cell parameters (indexed by selection order)
		addParameter(scale.set("Scale", {1.0f}, {0.0f}, {2.0f}));
		addParameter(opacity.set("Opacity", {1.0f}, {0.0f}, {1.0f}));
		addParameter(red.set("Red", {1.0f}, {0.0f}, {1.0f}));
		addParameter(green.set("Green", {1.0f}, {0.0f}, {1.0f}));
		addParameter(blue.set("Blue", {1.0f}, {0.0f}, {1.0f}));
		
		// Outputs
		addOutputParameter(outX.set("Out.X", {0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(outY.set("Out.Y", {0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(outOpacity.set("Out.A", {0}, {0}, {1}));
		addOutputParameter(outR.set("Out.R", {0}, {0}, {1}));
		addOutputParameter(outG.set("Out.G", {0}, {0}, {1}));
		addOutputParameter(outB.set("Out.B", {0}, {0}, {1}));
		
		// Listeners
		listeners.push(gridX.newListener([this](vector<float> &v) { calculate(); }));
		listeners.push(gridY.newListener([this](vector<float> &v) { calculate(); }));
		listeners.push(shapeType.newListener([this](vector<int> &v) { calculate(); }));
		listeners.push(endpointDots.newListener([this](bool &b) { calculate(); }));
		listeners.push(scale.newListener([this](vector<float> &v) { calculate(); }));
		listeners.push(opacity.newListener([this](vector<float> &v) { calculate(); }));
		listeners.push(red.newListener([this](vector<float> &v) { calculate(); }));
		listeners.push(green.newListener([this](vector<float> &v) { calculate(); }));
		listeners.push(blue.newListener([this](vector<float> &v) { calculate(); }));
	}
	
	void draw(ofEventArgs &a) override {
		if (!showWindow) return;
		
		if (ImGui::Begin(("Generative Grid 2 " + ofToString(getNumIdentifier())).c_str())) {
			// Calculate grid dimensions
			int numCellsX = std::max(0, (int)gridX.get().size() - 1);
			int numCellsY = std::max(0, (int)gridY.get().size() - 1);
			int totalCells = numCellsX * numCellsY;
			
			ImGui::Text("Grid: %d x %d = %d cells", numCellsX, numCellsY, totalCells);
			ImGui::Text("Selected cells: %d", (int)selectedCells.size());
			
			// Shape type names for reference
			ImGui::TextDisabled("Shapes: 0=Cross, 1=DiagCross, 2=Ellipse, 3=Dot, 4=Horiz, 5=Vert");
			
			ImGui::Separator();
			
			// Selection controls
			if (ImGui::Button("Select All")) {
				selectedCells.clear();
				for (int i = 0; i < totalCells; i++) {
					selectedCells.push_back(i);
				}
				calculate();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Selection")) {
				selectedCells.clear();
				focusedCell = -1;
				calculate();
			}
			ImGui::SameLine();
			if (ImGui::Button("Invert Selection")) {
				vector<int> newSelection;
				for (int i = 0; i < totalCells; i++) {
					if (std::find(selectedCells.begin(), selectedCells.end(), i) == selectedCells.end()) {
						newSelection.push_back(i);
					}
				}
				selectedCells = newSelection;
				calculate();
			}
			
			ImGui::Separator();
			
			// Display selected cells list
			if (!selectedCells.empty()) {
				string cellList;
				for (int cellIdx : selectedCells) {
					int col = cellIdx % numCellsX;
					int row = cellIdx / numCellsX;
					cellList += "(" + ofToString(col) + "," + ofToString(row) + ") ";
				}
				ImGui::TextWrapped("Selected: %s", cellList.c_str());
			}
			
			ImGui::Separator();
			ImGui::Text("Click cells to select/deselect");
			
			// Visual preview
			auto screenSize = ImGui::GetContentRegionAvail();
			
			if (screenSize.x > 1.0f && screenSize.y > 1.0f) {
				screenSize.y = std::min(screenSize.y, 500.0f);
				screenSize.x = std::max(screenSize.x, 100.0f);
				screenSize.y = std::max(screenSize.y, 100.0f);
				
				auto screenPos = ImGui::GetCursorScreenPos();
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				
				// Draw background
				draw_list->AddRectFilled(screenPos,
										ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
										IM_COL32(20, 20, 20, 255));
				
				const auto& gx = gridX.get();
				const auto& gy = gridY.get();
				
				// Draw grid lines
				for (float x : gx) {
					float px = screenPos.x + x * screenSize.x;
					draw_list->AddLine(ImVec2(px, screenPos.y),
									  ImVec2(px, screenPos.y + screenSize.y),
									  IM_COL32(60, 60, 60, 255), 1.0f);
				}
				for (float y : gy) {
					float py = screenPos.y + y * screenSize.y;
					draw_list->AddLine(ImVec2(screenPos.x, py),
									  ImVec2(screenPos.x + screenSize.x, py),
									  IM_COL32(60, 60, 60, 255), 1.0f);
				}
				
				// Track hovered cell
				int hoveredCell = -1;
				
				// Create invisible button for interaction
				ImGui::InvisibleButton("GridPreview", screenSize);
				bool isHovered = ImGui::IsItemHovered();
				
				if (isHovered) {
					ImVec2 mousePos = ImGui::GetMousePos();
					float normX = (mousePos.x - screenPos.x) / screenSize.x;
					float normY = (mousePos.y - screenPos.y) / screenSize.y;
					
					// Find which cell is hovered
					int cellCol = -1, cellRow = -1;
					for (int i = 0; i < (int)gx.size() - 1; i++) {
						if (normX >= gx[i] && normX < gx[i + 1]) {
							cellCol = i;
							break;
						}
					}
					for (int i = 0; i < (int)gy.size() - 1; i++) {
						if (normY >= gy[i] && normY < gy[i + 1]) {
							cellRow = i;
							break;
						}
					}
					
					if (cellCol >= 0 && cellRow >= 0) {
						hoveredCell = cellRow * numCellsX + cellCol;
					}
					
					// Handle click
					if (ImGui::IsMouseClicked(0) && hoveredCell >= 0) {
						auto it = std::find(selectedCells.begin(), selectedCells.end(), hoveredCell);
						if (it != selectedCells.end()) {
							selectedCells.erase(it);
						} else {
							selectedCells.push_back(hoveredCell);
						}
						calculate();
					}
				}
				
				// Draw cells
				for (int row = 0; row < numCellsY; row++) {
					for (int col = 0; col < numCellsX; col++) {
						int cellIdx = row * numCellsX + col;
						
						float left = gx[col];
						float right = gx[col + 1];
						float top = gy[row];
						float bottom = gy[row + 1];
						
						ImVec2 p1(screenPos.x + left * screenSize.x, screenPos.y + top * screenSize.y);
						ImVec2 p2(screenPos.x + right * screenSize.x, screenPos.y + bottom * screenSize.y);
						
						bool isSelected = std::find(selectedCells.begin(), selectedCells.end(), cellIdx) != selectedCells.end();
						bool isHoveredCell = (hoveredCell == cellIdx);
						
						// Find selection index for color
						int selectionIndex = -1;
						for (int i = 0; i < selectedCells.size(); i++) {
							if (selectedCells[i] == cellIdx) {
								selectionIndex = i;
								break;
							}
						}
						
						// Cell fill
						if (isSelected) {
							ImVec4 cellColor = getCellColor(selectionIndex);
							draw_list->AddRectFilled(p1, p2,
								IM_COL32(cellColor.x * 255, cellColor.y * 255, cellColor.z * 255, 100));
						}
						
						// Hover highlight
						if (isHoveredCell) {
							draw_list->AddRect(p1, p2, IM_COL32(255, 255, 255, 200), 0, 0, 3.0f);
						}
						
						// Draw shape preview if selected
						if (isSelected) {
							drawShapePreview(draw_list, screenPos, screenSize, left, top, right, bottom, selectionIndex);
						}
						
						// Cell index label
						char label[16];
						snprintf(label, sizeof(label), "%d", cellIdx);
						ImVec2 textPos((p1.x + p2.x) * 0.5f - 5, (p1.y + p2.y) * 0.5f - 5);
						draw_list->AddText(textPos, IM_COL32(150, 150, 150, 255), label);
					}
				}
				
				// Hover tooltip
				if (hoveredCell >= 0) {
					bool isSelected = std::find(selectedCells.begin(), selectedCells.end(), hoveredCell) != selectedCells.end();
					ImGui::BeginTooltip();
					ImGui::Text("Cell %d", hoveredCell);
					ImGui::Text(isSelected ? "Click to deselect" : "Click to select");
					ImGui::EndTooltip();
				}
			}
		}
		ImGui::End();
	}
	
	void calculate() {
		const auto& gx = gridX.get();
		const auto& gy = gridY.get();
		
		int numCellsX = std::max(0, (int)gx.size() - 1);
		int numCellsY = std::max(0, (int)gy.size() - 1);
		
		if (numCellsX == 0 || numCellsY == 0 || selectedCells.empty()) {
			outX = vector<float>();
			outY = vector<float>();
			outOpacity = vector<float>();
			outR = vector<float>();
			outG = vector<float>();
			outB = vector<float>();
			return;
		}
		
		vector<float> finalX, finalY, finalOpacity, finalR, finalG, finalB;
		
		// Process each selected cell in order
		for (int selIdx = 0; selIdx < selectedCells.size(); selIdx++) {
			int cellIdx = selectedCells[selIdx];
			
			int col = cellIdx % numCellsX;
			int row = cellIdx / numCellsX;
			
			if (col >= numCellsX || row >= numCellsY) continue;
			
			float left = gx[col];
			float right = gx[col + 1];
			float top = gy[row];
			float bottom = gy[row + 1];
			
			// Get per-cell parameters
			int cellShapeType = getValueForIndex(shapeType.get(), selIdx, 0);
			float cellScale = getValueForIndex(scale.get(), selIdx, 1.0f);
			float cellOpacity = getValueForIndex(opacity.get(), selIdx, 1.0f);
			float cellR = getValueForIndex(red.get(), selIdx, 1.0f);
			float cellG = getValueForIndex(green.get(), selIdx, 1.0f);
			float cellB = getValueForIndex(blue.get(), selIdx, 1.0f);
			
			// Calculate scaled bounds (centered)
			float centerX = (left + right) * 0.5f;
			float centerY = (top + bottom) * 0.5f;
			float halfW = (right - left) * 0.5f * cellScale;
			float halfH = (bottom - top) * 0.5f * cellScale;
			
			float scaledLeft = centerX - halfW;
			float scaledRight = centerX + halfW;
			float scaledTop = centerY - halfH;
			float scaledBottom = centerY + halfH;
			
			// Generate shape and track vertex count for color/opacity assignment
			vector<float> shapeX, shapeY;
			generateShape(cellShapeType, scaledLeft, scaledTop, scaledRight, scaledBottom, centerX, centerY, endpointDots.get(), shapeX, shapeY);
			
			if (!shapeX.empty()) {
				// Add separator before shape (except first)
				if (!finalX.empty()) {
					finalX.push_back(-1);
					finalY.push_back(-1);
					// No opacity/color for separator
				}
				
				// Add shape vertices with per-vertex opacity and color
				for (size_t i = 0; i < shapeX.size(); i++) {
					if (shapeX[i] == -1) {
						// Internal separator
						finalX.push_back(-1);
						finalY.push_back(-1);
					} else {
						finalX.push_back(shapeX[i]);
						finalY.push_back(shapeY[i]);
						finalOpacity.push_back(cellOpacity);
						finalR.push_back(cellR);
						finalG.push_back(cellG);
						finalB.push_back(cellB);
					}
				}
			}
		}
		
		outX = finalX;
		outY = finalY;
		outOpacity = finalOpacity;
		outR = finalR;
		outG = finalG;
		outB = finalB;
	}
	
	void presetSave(ofJson &json) override {
		json["SelectedCells"] = selectedCells;
	}
	
	void presetRecallBeforeSettingParameters(ofJson &json) override {
		if (json.count("SelectedCells") > 0) {
			selectedCells = json["SelectedCells"].get<vector<int>>();
		}
	}
	
private:
	void generateShape(int type, float left, float top, float right, float bottom,
					  float centerX, float centerY, bool addEndpointDots,
					  vector<float>& x, vector<float>& y) {
		// Helper to add a dot point
		auto addDot = [&](float px, float py) {
			x.push_back(-1);
			y.push_back(-1);
			x.push_back(px);
			y.push_back(py);
		};
		
		switch (type) {
			case 0: // Cross (+)
				// Horizontal line
				x.push_back(left);
				y.push_back(centerY);
				x.push_back(right);
				y.push_back(centerY);
				// Separator
				x.push_back(-1);
				y.push_back(-1);
				// Vertical line
				x.push_back(centerX);
				y.push_back(top);
				x.push_back(centerX);
				y.push_back(bottom);
				// Endpoint dots: left, right, top, bottom, center
				if (addEndpointDots) {
					addDot(left, centerY);
					addDot(right, centerY);
					addDot(centerX, top);
					addDot(centerX, bottom);
					addDot(centerX, centerY);
				}
				break;
				
			case 1: // Diagonal cross (X)
				// TL to BR
				x.push_back(left);
				y.push_back(top);
				x.push_back(right);
				y.push_back(bottom);
				// Separator
				x.push_back(-1);
				y.push_back(-1);
				// TR to BL
				x.push_back(right);
				y.push_back(top);
				x.push_back(left);
				y.push_back(bottom);
				// Endpoint dots: 4 corners + center
				if (addEndpointDots) {
					addDot(left, top);
					addDot(right, bottom);
					addDot(right, top);
					addDot(left, bottom);
					addDot(centerX, centerY);
				}
				break;
				
			case 2: // Ellipse (no endpoint dots)
				{
					int segments = 32;
					float radiusX = (right - left) * 0.5f;
					float radiusY = (bottom - top) * 0.5f;
					
					for (int i = 0; i <= segments; i++) {
						float angle = (i * 2.0f * M_PI) / segments;
						x.push_back(centerX + radiusX * cos(angle));
						y.push_back(centerY + radiusY * sin(angle));
					}
				}
				break;
				
			case 3: // Central point (single dot) - no additional dots needed
				x.push_back(centerX);
				y.push_back(centerY);
				break;
				
			case 4: // Central horizontal
				x.push_back(left);
				y.push_back(centerY);
				x.push_back(right);
				y.push_back(centerY);
				// Endpoint dots: left, right
				if (addEndpointDots) {
					addDot(left, centerY);
					addDot(right, centerY);
				}
				break;
				
			case 5: // Central vertical
				x.push_back(centerX);
				y.push_back(top);
				x.push_back(centerX);
				y.push_back(bottom);
				// Endpoint dots: top, bottom
				if (addEndpointDots) {
					addDot(centerX, top);
					addDot(centerX, bottom);
				}
				break;
		}
	}
	
	void drawShapePreview(ImDrawList* draw_list, ImVec2 screenPos, ImVec2 screenSize,
						 float left, float top, float right, float bottom, int selectionIndex) {
		float centerX = (left + right) * 0.5f;
		float centerY = (top + bottom) * 0.5f;
		
		// Get scale and shape type for this cell
		float cellScale = getValueForIndex(scale.get(), selectionIndex, 1.0f);
		int cellShapeType = getValueForIndex(shapeType.get(), selectionIndex, 0);
		bool showDots = endpointDots.get();
		
		// Calculate scaled bounds
		float halfW = (right - left) * 0.5f * cellScale;
		float halfH = (bottom - top) * 0.5f * cellScale;
		float sLeft = centerX - halfW;
		float sRight = centerX + halfW;
		float sTop = centerY - halfH;
		float sBottom = centerY + halfH;
		
		// Convert to screen coords
		auto toScreen = [&](float x, float y) -> ImVec2 {
			return ImVec2(screenPos.x + x * screenSize.x, screenPos.y + y * screenSize.y);
		};
		
		ImVec4 col = getCellColor(selectionIndex);
		ImU32 color = IM_COL32(col.x * 255, col.y * 255, col.z * 255, 255);
		float lineWidth = 2.0f;
		float dotRadius = std::min(halfW, halfH) * 0.12f * screenSize.x;
		
		// Helper to draw endpoint dot
		auto drawDot = [&](float x, float y) {
			if (showDots) {
				draw_list->AddCircleFilled(toScreen(x, y), dotRadius, color, 12);
			}
		};
		
		switch (cellShapeType) {
			case 0: // Cross
				draw_list->AddLine(toScreen(sLeft, centerY), toScreen(sRight, centerY), color, lineWidth);
				draw_list->AddLine(toScreen(centerX, sTop), toScreen(centerX, sBottom), color, lineWidth);
				drawDot(sLeft, centerY);
				drawDot(sRight, centerY);
				drawDot(centerX, sTop);
				drawDot(centerX, sBottom);
				drawDot(centerX, centerY);
				break;
				
			case 1: // Diagonal cross
				draw_list->AddLine(toScreen(sLeft, sTop), toScreen(sRight, sBottom), color, lineWidth);
				draw_list->AddLine(toScreen(sRight, sTop), toScreen(sLeft, sBottom), color, lineWidth);
				drawDot(sLeft, sTop);
				drawDot(sRight, sBottom);
				drawDot(sRight, sTop);
				drawDot(sLeft, sBottom);
				drawDot(centerX, centerY);
				break;
				
			case 2: // Ellipse (no endpoint dots)
				{
					int segments = 32;
					float radiusX = halfW;
					float radiusY = halfH;
					for (int i = 0; i < segments; i++) {
						float a1 = (i * 2.0f * M_PI) / segments;
						float a2 = ((i + 1) * 2.0f * M_PI) / segments;
						draw_list->AddLine(
							toScreen(centerX + radiusX * cos(a1), centerY + radiusY * sin(a1)),
							toScreen(centerX + radiusX * cos(a2), centerY + radiusY * sin(a2)),
							color, lineWidth);
					}
				}
				break;
				
			case 3: // Central point (dot) - no additional dots
				{
					float centralDotRadius = std::min(sRight - sLeft, sBottom - sTop) * 0.08f * screenSize.x;
					draw_list->AddCircleFilled(toScreen(centerX, centerY), centralDotRadius, color, 16);
				}
				break;
				
			case 4: // Central horizontal
				draw_list->AddLine(toScreen(sLeft, centerY), toScreen(sRight, centerY), color, lineWidth);
				drawDot(sLeft, centerY);
				drawDot(sRight, centerY);
				break;
				
			case 5: // Central vertical
				draw_list->AddLine(toScreen(centerX, sTop), toScreen(centerX, sBottom), color, lineWidth);
				drawDot(centerX, sTop);
				drawDot(centerX, sBottom);
				break;
		}
	}
	
	template<typename T>
	T getValueForIndex(const vector<T>& vec, int index, T defaultValue) {
		if (vec.empty()) return defaultValue;
		if (index < 0) return defaultValue;
		if (index >= (int)vec.size()) return vec.back(); // Use last value for overflow
		return vec[index];
	}
	
	ImVec4 getCellColor(int index) {
		// Golden ratio based color distribution
		float hue = (index * 0.618033988749895f);
		hue = hue - floor(hue);
		
		float s = 0.8f;
		float v = 0.9f;
		int hi = (int)(hue * 6);
		float f = hue * 6 - hi;
		float p = v * (1 - s);
		float q = v * (1 - f * s);
		float t = v * (1 - (1 - f) * s);
		
		float r, g, b;
		switch (hi) {
			case 0: r = v; g = t; b = p; break;
			case 1: r = q; g = v; b = p; break;
			case 2: r = p; g = v; b = t; break;
			case 3: r = p; g = q; b = v; break;
			case 4: r = t; g = p; b = v; break;
			case 5: r = v; g = p; b = q; break;
			default: r = v; g = t; b = p; break;
		}
		
		return ImVec4(r, g, b, 1.0f);
	}
	
	// Parameters
	ofParameter<vector<float>> gridX, gridY;
	ofParameter<bool> showWindow;
	ofParameter<vector<int>> shapeType;
	ofParameter<bool> endpointDots;
	ofParameter<vector<float>> scale, opacity;
	ofParameter<vector<float>> red, green, blue;
	
	// Outputs
	ofParameter<vector<float>> outX, outY, outOpacity;
	ofParameter<vector<float>> outR, outG, outB;
	
	// State
	vector<int> selectedCells; // Ordered list of selected cell indices
	int focusedCell;
	
	ofEventListeners listeners;
};

#endif /* generativeGrid2_h */
