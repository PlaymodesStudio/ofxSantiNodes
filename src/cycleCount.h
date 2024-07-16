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
        addParameter(mod.set("Mod", 64, 1, 1000)); 
        addOutputParameter(countOutput.set("Count", {0}, {0}, {INT_MAX}));
        addOutputParameter(resetOut.set("Reset Out"));

        listeners.push(resetCount.newListener([this](){
            std::fill(fallingEdgeCount.begin(), fallingEdgeCount.end(), 0);
            updateCountOutput();
        }));

        listeners.push(resetNext.newListener([this](){
            std::fill(shouldResetNextCycle.begin(), shouldResetNextCycle.end(), true);
        }));

        listeners.push(input.newListener([this](vector<float> &vf){
            if(vf.size() != lastInput.size()) {
                lastInput.resize(vf.size(), 0);
                fallingEdgeCount.resize(vf.size(), 0);
                shouldResetNextCycle.resize(vf.size(), false);
            }

            for(int i = 0; i < vf.size(); i++) {
                if(vf[i] < lastInput[i]) { // Detect falling edge
                    if(shouldResetNextCycle[i]) {
                        fallingEdgeCount[i] = 0;
                        shouldResetNextCycle[i] = false;
                    } else {
                        fallingEdgeCount[i]++;
                    }
                    resetOut.trigger(); // Trigger the 'Reset Out' event
                }
            }
            lastInput = vf;

            updateCountOutput();
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<void> resetCount;
    ofParameter<void> resetNext;
    ofParameter<int> mod; // Modulo parameter
    ofParameter<vector<int>> countOutput;
    ofParameter<void> resetOut; // 'Reset Out' void output parameter

    vector<float> lastInput;
    vector<int> fallingEdgeCount;
    vector<bool> shouldResetNextCycle;

    ofEventListeners listeners;

    void updateCountOutput() {
        vector<int> modulatedCountOutput(fallingEdgeCount.size());
        int modValue = mod.get();

        for (size_t i = 0; i < fallingEdgeCount.size(); ++i) {
            modulatedCountOutput[i] = fallingEdgeCount[i] % modValue;
        }

        countOutput = modulatedCountOutput;
    }
};

#endif /* cycleCount_h */

