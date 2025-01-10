//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef FilterDuplicates_h
#define FilterDuplicates_h

#include "ofxOceanodeNodeModel.h"

class filterDuplicates : public ofxOceanodeNodeModel{
public:
    filterDuplicates() : ofxOceanodeNodeModel("Filter Duplicates") {}

    void setup() {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(input.newListener([this](vector<float> &vf){
            std::vector<float> uniqueValues;

            // Use a set to help identify duplicates
            std::set<float> foundValues;
            
            for (const auto &value : vf) {
                if (foundValues.find(value) == foundValues.end()) {
                    uniqueValues.push_back(value);
                    foundValues.insert(value);
                }
            }
            
            output = uniqueValues;
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> output;

    ofEventListeners listeners;
};

#endif /* FilterDuplicates_h */
