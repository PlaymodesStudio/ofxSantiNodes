#pragma once

#include "ofxOceanodeNodeModel.h"

class splitRoute : public ofxOceanodeNodeModel {
public:
    splitRoute() : ofxOceanodeNodeModel("Split Route") {
        description = "Routes values from the input to one of the two outputs based on comparison with a threshold.";

        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(threshold.set("Threshold", 0.0f, -FLT_MAX, FLT_MAX));
        addOutputParameter(greaterEqual.set(">=", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(lessThan.set("<", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listener = input.newListener([this](std::vector<float> &v) {
            std::vector<float> greaterEqualValues;
            std::vector<float> lessThanValues;

            for (float value : v) {
                if (value >= threshold) {
                    greaterEqualValues.push_back(value);
                } else {
                    lessThanValues.push_back(value);
                }
            }
            
            greaterEqual = greaterEqualValues;
            lessThan = lessThanValues;
        });

        thresholdListener = threshold.newListener([this](float &t) {
            // Re-calculate outputs whenever the threshold changes
            std::vector<float> v = input;
            std::vector<float> greaterEqualValues;
            std::vector<float> lessThanValues;

            for (float value : v) {
                if (value >= threshold) {
                    greaterEqualValues.push_back(value);
                } else {
                    lessThanValues.push_back(value);
                }
            }
            
            greaterEqual = greaterEqualValues;
            lessThan = lessThanValues;
        });
    }

private:
    ofParameter<std::vector<float>> input;
    ofParameter<float> threshold;
    ofParameter<std::vector<float>> greaterEqual;
    ofParameter<std::vector<float>> lessThan;

    ofEventListener listener;
    ofEventListener thresholdListener;
};
