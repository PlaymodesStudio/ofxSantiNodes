#ifndef divmult2ms_h
#define divmult2ms_h

#include "ofxOceanodeNodeModel.h"

class divmult2ms : public ofxOceanodeNodeModel {
public:
	divmult2ms() : ofxOceanodeNodeModel("DivMult to MS") {}

	void setup() override {
		description = "Converts musical timing parameters (BPM, division, multiplier) to milliseconds. "
					  "Formula follows the phasor calculation where frequency = (bpm/60) * mult / div. "
					  "Example: BPM=120, div=6, mult=3 means period of 2000ms with 2 beats.";
		
		// Input parameters
		addParameter(bpmInput.set("BPM", 120.0f, 1.0f, 999.0f));
		addParameter(divInput.set("Div", 4.0f, 0.001f, 128.0f));
		addParameter(multInput.set("Mult", 1.0f, 0.001f, 128.0f));
		
		// Output parameters
		addOutputParameter(msOutput.set("Ms", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(beatsOutput.set("Beats", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(frequencyOutput.set("Hz", 0.0f, 0.0f, FLT_MAX));
		
		// Add listeners to recalculate when any input changes
		listeners.push(bpmInput.newListener([this](float &){
			calculateMs();
		}));
		
		listeners.push(divInput.newListener([this](float &){
			calculateMs();
		}));
		
		listeners.push(multInput.newListener([this](float &){
			calculateMs();
		}));
		
		// Initial calculation
		calculateMs();
	}

private:
	void calculateMs() {
		float bpm = bpmInput.get();
		float div = divInput.get();
		float mult = multInput.get();
		
		// Prevent division by zero
		if (bpm <= 0.0f || div <= 0.0f) {
			msOutput = 0.0f;
			beatsOutput = 0.0f;
			frequencyOutput = 0.0f;
			return;
		}
		
		// Calculate frequency following the phasor approach:
		// freq = (bpm/60) * mult / div
		float freq = (bpm / 60.0f) * mult / div;
		
		// Calculate milliseconds from frequency
		// ms = 1000 / freq
		float ms = 1000.0f / freq;
		
		// Calculate beats (time in seconds * beats per second)
		float beats = (ms / 1000.0f) * (bpm / 60.0f);
		
		// Update the output parameters
		msOutput = ms;
		beatsOutput = beats;
		frequencyOutput = freq;
	}

	ofParameter<float> bpmInput;
	ofParameter<float> divInput;
	ofParameter<float> multInput;
	ofParameter<float> msOutput;
	ofParameter<float> beatsOutput;
	ofParameter<float> frequencyOutput;
	ofEventListeners listeners;
};

#endif /* divmult2ms_h */
