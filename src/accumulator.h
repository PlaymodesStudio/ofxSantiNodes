#pragma once
#include "ofxOceanodeNodeModel.h"
#include <cfloat>

class accumulator : public ofxOceanodeNodeModel {
public:
	accumulator() : ofxOceanodeNodeModel("Accumulator") {
		description = "Running sum of the incoming values. Every time the input receives a value it is added (per index) to the accumulated total. Reset sets the total back to zero.";
	}

	void setup() override {
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(reset.set("Reset"));
		addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

		listeners.push(input.newListener([this](vector<float> &v){
			// Follow the input size: kept indices keep their sum, new ones start at zero
			if(sum.size() != v.size()) sum.resize(v.size(), 0.0);
			vector<float> out(v.size());
			for(size_t i = 0; i < v.size(); i++){
				sum[i] += v[i];
				out[i] = (float)sum[i];
			}
			output = out;
		}));

		listeners.push(reset.newListener([this](){
			std::fill(sum.begin(), sum.end(), 0.0);
			output = vector<float>(std::max<size_t>(sum.size(), 1), 0.0f);
		}));
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<void>          reset;
	ofParameter<vector<float>> output;

	vector<double> sum; // double so long runs don't lose precision
	ofEventListeners listeners;
};
