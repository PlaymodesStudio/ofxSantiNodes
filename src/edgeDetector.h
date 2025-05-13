#ifndef edgeDetector_h
#define edgeDetector_h

#include "ofxOceanodeNodeModel.h"

class edgeDetector : public ofxOceanodeNodeModel {
public:
	edgeDetector() : ofxOceanodeNodeModel("Edge Detector") {}

	void setup() override {
		description = "Detects rising and falling edges in a float vector input. Outputs '1' on rising/falling outputs where an edge is detected, '0' otherwise.";
		
		// Input parameter
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Output parameters
		addOutputParameter(risingEdge.set("Rising", {0}, {0}, {1}));
		addOutputParameter(fallingEdge.set("Falling", {0}, {0}, {1}));
		
		// Store previous input values
		previousInput.clear();
		
		// Add listener
		listener = input.newListener([this](vector<float> &){
			detectEdges();
		});
	}

private:
	void detectEdges() {
		const vector<float>& currentInput = input.get();
		size_t inputSize = currentInput.size();
		
		// Initialize output vectors with zeros
		vector<float> rising(inputSize, 0.0f);
		vector<float> falling(inputSize, 0.0f);
		
		// Resize previous input if needed
		if (previousInput.size() != inputSize) {
			previousInput = vector<float>(inputSize, currentInput[0]);
		}
		
		// Detect edges by comparing with previous values
		for (size_t i = 0; i < inputSize; ++i) {
			// Rising edge (current > previous)
			if (currentInput[i] > previousInput[i]) {
				rising[i] = 1.0f;
			}
			
			// Falling edge (current < previous)
			if (currentInput[i] < previousInput[i]) {
				falling[i] = 1.0f;
			}
			
			// Update previous value
			previousInput[i] = currentInput[i];
		}
		
		// Set output parameters
		risingEdge = rising;
		fallingEdge = falling;
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<float>> risingEdge;
	ofParameter<vector<float>> fallingEdge;
	vector<float> previousInput;
	ofEventListener listener;
};

#endif /* edgeDetector_h */
