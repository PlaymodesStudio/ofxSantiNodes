#ifndef vectorMatrixReflect_h
#define vectorMatrixReflect_h

#include "ofxOceanodeNodeModel.h"
#include <cmath>

class vectorMatrixReflect : public ofxOceanodeNodeModel {
public:
	vectorMatrixReflect() : ofxOceanodeNodeModel("Vector Matrix Reflect") {}

	void setup() override {
		description = "Transforms a 1D vector as a 2D matrix with rotation and reflection operations. Input vector is interpreted as a matrix with specified width and height dimensions.";
		
		// Input parameter
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Matrix dimensions
		addParameter(width.set("W", 3, 1, 1000));
		addParameter(height.set("H", 3, 1, 1000));
		
		// Rotation parameter (0-1 maps to 0-360 degrees)
		addParameter(rotate.set("Rotate", 0.0f, 0.0f, 1.0f));
		
		// Reflection toggles
		addParameter(reflectH.set("Reflect H", false));
		addParameter(reflectV.set("Reflect V", false));
		
		// Output parameter
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
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
		
		listeners.push(rotate.newListener([this](float &){
			processMatrix();
		}));
		
		listeners.push(reflectH.newListener([this](bool &){
			processMatrix();
		}));
		
		listeners.push(reflectV.newListener([this](bool &){
			processMatrix();
		}));
	}

private:
	void processMatrix() {
		const auto& inputVec = input.get();
		int w = width.get();
		int h = height.get();
		
		// Ensure we have valid dimensions
		if (w <= 0 || h <= 0) {
			output.set(vector<float>());
			return;
		}
		
		int matrixSize = w * h;
		vector<float> matrix(matrixSize, 0.0f);
		
		// Fill matrix from input vector (pad with zeros if input is smaller)
		for (int i = 0; i < std::min((int)inputVec.size(), matrixSize); ++i) {
			matrix[i] = inputVec[i];
		}
		
		// Apply transformations
		matrix = applyRotation(matrix, w, h);
		matrix = applyReflections(matrix, w, h);
		
		output.set(matrix);
	}
	
	vector<float> applyRotation(const vector<float>& matrix, int w, int h) {
		float rotationAngle = rotate.get() * 360.0f; // Convert 0-1 to 0-360 degrees
		
		// Normalize angle to 0-360 range
		while (rotationAngle < 0) rotationAngle += 360.0f;
		while (rotationAngle >= 360) rotationAngle -= 360.0f;
		
		// For simplicity, implement 90-degree increments
		int rotationSteps = (int)round(rotationAngle / 90.0f) % 4;
		
		vector<float> result = matrix;
		int currentW = w;
		int currentH = h;
		
		for (int step = 0; step < rotationSteps; ++step) {
			vector<float> rotated(currentW * currentH);
			
			// Rotate 90 degrees clockwise
			for (int y = 0; y < currentH; ++y) {
				for (int x = 0; x < currentW; ++x) {
					int oldIndex = y * currentW + x;
					int newX = currentH - 1 - y;
					int newY = x;
					int newIndex = newY * currentH + newX;
					rotated[newIndex] = result[oldIndex];
				}
			}
			
			result = rotated;
			std::swap(currentW, currentH); // Dimensions swap after 90-degree rotation
		}
		
		return result;
	}
	
	vector<float> applyReflections(const vector<float>& matrix, int w, int h) {
		vector<float> result = matrix;
		
		// Apply horizontal reflection (flip left-right)
		if (reflectH.get()) {
			vector<float> reflected(w * h);
			for (int y = 0; y < h; ++y) {
				for (int x = 0; x < w; ++x) {
					int oldIndex = y * w + x;
					int newX = w - 1 - x;
					int newIndex = y * w + newX;
					reflected[newIndex] = result[oldIndex];
				}
			}
			result = reflected;
		}
		
		// Apply vertical reflection (flip top-bottom)
		if (reflectV.get()) {
			vector<float> reflected(w * h);
			for (int y = 0; y < h; ++y) {
				for (int x = 0; x < w; ++x) {
					int oldIndex = y * w + x;
					int newY = h - 1 - y;
					int newIndex = newY * w + x;
					reflected[newIndex] = result[oldIndex];
				}
			}
			result = reflected;
		}
		
		return result;
	}
	
	// Parameters
	ofParameter<vector<float>> input;
	ofParameter<int> width;
	ofParameter<int> height;
	ofParameter<float> rotate;
	ofParameter<bool> reflectH;
	ofParameter<bool> reflectV;
	ofParameter<vector<float>> output;
	
	// Event listeners
	ofEventListeners listeners;
};

#endif /* vectorMatrixReflect_h */
