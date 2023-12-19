//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef PolyFill_h
#define PolyFill_h

#include "ofxOceanodeNodeModel.h"

class polyFill : public ofxOceanodeNodeModel {
public:
    polyFill() : ofxOceanodeNodeModel("Poly Fill") {}

    void setup() override {
        description = "Fills a polyphonic structure based on a given input vector. It takes an input and repeats its non-zero elements based on the 'nVoices' and 'Poly Step' parameters. 'nVoices' determines the repetition count, and 'Poly Step' defines the interval between these repeated elements.";

        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(nVoices.set("nVoices", 3, 1, 12));
        addParameter(polyStep.set("Poly Step", 1, 1, 12));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push_back(std::make_unique<ofEventListener>(input.newListener([this](vector<float>& vf){
            //Get the values of nVoices and polyStep locally at the time of event arrival
            int voices = nVoices.get();
            int step = polyStep.get();

            vector<float> tempOutput(vf.size(), 0);

            for(int i = 0; i < vf.size(); i++){
                if(vf[i] != 0){
                    for(int j = 0; j < voices; j++){
                        int index = (i + j * step) % vf.size();
                        tempOutput[index] = vf[i];
                    }
                }
            }
            output = tempOutput;
        })));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<int> nVoices;
    ofParameter<int> polyStep;
    ofParameter<vector<float>> output;

    vector<std::unique_ptr<ofEventListener>> listeners;
};

#endif /* PolyFill_h */
