#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>
#include <float.h>
#include <limits.h>

class vectorExtract : public ofxOceanodeNodeModel {
public:
    vectorExtract() : ofxOceanodeNodeModel("Vector Extract"){
        description = "Extracts values from input vector based on binary gates.\n"
                     "Input: Vector of floats to extract from\n"
                     "Idx Gates: Vector of 0s and 1s (binary mask)\n"
                     "Output: Vector containing input values where gate=1";
        
        // Setup parameters with proper limits
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(idxGates.set("Idx Gates", {0}, {0}, {1}));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
        
        // Setup listeners
        listeners.push(input.newListener([this](const vector<float>& _){
            computeOutput();
        }));
        listeners.push(idxGates.newListener([this](const vector<int>& _){
            computeOutput();
        }));
    }
    
private:
    void computeOutput() {
        vector<float> result;
        
        // Get references to input vectors
        const vector<float>& inputVec = input.get();
        const vector<int>& gatesVec = idxGates.get();
        
        // Calculate the minimum size between input and gates
        size_t minSize = std::min(inputVec.size(), gatesVec.size());
        
        // Reserve space for worst case (all gates = 1)
        result.reserve(minSize);
        
        // Extract values where gate = 1
        for(size_t i = 0; i < minSize; ++i) {
            if(gatesVec[i] == 1) {
                result.push_back(inputVec[i]);
            }
        }
        
        // Set output
        output.set(result);
    }
    
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> idxGates;
    ofParameter<vector<float>> output;
    ofEventListeners listeners;
};
