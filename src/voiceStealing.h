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
    
    // New auxiliary parameters
    ofParameter<std::vector<float>> inputAux1;
    ofParameter<std::vector<float>> inputAux2;
    ofParameter<std::vector<float>> inputAux3;
    ofParameter<std::vector<float>> inputAux4;
    
    ofParameter<std::vector<float>> outputAux1;
    ofParameter<std::vector<float>> outputAux2;
    ofParameter<std::vector<float>> outputAux3;
    ofParameter<std::vector<float>> outputAux4;

    std::vector<int> voiceAge;
    ofEventListeners listeners;

    void resizeOutputs(int size);
    size_t findOldestVoice(const std::vector<float>& gates);
};
