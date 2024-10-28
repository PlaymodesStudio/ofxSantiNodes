//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef timeBetweenEvents_h
#define timeBetweenEvents_h

#include "ofxOceanodeNodeModel.h"

class TimeBetweenEvents : public ofxOceanodeNodeModel {
public:
    TimeBetweenEvents() : ofxOceanodeNodeModel("TimeBtwEv")
    {
        addParameter(input.set("Input", {0}, {0}, {1}));
        addParameter(verbose.set("Verbose",false));
        addOutputParameter(output.set("Output", 0, -FLT_MAX, FLT_MAX));
    
        // Setting up the listener for input parameter changes
        listener = input.newListener([this](vector<float> &vi)
        {
            float elapsedTime;
            float currentTime = ofGetElapsedTimef();
            elapsedTime = currentTime - lastTime;
            if(verbose)
            {
                ofLog() << "[" << currentTime << " s. // " /*<< std::setprecision(2)*/ << int(elapsedTime*1000.0f) << " Ms.]"  << " : " ;
                for(int i=0;i<vi.size();i++)
                {
                    ofLog() << ofToString(vi[i]).c_str() << "," ;
                }
                ofLog() << endl;
            }
            
            // Updating the output parameter
            output = elapsedTime;
            lastTime = currentTime;
        });

    }

private:
    ofParameter<vector<float>> input;
    ofParameter<float> output;
    float lastTime;
    //vector<int> previousInput;
    ofEventListener listener;

    vector<int> lastInputState;
    ofParameter<bool> verbose;
};

#endif /* timeBetweenEvents_h */
