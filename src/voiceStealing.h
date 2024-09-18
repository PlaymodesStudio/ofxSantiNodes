#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>

class voiceStealing : public ofxOceanodeNodeModel {
public:
    voiceStealing();
    void setup() override;
    void update(ofEventArgs &args) override;

private:
    ofParameter<std::vector<float>> inputPitch;
    ofParameter<std::vector<float>> inputGate;
    ofParameter<int> outputSize;
    ofParameter<std::vector<float>> outputPitch;
    ofParameter<std::vector<float>> outputGate;

    std::vector<int> voiceAge;
    ofEventListeners listeners;

    void resizeOutputs(int size);
    size_t findOldestVoice(const std::vector<float>& gates);  // Changed from findOldestFreeSlot
};
