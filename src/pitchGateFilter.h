#ifndef pitchGateFilter_h
#define pitchGateFilter_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class pitchGateFilter : public ofxOceanodeNodeModel {
public:
    pitchGateFilter() : ofxOceanodeNodeModel("pitchGateFilter") {}

    void setup() override {
        description = "Filters duplicate pitch values, keeping unique pitches and merging their gates into a single output gate per pitch.";

        addParameter(inputPitch.set("Input Pitch", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(inputGate.set("Input Gate", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(outputPitch.set("Output Pitch", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(outputGate.set("Output Gate", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(inputPitch.newListener([this](std::vector<float> &) { process(); }));
        listeners.push(inputGate.newListener([this](std::vector<float> &) { process(); }));
    }

private:
    void process() {
        const auto &inPitch = inputPitch.get();
        const auto &inGate = inputGate.get();
        const size_t size = std::min(inPitch.size(), inGate.size());

        std::vector<float> uniquePitches;
        std::vector<float> mergedGates;

        for(size_t i = 0; i < size; i++) {
            auto it = std::find(uniquePitches.begin(), uniquePitches.end(), inPitch[i]);

            if(it == uniquePitches.end()) {
                uniquePitches.push_back(inPitch[i]);
                mergedGates.push_back(inGate[i]);
            } else {
                const size_t index = std::distance(uniquePitches.begin(), it);
                mergedGates[index] = std::max(mergedGates[index], inGate[i]);
            }
        }

        outputPitch = uniquePitches;
        outputGate = mergedGates;
    }

    ofParameter<std::vector<float>> inputPitch;
    ofParameter<std::vector<float>> inputGate;
    ofParameter<std::vector<float>> outputPitch;
    ofParameter<std::vector<float>> outputGate;

    ofEventListeners listeners;
};

#endif /* pitchGateFilter_h */
