#ifndef vectorInverter_h
#define vectorInverter_h

#include "ofxOceanodeNodeModel.h"

class vectorInverter : public ofxOceanodeNodeModel {
public:
	vectorInverter() : ofxOceanodeNodeModel("Vector Inverter") {}

	void setup() override {
		description = "Inverts specific indices of a vector based on an invert mask. "
					 "For each index where invert[i] is non-zero, output[i] = 1 - input[i].";
		
		addParameter(input.set("Input", {0}, {0}, {1}));
		addParameter(invert.set("Invert", {0}, {0}, {1}));
		addOutputParameter(output.set("Output", {0}, {0}, {1}));
		
		listeners.push(input.newListener([this](vector<float> &){
			processInversion();
		}));
		
		listeners.push(invert.newListener([this](vector<int> &){
			processInversion();
		}));
	}

private:
	void processInversion() {
		const auto& inputVec = input.get();
		const auto& invertVec = invert.get();
		vector<float> result;
		
		if(inputVec.empty()) {
			output = result;
			return;
		}
		
		result.reserve(inputVec.size());
		
		for(size_t i = 0; i < inputVec.size(); ++i) {
			// If invert vector is shorter, treat missing values as 0 (no inversion)
			bool shouldInvert = (i < invertVec.size()) && (invertVec[i] != 0);
			
			if(shouldInvert) {
				result.push_back(1.0f - inputVec[i]);
			} else {
				result.push_back(inputVec[i]);
			}
		}
		
		output = result;
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<int>> invert;
	ofParameter<vector<float>> output;
	
	ofEventListeners listeners;
};

#endif /* vectorInverter_h */
