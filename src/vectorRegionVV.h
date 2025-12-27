#pragma once

#include "ofxOceanodeNodeModel.h"

class vectorRegionVV : public ofxOceanodeNodeModel {
public:
	vectorRegionVV() : ofxOceanodeNodeModel("Vector Region VV") {}

	void setup() override {
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(idxMin.set("Idx Min", {0}, {0}, {INT_MAX}));
		addParameter(idxMax.set("Idx Max", {1}, {0}, {INT_MAX}));
		addOutputParameter(output.set("Output", {{0.0f}}, {{-FLT_MAX}}, {{FLT_MAX}}));

		description = "Outputs a vector of sub-vectors from the input vector. Each sub-vector is defined by corresponding indices in Idx Min and Idx Max vectors";

		listeners.push(input.newListener([this](vector<float> &v) {
			processOutput(v);
		}));
		
		listeners.push(idxMin.newListener([this](vector<int> &v) {
			processOutput(input.get());
		}));
		
		listeners.push(idxMax.newListener([this](vector<int> &v) {
			processOutput(input.get());
		}));
	}

private:
	void processOutput(const vector<float> &v) {
		auto minIndices = idxMin.get();
		auto maxIndices = idxMax.get();
		
		// Get the size to iterate (minimum of both vectors)
		size_t numRegions = min(minIndices.size(), maxIndices.size());
		
		vector<vector<float>> result;
		result.reserve(numRegions);
		
		for (size_t i = 0; i < numRegions; i++) {
			int minIdx = minIndices[i];
			int maxIdx = maxIndices[i];
			
			// Validate indices
			if (maxIdx > minIdx && minIdx >= 0 && maxIdx <= v.size()) {
				vector<float> subVector(v.begin() + minIdx, v.begin() + maxIdx);
				result.push_back(subVector);
			} else {
				// Add empty vector for invalid indices
				result.push_back(vector<float>{});
			}
		}
		
		output = result;
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<int>> idxMin, idxMax;
	ofParameter<vector<vector<float>>> output;

	ofEventListeners listeners;
};
