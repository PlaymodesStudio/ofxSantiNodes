#pragma once

#include "ofxOceanodeNodeModel.h"

class vectorRegion : public ofxOceanodeNodeModel {
public:
    vectorRegion() : ofxOceanodeNodeModel("Vector Region") {}

    void setup() override {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(idxMin.set("Idx Min", 0, 0, INT_MAX));
        addParameter(idxMax.set("Idx Max", 1, 0, INT_MAX));
        addParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        description = "Outputs a sub-vector of the input vector, comprised between the indices defined by Idx Min and Idx Max";

        listeners.push(input.newListener([this](vector<float> &v) {
            if (idxMax > idxMin && idxMin >= 0 && idxMax <= v.size()) {
                vector<float> out(v.begin() + idxMin, v.begin() + idxMax);
                output = out;
            } else {
                // In case the indices are not valid, clear the output or handle as needed.
                output.set(vector<float>{});
            }
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<int> idxMin, idxMax;
    ofParameter<vector<float>> output;

    ofEventListener listener;
    ofEventListeners listeners;
};
