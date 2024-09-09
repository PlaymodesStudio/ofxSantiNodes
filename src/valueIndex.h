#pragma once

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class valueIndex : public ofxOceanodeNodeModel {
public:
    valueIndex() : ofxOceanodeNodeModel("Value Index") {
        description = "Searches for values in the input vector and outputs all their indices.";

        // Setting up the parameters
        addParameter(input.set("Input", {0.0f}, {0.0f}, {FLT_MAX}));
        addParameter(values.set("Values", {0.0f}, {0.0f}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {vector<int>()}));

        listener = input.newListener([this](vector<float> &){
            processInput();
        });
        listener2 = values.newListener([this](vector<float> &){
            processInput();
        });
    }

private:
    void processInput() {
        const vector<float>& inputVec = input.get();
        const vector<float>& valuesVec = values.get();
        vector<vector<int>> resultOutput(valuesVec.size());

        for (size_t i = 0; i < valuesVec.size(); ++i) {
            float value = valuesVec[i];
            vector<int> indices;
            
            for (size_t j = 0; j < inputVec.size(); ++j) {
                if (inputVec[j] == value) {
                    indices.push_back(j);
                }
            }
            
            if (indices.empty()) {
                indices.push_back(-1);
            }
            
            resultOutput[i] = indices;
        }

        output = resultOutput;
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<float>> values;
    ofParameter<vector<vector<int>>> output;
    ofEventListener listener;
    ofEventListener listener2;
};
