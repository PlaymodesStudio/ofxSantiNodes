#ifndef BEATMEASURES_H
#define BEATMEASURES_H

#include "ofxOceanodeNodeModel.h"

class beatMeasures : public ofxOceanodeNodeModel {
public:
    beatMeasures() : ofxOceanodeNodeModel("Beat Measures") {
        addParameter(barDiv.set("BarTh", {4}, {0}, {256}));
        addParameterDropdown(figure, "Figure", 0, {"Straight", "Dotted", "Triplet"});
        addOutputParameter(divResult.set("Div Result", {4}, {0}, {256}));
        addOutputParameter(multResult.set("Mult Result", {4}, {0}, {FLT_MAX}));
        
        description = "Calculates divisions of a bar given a Bar subdivision (nTh) and figure (Straight, Dotted or Triplet). Outputs values suitable for Div and Mult in Phasor.";
        
        listeners.push(barDiv.newListener([this](vector<float> &val){ this->recalculate(); }));
        listeners.push(figure.newListener([this](int &val){ this->recalculate(); }));

    }

    void recalculateBarDiv(const vector<float> &) {
        recalculate();
    }

    void recalculateFigure(const int &) {
        recalculate();
    }

    void recalculate() {
        vector<float> divOut, multOut;
        int figIndex = figure.get(); // Retrieve the integer value

        for(const auto& value : barDiv.get()){
            int divRes, multRes;

            if(figIndex == 0) {  // Straight
                multRes = value;
                divRes = 4;
            }
            else if(figIndex == 1) {  // Dotted
                       multRes = value; // Mult is same as BarTh
                       divRes = 6; // Div is 6 to make it 1.5 times slower than straight
                   }
            else if(figIndex == 2) {  // Triplet
                multRes = value * 3;  // 3 times the base value for triplet
                divRes = 4;  // Keep Div the same, as the Mult will speed it up
            }

            divOut.push_back(divRes);
            multOut.push_back(multRes);
        }

        divResult = divOut;
        multResult = multOut;
    }

    
private:
    ofParameter<vector<float>> barDiv;
    ofParameter<int> figure;
    ofParameter<vector<float>> divResult;
    ofParameter<vector<float>> multResult;
    
    ofEventListeners listeners;
};

#endif /* BEATMEASURES_H */
