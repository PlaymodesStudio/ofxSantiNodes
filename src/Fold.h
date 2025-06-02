#ifndef fold_h
#define fold_h

#include "ofxOceanodeNodeModel.h"
#include <float.h>

class fold : public ofxOceanodeNodeModel {
public:
	fold() : ofxOceanodeNodeModel("Fold") {}
	
	void setup() override {
		description = "Folds values from input that exceed the high or low thresholds back into the threshold range, similar to audio wavefolding but for vectors of values. When a value exceeds the high threshold, it's reflected back down. When a value falls below the low threshold, it's reflected back up.";
		
		// Input parameters
		addParameter(input.set("Input", {0.5}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(lowThreshold.set("Low", 0.0, -FLT_MAX, FLT_MAX));
		addParameter(highThreshold.set("High", 1.0, -FLT_MAX, FLT_MAX));
		
		// Output parameter
		addOutputParameter(output.set("Output", {0.5}, {-FLT_MAX}, {FLT_MAX}));
		
		// Setup listeners to trigger processing when any parameter changes
		inputListener = input.newListener([this](vector<float> &_){
			processFolding();
		});
		
		lowThresholdListener = lowThreshold.newListener([this](float &_){
			processFolding();
		});
		
		highThresholdListener = highThreshold.newListener([this](float &_){
			processFolding();
		});
	}
	
private:
	void processFolding() {
		// Get parameter values
		auto inputValues = input.get();
		float low = lowThreshold.get();
		float high = highThreshold.get();
		
		// Make sure high is greater than low
		if (high <= low) {
			// Swap if necessary
			std::swap(high, low);
		}
		
		// Calculate the range
		float range = high - low;
		
		// Process each value
		vector<float> outputValues;
		outputValues.reserve(inputValues.size());
		
		for (float value : inputValues) {
			// Apply folding operation
			outputValues.push_back(foldValue(value, low, high, range));
		}
		
		// Set output
		output = outputValues;
	}
	
	float foldValue(float value, float low, float high, float range) {
		// Check if already in range
		if (value >= low && value <= high) {
			return value;
		}
		
		// Normalize to 0-1 range for folding calculation
		float normalized = (value - low) / range;
		
		// Apply folding using modulo and reflection
		float cycle = 2.0f;
		float modVal = std::fmod(std::abs(normalized), cycle);
		
		// Apply folding
		float folded = (modVal <= 1.0f) ? modVal : 2.0f - modVal;
		
		// Scale back to original range
		return folded * range + low;
	}
	
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	ofParameter<float> lowThreshold;
	ofParameter<float> highThreshold;
	ofEventListener inputListener;
	ofEventListener lowThresholdListener;
	ofEventListener highThresholdListener;
};

#endif /* fold_h */
