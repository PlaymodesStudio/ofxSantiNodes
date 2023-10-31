#pragma once
#include "ofxOceanodeNodeModel.h"

class resetPhasor : public ofxOceanodeNodeModel{
public:
    resetPhasor() : ofxOceanodeNodeModel("Phasor Reset"){
        addParameter(input.set("Input", 0, 0, 1));
        addOutputParameter(output.set("Output", false));
    }
    
    void update(ofEventArgs &a) override{
        float inputValue = input.get();
        if(inputValue != 0) {
            output = true;
        } else {
            output = false;
        }
    }

private:
    ofParameter<float> input;
    ofParameter<bool> output;
};
