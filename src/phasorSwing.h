#ifndef phasorSwing_h
#define phasorSwing_h

#include "ofxOceanodeNodeModel.h"

class phasorSwing : public ofxOceanodeNodeModel {
public:
    phasorSwing() : ofxOceanodeNodeModel("Phasor Swing") {}
    
    void setup() {
        description = "Adds TR-909 style swing to a phasor input";
        
        addParameter(phaseIn.set("Ph In", {0}, {0}, {1}));
        addParameter(swing.set("Swing", 0.5, 0.0, 1.0));
        addParameter(steps.set("Steps", 16, 2, 64));
        addOutputParameter(phaseOut.set("Ph Out", {0}, {0}, {1}));
        
        listeners.push(phaseIn.newListener([this](vector<float> &ph){
            calculate();
        }));
        
        listeners.push(swing.newListener([this](float &s){
            calculate();
        }));
        
        listeners.push(steps.newListener([this](int &st){
            calculate();
        }));
    }

private:
    ofParameter<vector<float>> phaseIn;
    ofParameter<float> swing;
    ofParameter<int> steps;
    ofParameter<vector<float>> phaseOut;
    
    ofEventListeners listeners;
    
    void calculate() {
        vector<float> swungPhase;
        float swingAmount = swing.get();
        int stepsCount = steps.get();

        for(float ph : phaseIn.get()) {
            int currentStep = int(ph * stepsCount);
            float stepSize = 1.0f / stepsCount;
            float swingTime = stepSize * swingAmount;
            
            if(currentStep % 2 == 0) { // Even steps (0, 2, 4, ...)
                float t = fmod(ph, stepSize) / stepSize;
                float swungPh = currentStep * stepSize + t * (stepSize + swingTime);
                swungPhase.push_back(fmod(swungPh, 1.0f));
            } else { // Odd steps (1, 3, 5, ...)
                float t = fmod(ph - stepSize, stepSize) / stepSize;
                float swungPh = currentStep * stepSize + swingTime + t * (stepSize - swingTime);
                swungPhase.push_back(fmod(swungPh, 1.0f));
            }
        }
        
        phaseOut = swungPhase;
    }
};

#endif /* phasorSwing_h */
