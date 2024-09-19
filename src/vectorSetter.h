#ifndef vectorSetter_h
#define vectorSetter_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>

class vectorSetter : public ofxOceanodeNodeModel {
public:
    vectorSetter() : ofxOceanodeNodeModel("Vector Setter") {
        
        description = "Modifies specific elements of an input vector at given indices. Accepts single or multiple indices and can set to a single value or a vector of values. Useful for targeted vector manipulation.";

        addParameter(input.set("Input",
                               vector<float>(1, 0.0f),
                               vector<float>(1, -std::numeric_limits<float>::max()),
                               vector<float>(1, std::numeric_limits<float>::max())));
        addParameter(index.set("Index",
                               vector<int>(1, -1),
                               vector<int>(1, -1),
                               vector<int>(1, INT_MAX)));
        addParameter(setTo.set("Set To",
                               vector<float>(1, 0.0f),
                               vector<float>(1, -std::numeric_limits<float>::max()),
                               vector<float>(1, std::numeric_limits<float>::max())));
        addOutputParameter(output.set("Output",
                                      vector<float>(1, 0.0f),
                                      vector<float>(1, -std::numeric_limits<float>::max()),
                                      vector<float>(1, std::numeric_limits<float>::max())));

        listeners.push(input.newListener([this](vector<float> &v){
            updateOutput();
        }));
        listeners.push(index.newListener([this](vector<int> &v){
            updateOutput();
        }));
        listeners.push(setTo.newListener([this](vector<float> &v){
            updateOutput();
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> index;
    ofParameter<vector<float>> setTo;
    ofParameter<vector<float>> output;

    ofEventListeners listeners;

    void updateOutput() {
        vector<float> result = input.get();
        const vector<int>& indices = index.get();
        vector<float> values = setTo.get();

        // Resize and replicate values if necessary
        if (values.size() == 1) {
            // If setTo is a scalar, replicate it to match the size of indices
            values.resize(indices.size(), values[0]);
        } else if (values.size() < indices.size()) {
            // If setTo is smaller than indices, replicate the last value
            float lastValue = values.back();
            values.resize(indices.size(), lastValue);
        } else if (values.size() > indices.size()) {
            // If setTo is larger than indices, truncate it
            values.resize(indices.size());
        }

        // Apply the changes
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] >= 0 && indices[i] < result.size()) {
                result[indices[i]] = values[i];
            }
        }

        output.set(result);
    }
};

#endif /* vectorSetter_h */
