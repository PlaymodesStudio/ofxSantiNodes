#include "ofxOceanodeNodeModel.h"

class vectorSymmetry : public ofxOceanodeNodeModel {
public:
    vectorSymmetry() : ofxOceanodeNodeModel("Vector Symmetry") {}

    void setup() override {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(symmetry_Param.set("Symmetry", 0, 0, 10)); // Assuming symmetry=0 means no change to the input
        addParameter(repEdge.set("RepEdge", false));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        auto calculateSymmetry = [this](vector<float> &vf) {
            if(symmetry_Param == 0) {
                output = input.get(); // No symmetry applied
                return;
            }

            int numOfElements = vf.size();
            vector<float> tempOutput(numOfElements, 0);
            int midPoint = numOfElements / (symmetry_Param + 1);

            for(int i = 0; i < numOfElements; ++i) {
                if(i < midPoint) {
                    tempOutput[i] = vf[i];
                } else {
                    int mirroredIndex = midPoint - (i - midPoint) - 1;
                    // Handle repEdge by adjusting mirroredIndex when applicable
                    if(repEdge && mirroredIndex < 0) {
                        mirroredIndex = 0; // Stick to the first element if out of bounds
                    }
                    mirroredIndex = std::max(0, mirroredIndex); // Ensure mirroredIndex is not out of bounds
                    tempOutput[i] = vf[mirroredIndex];
                }
            }

            if(symmetry_Param >= 1 && repEdge) {
                // Adjust for symmetry and repEdge; refine based on specific behavior needed
                for(int i = midPoint; i < numOfElements; ++i) {
                    tempOutput[i] = tempOutput[midPoint - (i - midPoint) % midPoint - 1];
                }
            }

            output.set(tempOutput);
        };

        listener = input.newListener([this, calculateSymmetry](vector<float> &vf) {
            calculateSymmetry(vf);
        });
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<int> symmetry_Param;
    ofParameter<bool> repEdge;
    ofParameter<vector<float>> output;
    
    ofEventListener listener;
};
