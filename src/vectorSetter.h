#ifndef vectorSetter_h
#define vectorSetter_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>

class vectorSetter : public ofxOceanodeNodeModel {
public:
    vectorSetter() : ofxOceanodeNodeModel("Vector Setter") {
        addParameter(input.set("Input",
                               vector<float>(1, 0.0f),
                               vector<float>(1, -std::numeric_limits<float>::max()),
                               vector<float>(1, std::numeric_limits<float>::max())));
        addParameter(index.set("Index",
                               vector<int>(1, 0),
                               vector<int>(1, 0),
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
        const vector<float>& values = setTo.get();

        for (size_t i = 0; i < indices.size() && i < values.size(); ++i) {
            if (indices[i] >= 0 && indices[i] < result.size()) {
                result[indices[i]] = values[i];
            }
        }

        output.set(result);
    }
};

#endif /* vectorSetter_h */
