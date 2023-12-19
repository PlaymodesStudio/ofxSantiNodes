//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef vectorTimer_h
#define vectorTimer_h

#include "ofxOceanodeNodeModel.h"

class vectorTimer : public ofxOceanodeNodeModel {
public:
    vectorTimer() : ofxOceanodeNodeModel("Vector Timer") {}
    
    // Destructor
       ~vectorTimer() override {
           ofRemoveListener(ofEvents().update, this, &vectorTimer::update);
           listeners.clear();
       }
    
    void setup() override {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(ms.set("ms", {100}, {1}, {10000}));
        addOutputParameter(output.set("Output", {0}, {0}, {1}));

        listeners.push_back(std::make_unique<ofEventListener>(input.newListener([this](vector<float>& vf) {
            // Resize if necessary
            if (vf.size() != previousInput.size()) {
                previousInput.resize(vf.size(), 0);
                timerEnd.resize(vf.size(), 0);
                vector<float> tempOutput = output.get();
                tempOutput.resize(vf.size(), 0);
                output = tempOutput;
            }

            // Check for changes
            for (int i = 0; i < vf.size(); ++i) {
                if (vf[i] != previousInput[i]) {
                    timerEnd[i] = ofGetElapsedTimeMillis() + ms.get()[i];
                    vector<float> tempOutput = output.get();
                    tempOutput[i] = vf[i];
                    output = tempOutput;
                }
            }
            previousInput = vf;
        })));

        ofAddListener(ofEvents().update, this, &vectorTimer::update);
    }

    void update(ofEventArgs& args) {
        auto currentTime = ofGetElapsedTimeMillis();
        bool outputModified = false;
        vector<float> tempOutput = output.get();
        for (int i = 0; i < timerEnd.size(); ++i) {
            if (timerEnd[i] != 0 && currentTime >= timerEnd[i]) {
                tempOutput[i] = 0;
                timerEnd[i] = 0;
                outputModified = true;
            }
        }
        if (outputModified) {
            output = tempOutput;
        }
    }

    void exit(ofEventArgs& args) {
        ofRemoveListener(ofEvents().update, this, &vectorTimer::update);
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> ms;
    ofParameter<vector<float>> output;

    vector<std::unique_ptr<ofEventListener>> listeners;

    vector<float> previousInput;
    vector<unsigned long long> timerEnd;  // store end times in milliseconds
};

#endif /* vectorTimer_h */
