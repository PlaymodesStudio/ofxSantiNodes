#pragma once

#include "ofxOceanodeNodeModel.h"

class splitMinMax : public ofxOceanodeNodeModel {
public:
    splitMinMax() : ofxOceanodeNodeModel("Split MinMax") {
        description = "Lets values pass if they are between the specified min and max values.";

        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(minVal.set("Min", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(maxVal.set("Max", {1.0f}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listener = input.newListener([this](std::vector<float> &v) {
            updateOutput(v);
        });

        minListener = minVal.newListener([this](std::vector<float> &minV) {
            // Re-calculate outputs whenever the min values change
            updateOutput(input);
        });

        maxListener = maxVal.newListener([this](std::vector<float> &maxV) {
            // Re-calculate outputs whenever the max values change
            updateOutput(input);
        });
    }

private:
    ofParameter<std::vector<float>> input;
    ofParameter<std::vector<float>> minVal;
    ofParameter<std::vector<float>> maxVal;
    ofParameter<std::vector<float>> output;

    ofEventListener listener;
    ofEventListener minListener;
    ofEventListener maxListener;

    void updateOutput(const std::vector<float>& v) {
        std::vector<float> outputValues;
        std::vector<float> minValues = minVal.get();
        std::vector<float> maxValues = maxVal.get();

        for (size_t i = 0; i < v.size(); i++) {
            if (i < minValues.size() && i < maxValues.size()) {
                if (v[i] >= minValues[i] && v[i] <= maxValues[i]) {
                    outputValues.push_back(v[i]);
                }
            }
        }
        
        output = outputValues;
    }

};
