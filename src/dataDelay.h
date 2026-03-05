// dataDelay.h
#pragma once

#include "ofxOceanodeNodeModel.h"
#include <deque>

class dataDelay : public ofxOceanodeNodeModel {
public:
	dataDelay() : ofxOceanodeNodeModel("Data Delay") {
		description = "Delays the input vector by a number of frames. Output reflects the input as it was N frames ago.";
	}

	void setup() override {
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(delayFrames.set("Frames", 0, 0, INT_MAX));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

		listener = delayFrames.newListener([this](int &i) {
			store.resize(i);
		});
	}

	void update(ofEventArgs &a) override {
		store.push_back(input.get());
		if (!store.empty()) {
			output = store[0];
			store.pop_front();
		}
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<int>           delayFrames;
	ofParameter<vector<float>> output;

	ofEventListener            listener;
	std::deque<vector<float>>  store;
};
