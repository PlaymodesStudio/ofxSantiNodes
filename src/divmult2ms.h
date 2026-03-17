#ifndef divmult2ms_h
#define divmult2ms_h

#include "ofxOceanodeNodeModel.h"

class divmult2ms : public ofxOceanodeNodeModel {
public:
	divmult2ms() : ofxOceanodeNodeModel("DivMult to MS") {}

	void setup() override {
		description = "Converts musical timing parameters (BPM, division, multiplier) to milliseconds. "
					  "Formula follows the phasor calculation where frequency = (bpm/60) * mult / div. "
					  "Example: BPM=120, div=6, mult=3 means period of 2000ms with 2 beats. "
					  "Accepts scalar or vector inputs; scalars are broadcast to match vector size.";

		// Input parameters
		addParameter(bpmInput.set("BPM", {120.0f}, {1.0f}, {999.0f}));
		addParameter(divInput.set("Div", {4.0f}, {0.001f}, {128.0f}));
		addParameter(multInput.set("Mult", {1.0f}, {0.001f}, {128.0f}));

		// Output parameters
		addOutputParameter(msOutput.set("Ms", {0.0f}, {0.0f}, {FLT_MAX}));
		addOutputParameter(beatsOutput.set("Beats", {0.0f}, {0.0f}, {FLT_MAX}));
		addOutputParameter(frequencyOutput.set("Hz", {0.0f}, {0.0f}, {FLT_MAX}));

		listeners.push(bpmInput.newListener([this](vector<float> &){ calculateMs(); }));
		listeners.push(divInput.newListener([this](vector<float> &){ calculateMs(); }));
		listeners.push(multInput.newListener([this](vector<float> &){ calculateMs(); }));

		calculateMs();
	}

private:
	void calculateMs() {
		const vector<float> &bpmVec  = bpmInput.get();
		const vector<float> &divVec  = divInput.get();
		const vector<float> &multVec = multInput.get();

		size_t n = max({bpmVec.size(), divVec.size(), multVec.size()});
		if(n == 0) return;

		auto get = [](const vector<float> &v, size_t i) {
			return v.size() == 1 ? v[0] : v[i];
		};

		vector<float> ms(n), beats(n), freq(n);
		for(size_t i = 0; i < n; i++){
			float bpm  = get(bpmVec,  i);
			float div  = get(divVec,  i);
			float mult = get(multVec, i);

			if(bpm <= 0.0f || div <= 0.0f){
				ms[i] = beats[i] = freq[i] = 0.0f;
				continue;
			}

			float f    = (bpm / 60.0f) * mult / div;
			ms[i]      = 1000.0f / f;
			beats[i]   = (ms[i] / 1000.0f) * (bpm / 60.0f);
			freq[i]    = f;
		}

		msOutput        = ms;
		beatsOutput     = beats;
		frequencyOutput = freq;
	}

	ofParameter<vector<float>> bpmInput;
	ofParameter<vector<float>> divInput;
	ofParameter<vector<float>> multInput;
	ofParameter<vector<float>> msOutput;
	ofParameter<vector<float>> beatsOutput;
	ofParameter<vector<float>> frequencyOutput;
	ofEventListeners listeners;
};

#endif /* divmult2ms_h */
