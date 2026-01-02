#ifndef transportQuantizer_h
#define transportQuantizer_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>
#include <cmath>

class transportQuantizer : public ofxOceanodeNodeModel {
public:
	transportQuantizer() : ofxOceanodeNodeModel("Transport Quantizer") {}

	void setup() override {
		// ---- INPUTS ----
		addSeparator("INPUTS", ofColor(240, 240, 240));
		
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(beatTransport.set("Beat Transport", 0.0f, 0.0f, FLT_MAX));
		
		// ---- PARAMETERS ----
		addSeparator("PARAMETERS", ofColor(240, 240, 240));
		
		addParameter(qGrid.set("Q Grid", 0.25f, 0.001f, 16.0f));
		addParameterDropdown(mode, "Mode", 0, {
			"Sample/Hold",
			"Impulse",
			"Gate Align"
		});
		
		// ---- OUTPUTS ----
		addSeparator("OUTPUTS", ofColor(240, 240, 240));
		
		addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		
		// ---- LISTENERS ----
		listeners.push(input.newListener([this](vector<float> &v) {
			resizeState(v.size());
		}));
		
		resizeState(1);
	}

	void update(ofEventArgs &) override {
		const auto& inputV = input.get();
		const float beat = beatTransport.get();
		const float grid = std::max(0.001f, qGrid.get());
		const int modeVal = mode.get();
		
		size_t n = inputV.size();
		resizeState(n);
		
		vector<float> outV(n, 0.0f);
		
		// Calculate current grid position
		float currentGrid = std::floor(beat / grid);
		
		// Process each channel
		for(size_t i = 0; i < n; ++i) {
			float inputVal = inputV[i];
			
			switch(modeVal) {
				case 0: // Sample/Hold
					outV[i] = processSampleHold(i, inputVal, currentGrid);
					break;
					
				case 1: // Impulse
					outV[i] = processImpulse(i, inputVal, currentGrid);
					break;
					
				case 2: // Gate Align
					outV[i] = processGateAlign(i, inputVal, currentGrid, beat, grid);
					break;
					
				default:
					outV[i] = inputVal;
					break;
			}
		}
		
		output = outV;
	}

private:

	// ---- MODE: SAMPLE/HOLD ----
	// Samples the input whenever we cross to a new grid position
	float processSampleHold(size_t ch, float inputVal, float currentGrid) {
		// Detect grid crossing
		if(currentGrid != lastGridPos[ch]) {
			// Sample input at grid position
			lastSampledValue[ch] = inputVal;
			lastGridPos[ch] = currentGrid;
		}
		
		return lastSampledValue[ch];
	}

	// ---- MODE: IMPULSE ----
	// Delays impulses (brief triggers) to the next grid position
	float processImpulse(size_t ch, float inputVal, float currentGrid) {
		// Detect impulse (rising edge above threshold)
		bool impulseDetected = (inputVal > 0.5f && lastInputValue[ch] <= 0.5f);
		
		// Grid crossing
		bool gridCrossing = (currentGrid != lastGridPos[ch]);
		
		if(impulseDetected) {
			// Latch the impulse until next grid
			impulseLatched[ch] = true;
		}
		
		float result = 0.0f;
		
		if(gridCrossing) {
			// On grid crossing, output latched impulse
			if(impulseLatched[ch]) {
				result = 1.0f;
				impulseLatched[ch] = false; // Clear latch
			}
			lastGridPos[ch] = currentGrid;
		}
		
		// Store input for edge detection
		lastInputValue[ch] = inputVal;
		
		return result;
	}

	// ---- MODE: GATE ALIGN ----
	// Aligns gate start to grid, preserves gate value and duration
	float processGateAlign(size_t ch, float inputVal, float currentGrid, float beat, float grid) {
		// Detect non-zero state (with small threshold for noise)
		bool inputHigh = (std::abs(inputVal) > 0.01f);
		bool wasHigh = (std::abs(lastInputValue[ch]) > 0.01f);
		
		// Detect gate start (rising edge from ~0 to non-zero)
		if(inputHigh && !wasHigh) {
			gateWaiting[ch] = true;
			gateValueAtRise[ch] = inputVal; // Capture the actual value
			gateStartBeat[ch] = beat;
		}
		
		// Detect gate end (falling edge back to ~0)
		if(!inputHigh && wasHigh) {
			gateActive[ch] = false;
			gateWaiting[ch] = false;
		}
		
		// Grid crossing
		bool gridCrossing = (currentGrid != lastGridPos[ch]);
		
		if(gridCrossing) {
			// If we have a waiting gate, activate it at the grid
			if(gateWaiting[ch]) {
				gateActive[ch] = true;
				gateWaiting[ch] = false;
			}
			lastGridPos[ch] = currentGrid;
		}
		
		// Update current value if gate is active and input is still high
		if(gateActive[ch] && inputHigh) {
			gateValueAtRise[ch] = inputVal; // Track current value
		}
		
		// Store input for edge detection
		lastInputValue[ch] = inputVal;
		
		// Output the aligned gate with its value
		return gateActive[ch] ? gateValueAtRise[ch] : 0.0f;
	}

	// ---- STATE MANAGEMENT ----
	void resizeState(size_t n) {
		if(n == 0) n = 1;
		
		if(lastSampledValue.size() != n) {
			lastSampledValue.assign(n, 0.0f);
			lastGridPos.assign(n, -1.0f);
			lastInputValue.assign(n, 0.0f);
			impulseLatched.assign(n, false);
			gateStartBeat.assign(n, 0.0f);
			gateValueAtRise.assign(n, 0.0f);
			gateActive.assign(n, false);
			gateWaiting.assign(n, false);
			
			auto o = output.get();
			o.assign(n, 0.0f);
			output = o;
		}
	}

	// ---- PARAMETERS ----
	ofParameter<vector<float>> input;
	ofParameter<float> beatTransport;
	ofParameter<float> qGrid;
	ofParameter<int> mode;
	
	ofParameter<vector<float>> output;
	
	ofEventListeners listeners;
	
	// ---- STATE (per channel) ----
	vector<float> lastSampledValue;    // For Sample/Hold mode
	vector<float> lastGridPos;         // Last grid position seen
	vector<float> lastInputValue;      // For edge detection
	vector<bool> impulseLatched;       // For Impulse mode
	vector<float> gateStartBeat;       // For Gate mode
	vector<float> gateValueAtRise;     // For Gate mode - stores actual gate value
	vector<bool> gateActive;           // For Gate mode
	vector<bool> gateWaiting;          // For Gate mode
};

#endif /* transportQuantizer_h */
