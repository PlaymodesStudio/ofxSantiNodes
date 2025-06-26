#ifndef vectorMatrixOffset_h
#define vectorMatrixOffset_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class vectorMatrixOffset : public ofxOceanodeNodeModel {
public:
	vectorMatrixOffset() : ofxOceanodeNodeModel("Vector Matrix Offset") {}

	void setup() override {
		description = "Applies spatial offset to a matrix of angle values. Input is a 1D vector representing a 2D matrix of angles (0-1). Width and Height define matrix dimensions. Offset ranges are dynamically scaled: OffsetX ranges from -width/2 to +width/2, OffsetY from -height/2 to +height/2. When Bounds=true, values wrap around; when Bounds=false, out-of-bounds values are set to zero. Output vector always maintains the same size as the matrix.";
		
		addParameter(input.set("Input", {0.5}, {0}, {1}));
		addParameter(width.set("Width", 8, 1, 128));
		addParameter(height.set("Height", 8, 1, 128));
		addParameter(offsetX.set("Offset X", 0, -6, 6));
		addParameter(offsetY.set("Offset Y", 0, -3, 3));
		addParameter(bounds.set("Bounds", true));
		addOutputParameter(output.set("Output", {0.5}, {0}, {1}));

		// Add listeners to trigger processing when parameters change
		listeners.push(input.newListener([this](vector<float> &){
			processMatrix();
		}));
		
		listeners.push(width.newListener([this](int &){
			processMatrix();
		}));
		
		listeners.push(height.newListener([this](int &){
			processMatrix();
		}));
		
		listeners.push(offsetX.newListener([this](float &){
			processMatrix();
		}));
		
		listeners.push(offsetY.newListener([this](float &){
			processMatrix();
		}));
		
		listeners.push(bounds.newListener([this](bool &){
			processMatrix();
		}));
	}

private:
	void processMatrix() {
		const auto& inputVec = input.get();
		int w = width.get();
		int h = height.get();
		int matrixSize = w * h;
		
		// Update offset parameter ranges based on matrix dimensions
		updateOffsetRanges();
		
		// Always maintain the same output size as matrix size
		vector<float> result(matrixSize, 0.0f);
		
		// If input is empty or matrix size is 0, return zeros
		if (inputVec.empty() || matrixSize == 0) {
			output.set(result);
			return;
		}
		
		// Get offset values (already in pixel units)
		int pixelOffsetX = static_cast<int>(round(offsetX.get()));
		int pixelOffsetY = static_cast<int>(round(offsetY.get()));
		
		// Process each position in the output matrix
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				int dstIndex = y * w + x;
				
				// Calculate source position (reverse the offset)
				int srcX = x - pixelOffsetX;
				int srcY = y - pixelOffsetY;
				
				if (bounds.get()) {
					// Wrap around (toroidal topology)
					srcX = ((srcX % w) + w) % w;
					srcY = ((srcY % h) + h) % h;
					
					// Calculate source index and get value
					int srcIndex = srcY * w + srcX;
					int inputIndex = srcIndex % inputVec.size();
					result[dstIndex] = inputVec[inputIndex];
				} else {
					// Only use values that fall within bounds, otherwise leave as 0
					if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
						int srcIndex = srcY * w + srcX;
						int inputIndex = srcIndex % inputVec.size();
						result[dstIndex] = inputVec[inputIndex];
					}
					// else: result[dstIndex] remains 0.0f (already initialized)
				}
			}
		}
		
		output.set(result);
	}
	
	void updateOffsetRanges() {
		int w = width.get();
		int h = height.get();
		
		// Update X offset range: -width/2 to +width/2
		int maxOffsetX = w / 2;
		if (offsetX.getMax() != maxOffsetX || offsetX.getMin() != -maxOffsetX) {
			float currentValue = offsetX.get();
			offsetX.setMin(-maxOffsetX);
			offsetX.setMax(maxOffsetX);
			// Clamp current value to new range
			offsetX.set(std::max(-maxOffsetX, std::min(maxOffsetX, static_cast<int>(currentValue))));
		}
		
		// Update Y offset range: -height/2 to +height/2
		int maxOffsetY = h / 2;
		if (offsetY.getMax() != maxOffsetY || offsetY.getMin() != -maxOffsetY) {
			float currentValue = offsetY.get();
			offsetY.setMin(-maxOffsetY);
			offsetY.setMax(maxOffsetY);
			// Clamp current value to new range
			offsetY.set(std::max(-maxOffsetY, std::min(maxOffsetY, static_cast<int>(currentValue))));
		}
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	ofParameter<int> width;
	ofParameter<int> height;
	ofParameter<float> offsetX;
	ofParameter<float> offsetY;
	ofParameter<bool> bounds;
	
	ofEventListeners listeners;
};

#endif /* vectorMatrixOffset_h */
