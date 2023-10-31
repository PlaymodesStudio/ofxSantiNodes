#pragma once

#include "ofxOceanodeNodeModel.h"

class countNumber : public ofxOceanodeNodeModel {
public:
    countNumber() : ofxOceanodeNodeModel("Count Number"), resetOnNextMatch(false) {
        description = "Counts the occurrences of each number in the 'NumToCount' vector as they appear in successive data events in the 'Input' vector.";
        
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(numToCount.set("NumToCount", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(output.set("Output", {0}, {0}, {INT_MAX}));
        addParameter(resetButton.set("Reset"));
        addParameter(resetNext.set("Reset Next")); // Adding the 'resetNext' void parameter

        // Initialize counts
        counts.clear();

        listeners.push(input.newListener([this](vector<float> &v) {
            for(const auto& num : v) {
                if(resetOnNextMatch && counts.find(num) != counts.end()) {
                    counts.clear();
                    for (const auto& ntc : numToCount.get()) {
                        counts[ntc] = 0 ;
                    }
                    resetOnNextMatch = false;
                    break;
                }
            }
            updateCounts(v);
        }));

        listeners.push(numToCount.newListener([this](vector<float> &n) {
            initializeCounts(n);
        }));

        listeners.push(resetButton.newListener([this]() {
            counts.clear();
            for (const auto& num : numToCount.get()) {
                counts[num] = 0;
            }
            vector<int> out(numToCount->size(), 0);
            output = out;
        }));

        listeners.push(resetNext.newListener([this]() {
            resetOnNextMatch = true;
        }));
    }

    void updateCounts(const vector<float> &newInput) {
        for(size_t i = 0; i < newInput.size(); i++) {
            float num = newInput[i];
            if (counts.find(num) != counts.end()) {
                counts[num]++;
            }
        }

        vector<int> out;
        for (const auto& num : numToCount.get()) {
            out.push_back(counts[num]);
        }
        output = out;
    }

    void initializeCounts(const vector<float> &newNumToCount) {
        for (const auto& num : newNumToCount) {
            if (counts.find(num) == counts.end()) {
                counts[num] = 0;
            }
        }
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> numToCount;
    ofParameter<vector<int>> output;
    ofParameter<void> resetButton;
    ofParameter<void> resetNext; // Declaration for 'resetNext' parameter

    std::unordered_map<float, int> counts;
    bool resetOnNextMatch; // Flag to check if reset is needed on next match
    ofEventListeners listeners;
};
