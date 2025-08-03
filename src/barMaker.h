#ifndef barMaker_h
#define barMaker_h

#include "ofxOceanodeNodeModel.h"

class barMaker : public ofxOceanodeNodeModel {
public:
	barMaker() : ofxOceanodeNodeModel("Bar Maker") {
		description = "Creates bar chart-like paths from a point matrix. Full Bars mode draws complete bar outlines, while disabled draws only the bar tops/ends connected. The path behavior depends on the H/V orientation. Values parameter defines the height/width of each bar.";
	}
	
	void setup() override {
		addParameter(width.set("Width", 5, 2, 100));
		addParameter(height.set("Height", 5, 2, 100));
		addParameter(horizontalVertical.set("H/V", false)); // false = Horizontal bars, true = Vertical bars
		addParameter(values.set("Values", {2, 3, 1, 4}, {0}, {100}));
		addParameter(closed.set("Closed", false));
		addParameter(fullBars.set("Full Bars", false));
		addOutputParameter(x_out.set("X.Out", {0.5}, {0}, {1}));
		addOutputParameter(y_out.set("Y.Out", {0.5}, {0}, {1}));
		
		listeners.push(width.newListener([this](int &i) {
			updateValuesConstraints();
			calculateBars();
		}));
		
		listeners.push(height.newListener([this](int &i) {
			updateValuesConstraints();
			calculateBars();
		}));
		
		listeners.push(horizontalVertical.newListener([this](bool &b) {
			updateValuesConstraints();
			calculateBars();
		}));
		
		listeners.push(values.newListener([this](vector<int> &vi) {
			calculateBars();
		}));
		
		listeners.push(closed.newListener([this](bool &b) {
			calculateBars();
		}));
		
		listeners.push(fullBars.newListener([this](bool &b) {
			calculateBars();
		}));
		
		// Initialize
		updateValuesConstraints();
		calculateBars();
	}
	
private:
	void updateValuesConstraints() {
		// Update the maximum value constraint based on orientation
		if (horizontalVertical) {
			// Vertical bars: values represent width (X direction), constrained by width
			values.setMax(vector<int>(1, width - 1));
		} else {
			// Horizontal bars: values represent height (Y direction), constrained by height
			values.setMax(vector<int>(1, height - 1));
		}
	}
	
	void calculateBars() {
		vector<float> x_temp;
		vector<float> y_temp;
		
		if (values->empty()) {
			x_out = x_temp;
			y_out = y_temp;
			return;
		}
		
		// Calculate normalized coordinates for the matrix
		auto getMatrixPoint = [this](int col, int row) -> glm::vec2 {
			float x = (width > 1) ? (float)col / (width - 1) : 0.5f;
			float y = (height > 1) ? (float)row / (height - 1) : 0.5f;
			return glm::vec2(x, y);
		};
		
		if (!horizontalVertical) {
			// Horizontal bars mode
			// Expected number of bars = width - 1
			
			if (fullBars) {
				// Full bar outlines mode - draw complete bar shapes
				// Start at bottom-left corner
				glm::vec2 currentPoint = getMatrixPoint(0, height - 1);
				x_temp.push_back(currentPoint.x);
				y_temp.push_back(currentPoint.y);
				
				for (int bar = 0; bar < width - 1; bar++) {
					// Get bar height (how many rows up from bottom)
					int barHeight = (bar < values->size()) ? values->at(bar) : 0;
					barHeight = glm::clamp(barHeight, 0, height - 1);
					
					// Move vertically to bar height
					int targetRow = (height - 1) - barHeight;
					glm::vec2 topPoint = getMatrixPoint(bar, targetRow);
					x_temp.push_back(topPoint.x);
					y_temp.push_back(topPoint.y);
					
					// Move horizontally to next column at same height
					glm::vec2 nextTopPoint = getMatrixPoint(bar + 1, targetRow);
					x_temp.push_back(nextTopPoint.x);
					y_temp.push_back(nextTopPoint.y);
					
					// Move vertically down to bottom of next column
					glm::vec2 nextBottomPoint = getMatrixPoint(bar + 1, height - 1);
					x_temp.push_back(nextBottomPoint.x);
					y_temp.push_back(nextBottomPoint.y);
				}
			} else {
				// Bar tops only mode - draw just the tops connected in stair-step pattern
				// Start at bottom-left corner
				glm::vec2 startPoint = getMatrixPoint(0, height - 1);
				x_temp.push_back(startPoint.x);
				y_temp.push_back(startPoint.y);
				
				for (int bar = 0; bar < width - 1; bar++) {
					// Get bar height (how many rows up from bottom)
					int barHeight = (bar < values->size()) ? values->at(bar) : 0;
					barHeight = glm::clamp(barHeight, 0, height - 1);
					
					// Move vertically to bar height
					int targetRow = (height - 1) - barHeight;
					glm::vec2 topPoint = getMatrixPoint(bar, targetRow);
					x_temp.push_back(topPoint.x);
					y_temp.push_back(topPoint.y);
					
					// Move horizontally to next column at same height
					glm::vec2 nextTopPoint = getMatrixPoint(bar + 1, targetRow);
					x_temp.push_back(nextTopPoint.x);
					y_temp.push_back(nextTopPoint.y);
				}
				
				// For the last bar, go down to bottom-right corner
				glm::vec2 endPoint = getMatrixPoint(width - 1, height - 1);
				x_temp.push_back(endPoint.x);
				y_temp.push_back(endPoint.y);
			}
			
		} else {
			// Vertical bars mode
			// Expected number of bars = height - 1
			
			if (fullBars) {
				// Full bar outlines mode - draw complete bar shapes
				// Start at bottom-left corner
				glm::vec2 currentPoint = getMatrixPoint(0, height - 1);
				x_temp.push_back(currentPoint.x);
				y_temp.push_back(currentPoint.y);
				
				for (int bar = 0; bar < height - 1; bar++) {
					// Get bar width (how many columns right from left)
					int barWidth = (bar < values->size()) ? values->at(bar) : 0;
					barWidth = glm::clamp(barWidth, 0, width - 1);
					
					// Move horizontally to bar width
					int targetCol = barWidth;
					glm::vec2 rightPoint = getMatrixPoint(targetCol, (height - 1) - bar);
					x_temp.push_back(rightPoint.x);
					y_temp.push_back(rightPoint.y);
					
					// Move vertically to next row at same width
					glm::vec2 nextRightPoint = getMatrixPoint(targetCol, (height - 1) - bar - 1);
					x_temp.push_back(nextRightPoint.x);
					y_temp.push_back(nextRightPoint.y);
					
					// Move horizontally back to left edge of next row
					glm::vec2 nextLeftPoint = getMatrixPoint(0, (height - 1) - bar - 1);
					x_temp.push_back(nextLeftPoint.x);
					y_temp.push_back(nextLeftPoint.y);
				}
			} else {
				// Bar tops only mode - draw just the bar ends connected in stair-step pattern
				// Start at bottom-left corner
				glm::vec2 startPoint = getMatrixPoint(0, height - 1);
				x_temp.push_back(startPoint.x);
				y_temp.push_back(startPoint.y);
				
				for (int bar = 0; bar < height - 1; bar++) {
					// Get bar width (how many columns right from left)
					int barWidth = (bar < values->size()) ? values->at(bar) : 0;
					barWidth = glm::clamp(barWidth, 0, width - 1);
					
					// Move horizontally to bar width
					int targetCol = barWidth;
					glm::vec2 rightPoint = getMatrixPoint(targetCol, (height - 1) - bar);
					x_temp.push_back(rightPoint.x);
					y_temp.push_back(rightPoint.y);
					
					// Move vertically to next row at same width
					glm::vec2 nextRightPoint = getMatrixPoint(targetCol, (height - 1) - bar - 1);
					x_temp.push_back(nextRightPoint.x);
					y_temp.push_back(nextRightPoint.y);
				}
				
				// For the last bar, go to the left edge
				glm::vec2 endPoint = getMatrixPoint(0, 0);
				x_temp.push_back(endPoint.x);
				y_temp.push_back(endPoint.y);
			}
		}
		
		// If closed is enabled, connect back to starting point
		if (closed && !x_temp.empty()) {
			x_temp.push_back(x_temp[0]);
			y_temp.push_back(y_temp[0]);
		}
		
		// Add path separator
		x_temp.push_back(-1);
		y_temp.push_back(-1);
		
		x_out = x_temp;
		y_out = y_temp;
	}
	
	ofParameter<int> width, height;
	ofParameter<bool> horizontalVertical; // false = Horizontal bars, true = Vertical bars
	ofParameter<vector<int>> values;
	ofParameter<bool> closed, fullBars;
	ofParameter<vector<float>> x_out, y_out;
	
	ofEventListeners listeners;
};

#endif /* barMaker_h */
