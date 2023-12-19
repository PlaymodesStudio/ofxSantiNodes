#pragma once

#include "ofxOceanodeNodeModel.h"
#include <random>

class scramble : public ofxOceanodeNodeModel {
public:
    scramble() : ofxOceanodeNodeModel("Shuffle") {
        description = "The Shuffle node rearranges elements within an input vector based on a control signal.";

        addParameter(input.set("Input", {0.0f}, {0.0f}, {FLT_MAX}));
        addParameter(shuffleControl.set("Shuffle", {0}, {0}, {1}));
        addParameter(allTrigger.set("All"));
        addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {FLT_MAX}));

        shuffleListener = shuffleControl.newListener([this](vector<int> &shuffleVals){
            shuffleInput();
        });

        allTriggerListener = allTrigger.newListener([this](){
            shuffleAll();
        });
    }

private:
    void shuffleInput() {
        vector<float> shuffledOutput = input.get();
        for(size_t i = 0; i < shuffleControl.get().size(); i++) {
            // If a shuffle value is detected
            if (shuffleControl.get()[i] == 1) {
                // Pick a random index different from the current index
                size_t randomIndex;
                do {
                    randomIndex = ofRandom(input.get().size());
                } while (randomIndex == i && input.get().size() > 1); // Ensure we get a different index when possible

                // Swap the values
                std::swap(shuffledOutput[i], shuffledOutput[randomIndex]);
            }
        }
        output = shuffledOutput;
    }

    void shuffleAll() {
        vector<float> shuffledOutput = input.get();
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(shuffledOutput.begin(), shuffledOutput.end(), g);  // Shuffle all the elements
        output = shuffledOutput;
    }


    ofParameter<vector<float>> input;
    ofParameter<vector<int>> shuffleControl;
    ofParameter<void> allTrigger;  // Trigger button parameter
    ofParameter<vector<float>> output;
    ofEventListener shuffleListener;
    ofEventListener allTriggerListener;
};
