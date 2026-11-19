#pragma once
#include "ofxOceanodeNodeModel.h"
#include <cmath>

class derivative : public ofxOceanodeNodeModel {
public:
	derivative() : ofxOceanodeNodeModel("Derivative") {
		description = "Calculates the frame-to-frame change of an input signal, multiplied by Scale. "
		              "Onset Gate outputs 0 on the frame where a value jumps from zero to non-zero "
		              "(or drops back to zero), so the start and end of a sequence do not produce a spike. "
		              "Zero Threshold sets how close to zero still counts as zero for the gate. "
		              "Max Jump outputs 0 for any value that moves further than this in one frame, measured on the "
		              "input before Scale: use it when an entry can switch to another source (a new blob taking "
		              "the place of an old one) and so jumps between two non-zero values. 0 = off. "
		              "Smooth lowpass-filters the output (0 = no smoothing).";
	}

	void setup() override {
		addParameter(input.set("Input", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {1.0f}));
		addParameter(scale.set("Scale", 1.0f, 0.0f, 1000.0f));
		addParameter(onsetGate.set("Onset Gate", false));
		addParameter(zeroThreshold.set("Zero Threshold", 0.0f, 0.0f, 1.0f));
		addParameter(maxJump.set("Max Jump", 0.0f, 0.0f, 1.0f));
		addParameter(smooth.set("Smooth", 0.0f, 0.0f, 0.99f));

		// listen to input changes
		listener = input.newListener([this](vector<float> &v){
			compute(v);
		});
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	ofParameter<float>         scale;
	ofParameter<bool>          onsetGate;
	ofParameter<float>         zeroThreshold;
	ofParameter<float>         maxJump;
	ofParameter<float>         smooth;

	vector<float> prev;
	vector<float> smoothed;
	ofEventListener listener;

	void compute(const vector<float> &v){
		// On a size change there is no valid previous sample per element:
		// start from the current values (zero difference) and clear the filter.
		if(prev.size() != v.size()){
			prev = v;
			smoothed.assign(v.size(), 0.0f);
		}

		float s = scale.get();
		bool gate = onsetGate.get();
		float thr = zeroThreshold.get();
		float jump = maxJump.get();
		float a = ofClamp(smooth.get(), 0.0f, 0.99f);

		for(size_t i = 0; i < v.size(); i++){
			float raw = v[i] - prev[i];
			float d = raw * s;
			if(jump > 0.0f && std::fabs(raw) > jump) d = 0.0f; // discontinuity, not motion
			if(gate){
				bool wasZero = std::fabs(prev[i]) <= thr;
				bool isZero  = std::fabs(v[i]) <= thr;
				if(wasZero != isZero) d = 0.0f; // appearing or disappearing value
			}
			smoothed[i] = a * smoothed[i] + (1.0f - a) * d;
		}

		prev = v;
		output = smoothed;
	}
};
