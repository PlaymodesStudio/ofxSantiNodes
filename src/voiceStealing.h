#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>

using namespace std;

class voiceStealing : public ofxOceanodeNodeModel{
public:
    voiceStealing() : ofxOceanodeNodeModel("Voice Stealing"){}

    void setup() override;
    void update(ofEventArgs &args); // Ensure this matches your implementation

private:
    // Parameters
    ofParameter<vector<float>> pitchIn;
    ofParameter<vector<float>> gateIn;
    ofParameter<int> outputSize;
    ofParameter<vector<float>> pitchOut;
    ofParameter<vector<float>> gateOut;
    
    // Additional variables for internal logic
    vector<int> voiceAge; // To keep track of the oldest note
    vector<float> lastGateValue; // To remember the last gate values for detecting new notes

    // Method to find an available slot or the oldest slot for a new note
    int findOldestSlot(); //
    int findAvailableOrOldestSlot(const vector<float>& gateOutVector); // Adjusted signature

    // Container to store event listeners to ensure they are properly removed when no longer needed
    ofEventListeners listeners;
};
