#include "ofxOceanodeNodeModel.h"

class soloSequencer : public ofxOceanodeNodeModel {
public:
    soloSequencer() : ofxOceanodeNodeModel("Solo Sequencer") {
        description = "A sequencer node that outputs a number based on weighted probabilities from eight input vectors. "
                      "The 'step' input determines the index for reading values from each input vector. "
                      "'Hold Mode' allows the output to update only when there are changes in the input vectors' values.";

        // Parameters
        addParameter(step.set("Step", 0, 0, INT_MAX));
        addParameter(shift.set("Shift", 0, 0, INT_MAX));
        addParameter(in1.set("In 1", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in2.set("In 2", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in3.set("In 3", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in4.set("In 4", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in5.set("In 5", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in6.set("In 6", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in7.set("In 7", {0.0f}, {0.0f}, {1.0f}));
        addParameter(in8.set("In 8", {0.0f}, {0.0f}, {1.0f}));
        addParameter(holdMode.set("Hold Mode", false));
        addOutputParameter(solo.set("Solo", 0, 0, 8));
        
        lastValues = std::vector<float>(8, 0.0f); // Initialize with zeros for 8 inputs
        
        // Listener for step parameter changes
        listeners.push(step.newListener([this](int &s) {
            updateSolo();
        }));
        
        listeners.push(shift.newListener([this](int &s) {
            updateSolo();
        }));
    }

    void updateSolo() {
        std::vector<float> currentValues(8, 0.0f);
        std::array<std::vector<float>, 8> inputs = {in1.get(), in2.get(), in3.get(), in4.get(), in5.get(), in6.get(), in7.get(), in8.get()};
        bool hasChanged = false;
        std::vector<bool> changedVectors(8, false); // Track which vectors have changed

        // Determine current values and whether they've changed
        for(int i = 0; i < 8; ++i) {
            if(!inputs[i].empty()) {
                int index = (step + shift) % inputs[i].size();
                currentValues[i] = inputs[i][index];
                if(currentValues[i] != lastValues[i]) {
                    hasChanged = true;
                    changedVectors[i] = true; // Mark this vector as changed
                }
            }
        }

        float sum = std::accumulate(currentValues.begin(), currentValues.end(), 0.0f);
        if(sum == 0) {
            solo = 0; // All inputs are zero
            return;
        }

        std::vector<float> probabilities;
        for(auto value : currentValues) probabilities.push_back(value / sum);

        if(hasChanged || !holdMode) {
            performSelection(probabilities, changedVectors);
        }
        // If no changes and hold mode is active, 'solo' remains unchanged

        lastValues = currentValues; // Update lastValues for the next comparison
    }

    void performSelection(const std::vector<float>& probabilities, const std::vector<bool>& changedVectors) {
        float rand = ofRandom(1.0f);
        float cumulative = 0.0f;
        for(int i = 0; i < probabilities.size(); ++i) {
            cumulative += probabilities[i];
            if(rand <= cumulative) {
                solo = i + 1;
                break;
            }
        }
    }

private:
    ofParameter<int> step;
    ofParameter<int> shift;
    ofParameter<std::vector<float>> in1, in2, in3, in4, in5, in6, in7, in8;
    ofParameter<int> solo;
    ofParameter<bool> holdMode;
    std::vector<float> lastValues; // Cache of last step values for change detection
    ofEventListeners listeners;
};
