#ifndef vectorFire_h
#define vectorFire_h

#include "ofxOceanodeNodeModel.h"

class vectorFire : public ofxOceanodeNodeModel{
public:
    vectorFire() : ofxOceanodeNodeModel("Vector Fire"), fireUpdateNeeded(true){
        description = "A conditional vector routing node. It has two triggers: 'Fire' and 'vFire'. When 'Fire' is greater than 0, the entire input vector is routed to the output. 'vFire' serves as an index-wise trigger, routing values from input to output based on changes in its own indices.";

        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(fire.set("Fire", 1, 0, 1));
        addParameter(vFire.set("vFire", {0}, {0}, {1}));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        // Initialize prev_vFire to match initial vFire state
        prev_vFire = vFire.get();

        // vFire listener: responds to changes in vFire parameter
        listener = vFire.newListener([this](vector<float> &vf){
            vector<float> auxInput = input.get();
            vector<float> outputMod = output.get();

            // Ensure outputMod is the same size as the input
            outputMod.resize(auxInput.size());

            // Resize prev_vFire to match vFire size
            prev_vFire.resize(vf.size());

            // For each index in vFire, if its value has changed from the previous state
            // and it is within the size of input, copy the corresponding input value to the output
            for(size_t i = 0; i < vf.size(); i++){
                if(i < auxInput.size() && vf[i] != prev_vFire[i]){
                    outputMod[i] = auxInput[i];
                }
            }

            output = outputMod;
            prev_vFire = vf;

            // Update fire to 0 after the first vFire update
            if (fireUpdateNeeded) {
                fire = 0;
                fireUpdateNeeded = false;
            }
        });

        // fire listener: responds to changes in fire parameter
        // When fire > 0, simply pass input vector to output
        listener2 = fire.newListener([this](float &f){
            if(f > 0){
                output = input;
            }
        });
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<float> fire;
    ofParameter<vector<float>> vFire;
    ofParameter<vector<float>> output;

    vector<float> prev_vFire; // Keeps track of the previous state of vFire

    ofEventListener listener; // Listens for changes in vFire
    ofEventListener listener2; // Listens for changes in fire

    // Flag to indicate if fire needs to be updated to 0 after the first vFire update
    bool fireUpdateNeeded;
};

#endif /* vectorFire_h */
