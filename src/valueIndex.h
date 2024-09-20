#ifndef valueIndex_h
#define valueIndex_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>

class valueIndex : public ofxOceanodeNodeModel {
public:
    valueIndex() : ofxOceanodeNodeModel("Value Index") {
        description = "Searches for specific values within an input vector and returns all their indices. Accepts multiple search values and outputs corresponding indices in a flat vector, using -1 as a separator between different search values. Returns an empty vector if no values are found.";

        addParameter(inputVector.set("Input",
                                     {0.0f},
                                     {-std::numeric_limits<float>::max()},
                                     {std::numeric_limits<float>::max()}));
        addParameter(searchValues.set("Values",
                                      {0.0f},
                                      {-std::numeric_limits<float>::max()},
                                      {std::numeric_limits<float>::max()}));
        addOutputParameter(outputIndices.set("Output",
                                             {-1},
                                             {INT_MIN},
                                             {INT_MAX}));

        listeners.push(inputVector.newListener([this](vector<float> &){
            computeIndices();
        }));
        listeners.push(searchValues.newListener([this](vector<float> &){
            computeIndices();
        }));
    }

    ~valueIndex(){};

private:
    void computeIndices() {
        const auto& input = inputVector.get();
        const auto& values = searchValues.get();
        std::vector<int> flatIndices;

        for (const auto& value : values) {
            bool foundValue = false;
            for (int i = 0; i < input.size(); ++i) {
                if (input[i] == value) {
                    flatIndices.push_back(i);
                    foundValue = true;
                }
            }
            if (foundValue || !flatIndices.empty()) {
                flatIndices.push_back(-1); // Add separator
            }
        }

        if (!flatIndices.empty() && flatIndices.back() == -1) {
            flatIndices.pop_back(); // Remove trailing separator if exists
        }

        outputIndices = flatIndices;
    }

    ofParameter<vector<float>> inputVector;
    ofParameter<vector<float>> searchValues;
    ofParameter<vector<int>> outputIndices;
    ofEventListeners listeners;
};

#endif /* valueIndex_h */
