#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>

class Ocurrence : public ofxOceanodeNodeModel {
public:
    Ocurrence() : ofxOceanodeNodeModel("Ocurrence") {}

    void setup() override;

private:
    void processInput();

    ofParameter<std::vector<float>> inputVector;
    ofParameter<std::vector<int>> outputVector;

    // Storage for event listeners to ensure their lifetime
    ofEventListeners listeners;
};
