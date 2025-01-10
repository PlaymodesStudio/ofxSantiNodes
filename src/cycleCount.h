#ifndef cycleCount_h
#define cycleCount_h

#include "ofxOceanodeNodeModel.h"

class cycleCount : public ofxOceanodeNodeModel {
public:
    cycleCount() : ofxOceanodeNodeModel("Cycle Count") {}

    void setup() override {
        description = "Counts phasor cycles with pattern-based counting";
        addParameter(input.set("Input", {0}, {0}, {1}));
        addParameter(pattern.set("Pattern", {1}, {0}, {1}));
        addParameter(resetCount.set("Reset"));
        addParameter(resetNext.set("Reset Next"));
        addParameter(mod.set("Mod", 64, 1, 1000));
        addOutputParameter(countOutput.set("Count", {0}, {0}, {INT_MAX}));
        addOutputParameter(phOut.set("Ph Out", {0}, {0}, {1}));
        addOutputParameter(resetOut.set("Reset Out"));

        listeners.push(resetCount.newListener([this](){
            std::fill(fallingEdgeCount.begin(), fallingEdgeCount.end(), 0);
            std::fill(patternPositions.begin(), patternPositions.end(), 0);
            updateCountOutput();
        }));

        listeners.push(resetNext.newListener([this](){
            std::fill(shouldResetNextCycle.begin(), shouldResetNextCycle.end(), true);
        }));

        listeners.push(pattern.newListener([this](vector<int> &vp){
            if(vp.empty()) {
                pattern = vector<int>{1};
            }
        updatePhaseOutput(input.get());
        }));

        listeners.push(input.newListener([this](vector<float> &inVec){
            if(inVec.size() != lastInput.size()) {
                lastInput.resize(inVec.size(), 0);
                fallingEdgeCount.resize(inVec.size(), 0);
                shouldResetNextCycle.resize(inVec.size(), false);
                patternPositions.resize(inVec.size(), 0);
            }

            vector<int> currentPattern = pattern.get();
            if(currentPattern.empty()) currentPattern = {1}; // Safeguard

            for(int i = 0; i < inVec.size(); i++) {
                if(inVec[i] < lastInput[i]) { // Detect falling edge
                    if(shouldResetNextCycle[i]) {
                        fallingEdgeCount[i] = 0;
                        patternPositions[i] = 0;
                        shouldResetNextCycle[i] = false;
                    } else {
                        // Only increment count if the current pattern position is 1
                        if(currentPattern[patternPositions[i]] == 1) {
                            fallingEdgeCount[i]++;
                        }
                        
                        // Move to next pattern position
                        patternPositions[i] = (patternPositions[i] + 1) % currentPattern.size();
                    }
                    resetOut.trigger();
                }
            }
            lastInput = inVec;

            updateCountOutput();
            updatePhaseOutput(inVec);
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> pattern;  // New pattern parameter
    ofParameter<void> resetCount;
    ofParameter<void> resetNext;
    ofParameter<int> mod;
    ofParameter<vector<int>> countOutput;
    ofParameter<vector<float>> phOut;
    ofParameter<void> resetOut;

    vector<float> lastInput;
    vector<int> fallingEdgeCount;
    vector<bool> shouldResetNextCycle;
    vector<int> patternPositions;  // Tracks current position in pattern for each input

    ofEventListeners listeners;

    void updateCountOutput() {
        vector<int> modulatedCountOutput(fallingEdgeCount.size());
        int modValue = mod.get();

        for (size_t i = 0; i < fallingEdgeCount.size(); ++i) {
            modulatedCountOutput[i] = fallingEdgeCount[i] % modValue;
        }

        countOutput = modulatedCountOutput;
    }

    void updatePhaseOutput(const vector<float>& inputPhase) {
        vector<float> phaseOutput(inputPhase.size());
        vector<int> currentPattern = pattern.get();
        if(currentPattern.empty()) currentPattern = {1}; // Safeguard

        for (size_t i = 0; i < inputPhase.size(); ++i) {
            // Output the input phase only when pattern is 1, otherwise output 0
            phaseOutput[i] = currentPattern[patternPositions[i]] == 1 ? inputPhase[i] : 0.0f;
        }

        phOut = phaseOutput;
    }
};

#endif /* cycleCount_h */
