#ifndef ramp_h
#define ramp_h

#include "ofxOceanodeNodeModel.h"

class ramp : public ofxOceanodeNodeModel{
public:
    ramp() : ofxOceanodeNodeModel("Ramp"), startTime(0){
        addParameter(goTo.set("Go To", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(ms.set("ms", 1000, 0, 5000)); // Let's limit it to 5 seconds for this example
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        listener = goTo.newListener([this](vector<float> &newGoTo){
            // Set the starting values
            startValues = currentValues;
            
            // If currentValues is empty (initial state), set it to newGoTo
            if(currentValues.empty()){
                currentValues = newGoTo;
            }

            // Resize the targetValues to match the newGoTo size
            targetValues.resize(newGoTo.size());

            // Set the target values
            for(size_t i = 0; i < newGoTo.size(); i++){
                targetValues[i] = newGoTo[i];
            }

            // Record the start time
            startTime = ofGetElapsedTimeMillis();
        });

        ofAddListener(ofEvents().update, this, &ramp::onUpdate);
    }
    
    ~ramp(){
        ofRemoveListener(ofEvents().update, this, &ramp::onUpdate);
    }

    void onUpdate(ofEventArgs &args){
        // Calculate the ratio of time elapsed
        float ratio = float(ofGetElapsedTimeMillis() - startTime) / ms.get();
        
        if(ratio > 1) ratio = 1;

        // Resize the startValues and currentValues vectors to match the targetValues size
        startValues.resize(targetValues.size(), 0); // initialize new values to 0
        currentValues.resize(targetValues.size(), 0); // initialize new values to 0

        // Interpolate each value
        for(size_t i = 0; i < targetValues.size(); i++){
            currentValues[i] = ofLerp(startValues[i], targetValues[i], ratio);
        }

        output = currentValues;
    }


private:
    ofParameter<vector<float>> goTo;
    ofParameter<int> ms;
    ofParameter<vector<float>> output;

    vector<float> startValues;      // Starting values for the interpolation
    vector<float> targetValues;     // Target values for the interpolation
    vector<float> currentValues;    // Current interpolated values

    uint64_t startTime;             // The time when the interpolation started

    ofEventListener listener;
};

#endif /* ramp_h */
