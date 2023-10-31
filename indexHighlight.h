#pragma once

#include "ofxOceanodeNodeModel.h"

class indexHighlight : public ofxOceanodeNodeModel {
public:
    indexHighlight() : ofxOceanodeNodeModel("Index Highlight") {
        description = "Highlights specific indices of an input vector based on the 'highlight' input.";

        // Setting up the parameters
        addParameter(input.set("Input", {0.0f}, {0.0f}, {FLT_MAX}));
        addParameter(highlight.set("Highlight", {0}, {0}, {INT_MAX}));
        addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {FLT_MAX}));

        listener = highlight.newListener([this](vector<int> &highlightVals){
            processInput();
        });
    }

private:
    void processInput() {
        vector<float> resultOutput = input.get();

        // Set all values to zero initially
        for (auto &val : resultOutput) {
            val = 0.0f;
        }

        // Set the values for highlighted indices
        for (const auto &idx : highlight.get()) {
            if (idx >= 0 && idx < input.get().size()) {
                resultOutput[idx] = input.get()[idx];
            }
        }

        output = resultOutput;
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<int>> highlight;
    ofParameter<vector<float>> output;
    ofEventListener listener;
};
