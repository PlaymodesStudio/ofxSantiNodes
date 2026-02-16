#ifndef indexRouter_h
#define indexRouter_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class indexRouter : public ofxOceanodeNodeModel {
public:
	indexRouter() : ofxOceanodeNodeModel("Index Router") {}

	void setup() override {
		description = "Routes input values to specified output indices. Output size is determined by the maximum index value + 1. Missing indices are filled with 0 or -1.";

		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(indices.set("Indices", {0}, {0}, {INT_MAX}));
		addParameter(vectorSize.set("VecSize", -1, -1, INT_MAX));
		addParameter(useMinusOne.set("-1", false));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

		listeners.push(input.newListener([this](vector<float> &vf){
			processRouting();
		}));

		listeners.push(indices.newListener([this](vector<int> &vi){
			processRouting();
		}));

		listeners.push(vectorSize.newListener([this](int &i){
			processRouting();
		}));

		listeners.push(useMinusOne.newListener([this](bool &b){
			processRouting();
		}));
	}

private:
	void processRouting() {
		const auto& inputVec = input.get();
		const auto& indicesVec = indices.get();

		// If either input is empty, return empty output
		if (inputVec.empty() || indicesVec.empty()) {
			output = vector<float>();
			return;
		}

		// Determine output size
		int outputSize;
		if (vectorSize == -1) {
			// Dynamic sizing: use maximum index value + 1
			int maxIndex = *std::max_element(indicesVec.begin(), indicesVec.end());
			outputSize = maxIndex + 1;
		} else {
			// Fixed sizing: use specified vector size
			outputSize = vectorSize;
		}

		// Create output vector filled with default value
		vector<float> result(outputSize, useMinusOne ? -1.0f : 0.0f);

		// Map input values to their corresponding output indices
		size_t mapCount = std::min(inputVec.size(), indicesVec.size());
		for (size_t i = 0; i < mapCount; ++i) {
			int targetIndex = indicesVec[i];
			if (targetIndex >= 0 && targetIndex < outputSize) {
				result[targetIndex] = inputVec[i];
			}
		}

		output = result;
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<int>> indices;
	ofParameter<int> vectorSize;
	ofParameter<bool> useMinusOne;
	ofParameter<vector<float>> output;

	ofEventListeners listeners;
};

#endif /* indexRouter_h */
