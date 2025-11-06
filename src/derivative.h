#pragma once
#include "ofxOceanodeNodeModel.h"

class derivative : public ofxOceanodeNodeModel {
public:
	derivative() : ofxOceanodeNodeModel("Derivative") {}
	
	void setup() override {
		addParameter(input.set("Input", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {1.0f}));
		addParameter(scale.set("Scale", 1.0f, 0.0f, 1000.0f));
		
		// listen to input changes
		listener = input.newListener([this](vector<float> &v){
			compute(v);
		});
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	ofParameter<float>         scale;
	
	vector<float> prev;
	ofEventListener listener;
	
	void compute(const vector<float> &v){
		// resize prev if needed
		if(prev.size() != v.size()){
			prev.assign(v.size(), v.size() > 0 ? v[0] : 0.0f);
		}
		
		vector<float> diff;
		diff.resize(v.size());
		
		float s = scale.get();
		for(size_t i = 0; i < v.size(); i++){
			diff[i] = (v[i] - prev[i]) * s;
		}
		
		output = diff;
		prev = v;
	}
};
