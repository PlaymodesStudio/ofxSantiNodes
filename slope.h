//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef slope_h
#define slope_h

#include "ofxOceanodeNodeModel.h"

class slope : public ofxOceanodeNodeModel{
public:
    slope() : ofxOceanodeNodeModel("Slope") {}

    void setup() {
        addParameter(x.set("X", {0.5}, {0}, {1}));
        addParameter(y.set("Y", {0.5}, {0}, {1}));
        addParameter(slope_out.set("Slope_out", {0}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(x.newListener([this](vector<float> &vf){
            if(x->size() == y->size() && x->size() > 1){
                std::vector<float> slopes;

                for (int i = 0; i < x->size() - 1; i++) {
                    float deltaX = x->at(i+1) - x->at(i);
                    if(deltaX == 0){
                        slopes.push_back(FLT_MAX);
                    }
                    else{
                        float deltaY = y->at(i+1) - y->at(i);
                        slopes.push_back(deltaY/deltaX);
                    }
                }
                slope_out = slopes;
            }
        }));
    }

private:
    ofParameter<vector<float>> x, y;
    ofParameter<vector<float>> slope_out;

    ofEventListeners listeners;
};

#endif /* slope_h */
