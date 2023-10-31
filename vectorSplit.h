#pragma once

#include "ofxOceanodeNodeModel.h"

class split : public ofxOceanodeNodeModel {
public:
    split() : ofxOceanodeNodeModel("Vector Split") {}

    void setup() override {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(min.set("Min", 0, -FLT_MAX, FLT_MAX));
        addParameter(max.set("Max", 1, -FLT_MAX, FLT_MAX));
        addParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
        description="This module splits and resizes the input vector for it to return just the values which are between min and max";
        listeners.push(input.newListener([this](vector<float> &v){
            vector<float> out;
            for (float value : v) {
                if (value >= min && value <= max) {
                    out.push_back(value);
                }
            }
            output = out;
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<float> min, max;
    ofParameter<vector<float>> output;

    ofEventListener listener;
    ofEventListeners listeners;
};
