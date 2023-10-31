#pragma once

#include "ofxOceanodeNodeModel.h"

class eventGate : public ofxOceanodeNodeModel {
public:
    eventGate() : ofxOceanodeNodeModel("Event Gate") {
        description="The Event Gate node monitors its float vector input for changes, producing a gate signal (a value of 1 followed by 0) in response. The Frame toggle determines if the gate spans two frames (ON) or is instantaneous within one frame (OFF). Ideal for generating triggers based on input alterations.";
        addParameter(input.set("Input", {0.0f}, {0.0f}, {FLT_MAX}));
        addParameter(frameMode.set("Frame", true));
        addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {1.0f}));

        listener = input.newListener([this](vector<float> &inVals){
            detectChangesAndGenerateGate(inVals);
        });
    }

private:
    void detectChangesAndGenerateGate(const vector<float>& inVals) {
        vector<float> outVals(inVals.size(), 0.0f);

        // Ensure that the lastInputValues vector is the same size as the input
        if (lastInputValues.size() != inVals.size()) {
            lastInputValues.resize(inVals.size(), 0.0f);
        }

        for (size_t i = 0; i < inVals.size(); i++) {
            if (inVals[i] != lastInputValues[i]) {
                outVals[i] = 1.0f;
            }
        }

        output = outVals;

        if (!frameMode) {
            // Reset output immediately if not in frame mode
            output = vector<float>(inVals.size(), 0.0f);
        }

        lastInputValues = inVals;
    }

    ofParameter<vector<float>> input;
    ofParameter<bool> frameMode;
    ofParameter<vector<float>> output;
    ofEventListener listener;
    vector<float> lastInputValues;
};
