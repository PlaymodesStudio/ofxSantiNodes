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
        const auto& inputVals = input.get();
        vector<float> rises(inputVals.size(), 0);
        vector<float> falls(inputVals.size(), 0);
        vector<float> all(inputVals.size(), 0);
        
        if(inputVals.size() > 1) {
            for(size_t i = 1; i < inputVals.size(); i++) {
                float prev = inputVals[i-1];
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
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<float>> riseGates;
    ofParameter<vector<float>> fallGates;
    ofParameter<vector<float>> allGates;
    ofEventListener listener;
};

#endif /* binaryEdgeDetector_h */
