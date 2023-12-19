#ifndef cycleCount_h
#define cycleCount_h

#include "ofxOceanodeNodeModel.h"

class cycleCount : public ofxOceanodeNodeModel {
public:
    cycleCount() : ofxOceanodeNodeModel("Cycle Count") {}

    void setup() override {
        description = "Counts phasor cycles";
        addParameter(input.set("Input", {0}, {0}, {1}));
        addParameter(resetCount.set("Reset"));
        addParameter(resetNext.set("Reset Next"));
        addOutputParameter(countOutput.set("Count", {0}, {0}, {INT_MAX}));

        listeners.push(resetCount.newListener([this](){
            std::fill(fallingEdgeCount.begin(), fallingEdgeCount.end(), 0);
            countOutput = fallingEdgeCount;
        }));

        listeners.push(resetNext.newListener([this](){
            std::fill(shouldResetNextCycle.begin(), shouldResetNextCycle.end(), true);
        }));

        listeners.push(input.newListener([this](vector<float> &vf){
            if(vf.size() != lastInput.size()) {
                lastInput.resize(vf.size(), 0);
                fallingEdgeCount.resize(vf.size(), 0);
                shouldResetNextCycle.resize(vf.size(), false);  // Resize the reset flag vector
            }

            for(int i = 0; i < vf.size(); i++) {
                if(vf[i] < lastInput[i]) { // Detect falling edge
                    if(shouldResetNextCycle[i]) {
                        fallingEdgeCount[i] = 0;
                        shouldResetNextCycle[i] = false;
                    } else {
                        fallingEdgeCount[i]++;
                    }
                }
            }
            lastInput = vf;

            countOutput = fallingEdgeCount;
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<void> resetCount;
    ofParameter<void> resetNext;
    ofParameter<vector<int>> countOutput;

    vector<float> lastInput;
    vector<int> fallingEdgeCount;
    vector<bool> shouldResetNextCycle;  // New vector to track reset flags for each index

    ofEventListeners listeners;
};

#endif /* cycleCount_h */
