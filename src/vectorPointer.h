//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef vectorPointer_h
#define vectorPointer_h

#include "ofxOceanodeNodeModel.h"
#include <memory>

class vectorPointer : public ofxOceanodeNodeModel {
public:
    vectorPointer() : ofxOceanodeNodeModel("Vector Pointer") {}

    void setup() override;

    void calculate();

private:
    ofParameter<vector<float>> source;
    ofParameter<vector<float>> pointer;
    ofParameter<vector<float>> amps;
    ofParameter<bool> ampTrig;
    ofParameter<vector<float>> output;

    vector<std::unique_ptr<ofEventListener>> listeners;

    // Store previous state of pointer and amps
    vector<float> prev_pointer;
    vector<float> prev_amps;
};

#endif /* vectorPointer_h */
