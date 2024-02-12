//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef Logic_h
#define Logic_h

#include "ofxOceanodeNodeModel.h"

class Logic : public ofxOceanodeNodeModel{
public:
    Logic();
    ~Logic() = default;
    void setup() override;

private:
    void computeLogic();

    ofParameter<vector<float>> input1;
    ofParameter<vector<float>> input2;
    ofParameter<int> operation;  // Keep it as int
    ofParameter<vector<float>> output;
    ofParameter<vector<bool>> outputBool;

    ofEventListeners listeners;
};

#endif /* Logic_h */
