#ifndef indexMonitor_h
#define indexMonitor_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class indexMonitor : public ofxOceanodeNodeModel {
public:
    indexMonitor() : ofxOceanodeNodeModel("Index Monitor") {}

    void setup() {
        description = "Outputs the indices of input vector elements that are different from zero";
        addParameter(input.set("Input", vector<float>(1, 0.0f), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        addOutputParameter(output.set("Output", vector<int>(), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(threshold.set("Threshold", 0.0f, 0.0f, 1.0f));

        listeners.push(input.newListener([this](vector<float> &f) {
            updateOutput();
        }));

        listeners.push(threshold.newListener([this](float &f) {
            updateOutput();
        }));
    }

    void update(ofEventArgs &a) {
        updateOutput();
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> output;
    ofParameter<float> threshold;
    ofEventListeners listeners;

    void updateOutput() {
        const vector<float>& inputVec = input.get();
        vector<int> outputVec;
        outputVec.reserve(inputVec.size());
        for (int i = 0; i < inputVec.size(); ++i) {
            if (inputVec[i] > threshold) {
                outputVec.push_back(i);
            }
        }

        // Ensure the output vector is never empty
        if (outputVec.empty()) {
            outputVec.push_back(-1); 
        }

        output.set(outputVec);
    }
};

#endif /* indexMonitor_h */
