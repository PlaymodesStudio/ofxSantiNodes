#ifndef valueIndex_h
#define valueIndex_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>

class valueIndex : public ofxOceanodeNodeModel {
public:
    valueIndex() : ofxOceanodeNodeModel("Value Index") {
        description = "Searches for specific values within an input vector and returns their indices. Accepts multiple search values and outputs corresponding indices. Returns -1 for values not found. Useful for locating elements or checking presence of values in a dataset.";

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
        std::vector<int> indices;

        indices.reserve(values.size());  // Optimize for performance

        for (const auto& value : values) {
            auto it = std::find(input.begin(), input.end(), value);
            if (it != input.end()) {
                indices.push_back(static_cast<int>(std::distance(input.begin(), it)));
            } else {
                indices.push_back(-1);
            }
        }

        outputIndices = indices;
    }

    ofParameter<vector<float>> inputVector;
    ofParameter<vector<float>> searchValues;
    ofParameter<vector<int>> outputIndices;
    ofEventListeners listeners;
};

#endif /* valueIndex_h */
