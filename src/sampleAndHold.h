#ifndef sampleAndHold_h
#define sampleAndHold_h

#include "ofxOceanodeNodeModel.h"

class sampleAndHold : public ofxOceanodeNodeModel {
public:
	sampleAndHold() : ofxOceanodeNodeModel("Sample And Hold") {}
	
	void setup() override {
		description = "Samples input values when corresponding gate is 1. In non-edge mode, continuously samples while gate is 1. In edge mode, only samples on rising edge (gate transition from 0 to 1).";
		
		// Input parameters
		addParameter(gatesInput.set("Gates", {0}, {0}, {1}));
		addParameter(valuesInput.set("Values", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(strict.set("Edge", false));
		
		// Output parameter
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Setup listeners
		listeners.push(gatesInput.newListener([this](vector<int> &gates){
			processInputs();
		}));
		
		listeners.push(valuesInput.newListener([this](vector<float> &values){
			processInputs();
		}));
		
		listeners.push(strict.newListener([this](bool &b){
			processInputs();
		}));
	}
	
private:
	void processInputs() {
		const auto& gates = gatesInput.get();
		const auto& values = valuesInput.get();
		auto currentOutput = output.get();
		
		// If either input is empty, return empty output
		if(gates.empty() || values.empty()) {
			output = vector<float>();
			return;
		}
		
		// Output size should match the values vector size
		size_t outputSize = values.size();
		
		// Resize vectors if needed
		if(currentOutput.size() != outputSize) {
			currentOutput.resize(outputSize, 0.0f);
		}
		if(previousGates.size() != gates.size()) {
			previousGates.resize(gates.size(), 0);
		}
		
		// Process each output element
		for(size_t i = 0; i < outputSize; i++) {
			// Determine which gate to use for this element
			size_t gateIndex = std::min(i, gates.size() - 1);
			int currentGate = gates[gateIndex];
			int prevGate = (gateIndex < previousGates.size()) ? previousGates[gateIndex] : 0;
			
			bool shouldSample = false;
			
			if(strict) {
				// In strict mode, only sample on rising edge (0 to 1 transition)
				shouldSample = (currentGate == 1 && prevGate == 0);
			} else {
				// In non-strict mode, sample whenever gate is 1
				shouldSample = (currentGate == 1);
			}
			
			if(shouldSample) {
				currentOutput[i] = values[i];
			}
			// When not sampling, keep existing value (sample and hold behavior)
		}
		
		// Store current gates for next iteration
		previousGates = gates;
		
		// Update output parameter
		output = currentOutput;
	}
	
	ofParameter<vector<int>> gatesInput;
	ofParameter<vector<float>> valuesInput;
	ofParameter<bool> strict;
	ofParameter<vector<float>> output;
	vector<int> previousGates;  // Store previous gate values for edge detection
	ofEventListeners listeners;
};

#endif /* sampleAndHold_h */
