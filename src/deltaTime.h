#ifndef deltaTime_h
#define deltaTime_h

#include "ofxOceanodeNodeModel.h"

class deltaTime : public ofxOceanodeNodeModel {
public:
	deltaTime() : ofxOceanodeNodeModel("Delta Time") {}

	void setup() override {
		description = "Measures time in milliseconds between non-zero values on a per-index basis. When 'ignoreZeros' is enabled, it measures time between non-zero values even if the value is the same.";
		
		// Input parameter
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(ignoreZeros.set("Ignore Zeros", true));
		
		// Output parameter
		addOutputParameter(timeDelta.set("Output", {0}, {0}, {FLT_MAX}));
		
		// Initialize state variables
		lastTimeNonZero.clear();
		lastValueChangeTime.clear();
		lastValues.clear();
		outputValues.clear();
		
		// Add listeners
		inputListener = input.newListener([this](vector<float> &){
			processDeltaTime();
		});
		
		ignoreZerosListener = ignoreZeros.newListener([this](bool &){
			// Reset tracking when mode changes
			lastTimeNonZero.clear();
			lastValueChangeTime.clear();
			lastValues.clear();
			processDeltaTime();
		});
	}

private:
	void processDeltaTime() {
		const vector<float>& currentInput = input.get();
		float currentTime = ofGetElapsedTimeMillis(); // Current time in milliseconds
		bool ignoreZeroValues = ignoreZeros.get();
		
		// Resize state vectors if input size changed
		if (lastValues.size() != currentInput.size()) {
			lastTimeNonZero.resize(currentInput.size(), 0);
			lastValueChangeTime.resize(currentInput.size(), 0);
			lastValues.resize(currentInput.size(), 0);
			outputValues.resize(currentInput.size(), 0);
		}
		
		// Process each index in the input vector
		for (size_t i = 0; i < currentInput.size(); ++i) {
			float currentValue = currentInput[i];
			
			if (ignoreZeroValues) {
				// In ignoreZeros mode
				if (currentValue != 0) {
					// Non-zero value detected
					if (lastTimeNonZero[i] > 0) {
						// We've seen a non-zero value before
						outputValues[i] = currentTime - lastTimeNonZero[i];
					}
					// Update the time of this non-zero value
					lastTimeNonZero[i] = currentTime;
				}
			} else {
				// Normal mode - detect value changes
				if (currentValue != lastValues[i]) {
					// Value has changed
					if (lastValueChangeTime[i] > 0) {
						// Calculate time since last value change
						outputValues[i] = currentTime - lastValueChangeTime[i];
					}
					// Update last change time and value
					lastValueChangeTime[i] = currentTime;
					lastValues[i] = currentValue;
				}
			}
		}
		
		// Set output parameter
		timeDelta = outputValues;
	}

	ofParameter<vector<float>> input;
	ofParameter<bool> ignoreZeros;
	ofParameter<vector<float>> timeDelta;
	
	vector<float> lastTimeNonZero;     // Stores time of last non-zero value (used when ignoring zeros)
	vector<float> lastValueChangeTime; // Stores time of last value change (used when not ignoring zeros)
	vector<float> lastValues;          // Stores last value for each index (for change detection)
	vector<float> outputValues;        // Stores the calculated time deltas
	
	ofEventListener inputListener;
	ofEventListener ignoreZerosListener;
};

#endif /* deltaTime_h */
