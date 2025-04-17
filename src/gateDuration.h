#ifndef gateDuration_h
#define gateDuration_h

#include "ofxOceanodeNodeModel.h"

class gateDuration : public ofxOceanodeNodeModel {
public:
	gateDuration() : ofxOceanodeNodeModel("Gate Duration") {}

	void setup() override {
		description = "Extends gate signals for a specified duration in milliseconds. When an input gate transitions from 0 to 1, the output gate will remain at 1 for the duration specified in TimeMs.";
		
		// Add parameters
		addParameter(gateInput.set("Gate", {0}, {0}, {1}));
		addParameter(timeMs.set("TimeMs", {100}, {0}, {60000}));
		addOutputParameter(gateOutput.set("Output", {0}, {0}, {1}));
		
		// Initialize state tracking variables
		lastGateValues.clear();
		gateEndTimes.clear();
		
		// Register listener for gate input changes
		listener = gateInput.newListener([this](vector<float> &gates){
			processGateChanges();
		});
	}
	
	void update(ofEventArgs &e) override {
		// Get current time
		float currentTime = ofGetElapsedTimef();
		
		// Update output gates based on timing
		vector<float> currentOutput = gateOutput.get();
		bool outputChanged = false;
		
		// Make sure the output vector matches the input size
		if (currentOutput.size() != gateInput->size()) {
			currentOutput.resize(gateInput->size(), 0.0f);
			outputChanged = true;
		}
		
		// Update gates based on timers
		for (size_t i = 0; i < gateInput->size(); i++) {
			// If the gate is currently active, check if it should turn off
			if (currentOutput[i] > 0.5f && gateEndTimes[i] <= currentTime) {
				currentOutput[i] = 0.0f;
				outputChanged = true;
			}
			// If the gate just turned on due to a rising edge (handled in processGateChanges),
			// it will already have the correct endTime and be set to 1
		}
		
		// Update output if needed
		if (outputChanged) {
			gateOutput = currentOutput;
		}
	}

private:
	void processGateChanges() {
		// Get current values
		const auto& gates = gateInput.get();
		const auto& durations = timeMs.get();
		
		// Initialize or resize state vectors if needed
		if (lastGateValues.size() != gates.size()) {
			lastGateValues.resize(gates.size(), 0.0f);
			gateEndTimes.resize(gates.size(), 0.0f);
		}
		
		// Get current time
		float currentTime = ofGetElapsedTimef();
		
		// Prepare output vector
		vector<float> currentOutput = gateOutput.get();
		if (currentOutput.size() != gates.size()) {
			currentOutput.resize(gates.size(), 0.0f);
		}
		
		bool outputChanged = false;
		
		// Check for rising edges (0->1 transitions)
		for (size_t i = 0; i < gates.size(); i++) {
			// Get the appropriate duration for this index
			float duration = 100.0f; // Default 100ms
			if (i < durations.size()) {
				duration = durations[i];
			} else if (!durations.empty()) {
				// If index is out of range but durations has values, use the last value
				duration = durations.back();
			}
			
			// Check for rising edge (0->1)
			if (gates[i] > 0.5f && lastGateValues[i] <= 0.5f) {
				// Set the gate on
				currentOutput[i] = 1.0f;
				
				// Calculate when to turn it off (current time + duration in seconds)
				gateEndTimes[i] = currentTime + (duration / 1000.0f);
				
				outputChanged = true;
			}
			
			// Update last gate value
			lastGateValues[i] = gates[i];
		}
		
		// Update output if needed
		if (outputChanged) {
			gateOutput = currentOutput;
		}
	}

	ofParameter<vector<float>> gateInput;
	ofParameter<vector<float>> timeMs;
	ofParameter<vector<float>> gateOutput;
	
	vector<float> lastGateValues;  // For detecting transitions
	vector<float> gateEndTimes;    // Time when each gate should turn off
	
	ofEventListener listener;
};

#endif /* gateDuration_h */
