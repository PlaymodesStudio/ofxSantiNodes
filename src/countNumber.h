#pragma once

#include "ofxOceanodeNodeModel.h"

class countNumber : public ofxOceanodeNodeModel {
public:
    countNumber() : ofxOceanodeNodeModel("Count Number"), resetOnNextMatch(false) {
        description = "Counts the occurrences of each number in the 'NumToCount' vector at each position of the input vector.";
        
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(numToCount.set("NumToCount", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(output.set("Output", {0}, {0}, {INT_MAX}));
        addParameter(resetButton.set("Reset"));
        addParameter(resetNext.set("Reset Next"));

        // Initialize counts vector of maps
        positionCounts.clear();

        listeners.push(input.newListener([this](vector<float> &v) {
            if(resetOnNextMatch) {
                for(const auto& num : v) {
                    for(const auto& counts : positionCounts) {
                        if(counts.find(num) != counts.end()) {
                            resetCounts();
                            resetOnNextMatch = false;
                            break;
                        }
                    }
                }
            }
            updateCounts(v);
        }));

        listeners.push(numToCount.newListener([this](vector<float> &n) {
            initializeCounts();
        }));

        listeners.push(resetButton.newListener([this]() {
            resetCounts();
        }));

        listeners.push(resetNext.newListener([this]() {
            resetOnNextMatch = true;
        }));
    }

    void updateCounts(const vector<float> &newInput) {
        // Ensure we have enough position counters
        while(positionCounts.size() < newInput.size()) {
            positionCounts.push_back(std::unordered_map<float, int>());
            initializeCountsAtPosition(positionCounts.size() - 1);
        }

        // Update counts for each position
        for(size_t pos = 0; pos < newInput.size(); pos++) {
            float num = newInput[pos];
            if(positionCounts[pos].find(num) != positionCounts[pos].end()) {
                positionCounts[pos][num]++;
            }
        }

        // Create output vector for each number to count
        vector<int> out;
        for(const auto& num : numToCount.get()) {
            // For each position, check if this number was found
            for(size_t pos = 0; pos < positionCounts.size(); pos++) {
                out.push_back(positionCounts[pos][num]);
            }
        }
        
        output = out;
    }

    void initializeCounts() {
        positionCounts.clear();
        // We'll initialize position counts when we see the first input
    }

    void initializeCountsAtPosition(size_t position) {
        for(const auto& num : numToCount.get()) {
            positionCounts[position][num] = 0;
        }
    }

    void resetCounts() {
        for(auto& counts : positionCounts) {
            for(const auto& num : numToCount.get()) {
                counts[num] = 0;
            }
        }
        
        // Create output vector with zeros
        vector<int> out(numToCount->size() * positionCounts.size(), 0);
        output = out;
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> numToCount;
    ofParameter<vector<int>> output;
    ofParameter<void> resetButton;
    ofParameter<void> resetNext;

    vector<std::unordered_map<float, int>> positionCounts; // Counts for each position
    bool resetOnNextMatch;
    ofEventListeners listeners;
};
