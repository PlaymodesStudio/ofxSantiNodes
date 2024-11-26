#pragma once

#include "ofxOceanodeNodeModel.h"

class randomWalk : public ofxOceanodeNodeModel {
public:
    randomWalk() : ofxOceanodeNodeModel("Random Walk") {
        description="random walk with boundary biasing";
        addParameter(gate.set("Gate", {0}, {0}, {1}));
        addParameter(maxStep.set("Max Step", {1}, {1}, {10}));
        addParameter(range.set("Range", {12}, {1}, {100}));
        addOutputParameter(output.set("Output", {0}, {0}, {100}));

        lastGateValues.resize(gate->size(), 0);  // Initialize with default values

        listener = gate.newListener([this](vector<int> &vg){
            generateRandomWalk();
        });
    }

private:
    void generateRandomWalk() {
        vector<int> newOutput = output.get();
        while (newOutput.size() < gate.get().size()) {
            newOutput.push_back(0);  // Default starting value
        }

        size_t maxSize = std::max({gate.get().size(), maxStep.get().size(), range.get().size()});
        lastGateValues.resize(maxSize, 0);
        newOutput.resize(maxSize, 0);

        for(size_t i = 0; i < maxSize; i++) {
            if (i < lastGateValues.size() && gate.get()[i] == 1 && lastGateValues[i] == 0) {
                int step = generateStep(newOutput[i],
                                        (i < maxStep.get().size() ? maxStep.get()[i] : maxStep.get()[0]),
                                        (i < range.get().size() ? range.get()[i] : range.get()[0]));
                newOutput[i] += step;
                newOutput[i] = std::max(0, std::min(newOutput[i], (i < range.get().size() ? range.get()[i] : range.get()[0])));  // Ensure value is within [0, range]
            }
            if (i < lastGateValues.size()) {
                lastGateValues[i] = gate.get()[i];
            }
        }
        output = newOutput;
    }

    int generateStep(int currentValue, int maxStepValue, int rangeValue) {
        int step = floor(ofRandom(-maxStepValue, maxStepValue + 1));

        // Adjust step if the result would go out of bounds
        if (currentValue + step < 0) {
            step = abs(step);
        }
        else if (currentValue + step > rangeValue) {
            step = -abs(step);
        }
        return step;
    }

    ofParameter<vector<int>> gate;
    ofParameter<vector<int>> maxStep;
    ofParameter<vector<int>> range;
    ofParameter<vector<int>> output;
    ofEventListener listener;
    vector<int> lastGateValues;
};
