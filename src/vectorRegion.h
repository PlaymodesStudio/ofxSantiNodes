#pragma once

#include "ofxOceanodeNodeModel.h"

class vectorRegion : public ofxOceanodeNodeModel {
public:
	vectorRegion() : ofxOceanodeNodeModel("Vector Region") {}

	void setup() override {
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(idxMin.set("Idx Min", 0, 0, INT_MAX));
		addParameter(idxMax.set("Idx Max", 1, 0, INT_MAX));
		addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

		description = "Outputs a sub-vector of the input vector, comprised between the indices defined by Idx Min and Idx Max";

		auto computeOutput = [this]() {
			int minIdx = idxMin;
			int maxIdx = idxMax;
			int regionSize = max(0, maxIdx - minIdx);
			
			const vector<float>& v = input.get();
			
			if (regionSize > 0 && minIdx >= 0 && maxIdx <= (int)v.size() && v.size() > 1) {
				// Valid range within input vector
				vector<float> out(v.begin() + minIdx, v.begin() + maxIdx);
				output = out;
			} else {
				// No valid input or out of range: output zeros of the defined size
				output = vector<float>(regionSize, 0.0f);
			}
		};

		listeners.push(input.newListener([this, computeOutput](vector<float> &v) {
			computeOutput();
		}));

		listeners.push(idxMin.newListener([this, computeOutput](int &val) {
			computeOutput();
		}));

		listeners.push(idxMax.newListener([this, computeOutput](int &val) {
			computeOutput();
		}));

		// Initialize output with correct size
		computeOutput();
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<int> idxMin, idxMax;
	ofParameter<vector<float>> output;

	ofEventListeners listeners;
};
