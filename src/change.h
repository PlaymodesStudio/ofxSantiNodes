//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef change_h
#define change_h

#include "ofxOceanodeNodeModel.h"

class change : public ofxOceanodeNodeModel {
public:
    change() : ofxOceanodeNodeModel("Change") {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(triggerChanged.set("Trigger"));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
        
        listener = input.newListener([this](vector<float> &vf){
            if(vf != previousInput) {
                previousInput = vf;
                output = vf;
                triggerChanged.trigger();
            }
        });
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> output;
    ofParameter<void> triggerChanged;
    vector<float> previousInput;
    ofEventListener listener;
};

#endif /* change_h */
