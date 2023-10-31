// quantizeVector.h

#ifndef quantize_h
#define quantize_h

#include "ofxOceanodeNodeModel.h"

class quantize : public ofxOceanodeNodeModel {
public:
    quantize() : ofxOceanodeNodeModel("Quantize") {}

    void setup() override {
        description = "Quantizes each value in the input vector to the nearest value specified in the qList vector.";

        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(qList.set("qList", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(input.newListener([this](vector<float>& vf) {
            calculate();
        }));
        
        listeners.push(qList.newListener([this](vector<float>& vf) {
            calculate();
        }));
    }

    void calculate() {
        vector<float> out;
        for(const auto &value : input.get()){
            float closestDist = FLT_MAX;
            float closestValue = 0;
            for(const auto &qValue : qList.get()){
                float dist = abs(value - qValue);
                if(dist < closestDist){
                    closestDist = dist;
                    closestValue = qValue;
                }
            }
            out.push_back(closestValue);
        }
        output = out;
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> qList;
    ofParameter<vector<float>> output;

    ofEventListeners listeners;
};

#endif /* quantizeVector_h */
