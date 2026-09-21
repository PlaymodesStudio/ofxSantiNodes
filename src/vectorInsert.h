#pragma once

#include "ofxOceanodeNodeModel.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <vector>

class vectorInsert : public ofxOceanodeNodeModel {
public:
    vectorInsert() : ofxOceanodeNodeModel("Vector Insert") {
        description = "Inserts all values from Insert into Input at the zero-based Idx position. "
                      "Idx is clamped between 0 and the size of Input, so 0 prepends and the "
                      "input size appends.";

        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(insert.set("Insert", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(idx.set("Idx", 0, 0, INT_MAX));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(input.newListener([this](const std::vector<float>&) {
            computeOutput();
        }));
        listeners.push(insert.newListener([this](const std::vector<float>&) {
            computeOutput();
        }));
        listeners.push(idx.newListener([this](int&) {
            computeOutput();
        }));

        computeOutput();
    }

private:
    void computeOutput() {
        const std::vector<float>& inputValues = input.get();
        const std::vector<float>& insertValues = insert.get();
        const std::size_t insertPosition = std::min(
            static_cast<std::size_t>(std::max(0, idx.get())),
            inputValues.size());

        std::vector<float> result;
        result.reserve(inputValues.size() + insertValues.size());
        result.insert(result.end(), inputValues.begin(), inputValues.begin() + insertPosition);
        result.insert(result.end(), insertValues.begin(), insertValues.end());
        result.insert(result.end(), inputValues.begin() + insertPosition, inputValues.end());

        output.set(result);
    }

    ofParameter<std::vector<float>> input;
    ofParameter<std::vector<float>> insert;
    ofParameter<int> idx;
    ofParameter<std::vector<float>> output;
    ofEventListeners listeners;
};
