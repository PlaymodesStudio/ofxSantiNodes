#ifndef binaryEdgeDetector_h
#define binaryEdgeDetector_h

#include "ofxOceanodeNodeModel.h"

class binaryEdgeDetector : public ofxOceanodeNodeModel {
public:
    binaryEdgeDetector() : ofxOceanodeNodeModel("Binary Edge Detector") {}

    void setup() override {
        description = "Detects rising and falling edges in binary signals (0→1 and 1→0 transitions)";
        
        addParameter(input.set("Input", {0}, {0}, {1}));
        addOutputParameter(riseGates.set("Rise Gates", {0}, {0}, {1}));
        addOutputParameter(fallGates.set("Fall Gates", {0}, {0}, {1}));
        addOutputParameter(allGates.set("All Gates", {0}, {0}, {1}));
        
        listener = input.newListener([this](vector<float> &vals){
            process();
        });
    }

private:
    void process() {
        vector<float> inputVals = input.get();
        
        
        if(lastValues.size()!=input.get().size())
        {
            lastValues.resize(input.get().size(),0);
        }
        
        vector<float> rises(inputVals.size(), 0);
        vector<float> falls(inputVals.size(), 0);
        vector<float> all(inputVals.size(), 0);
        
        if(inputVals.size() > 1) 
        {
            for(size_t i = 0; i < inputVals.size(); i++)
            {
                float prev = lastValues[i];
                float curr = inputVals[i];
                
                // Rising edge (0→1)
                if(prev == 0 && curr == 1) {
                    rises[i] = 1;
                    all[i] = 1;
                }
                // Falling edge (1→0)
                else if(prev == 1 && curr == 0) {
                    falls[i] = 1;
                    all[i] = 1;
                }
            }
        }
        
        riseGates = rises;
        fallGates = falls;
        allGates = all;

        // copy the actual values to the lastValues for next iteration
        lastValues = inputVals;
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<float>> riseGates;
    ofParameter<vector<float>> fallGates;
    ofParameter<vector<float>> allGates;
    ofEventListener listener;
    
    vector<float> lastValues;
};

#endif /* binaryEdgeDetector_h */
