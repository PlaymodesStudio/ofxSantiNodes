#ifndef probSeq_h
#define probSeq_h

#include "ofxOceanodeNodeModel.h"
#include <random>

class probSeq : public ofxOceanodeNodeModel {
public:
    probSeq() : ofxOceanodeNodeModel("Probabilistic Step Sequencer") {}
    ~probSeq() {}

    void setup() override {
        // Parameter setup
        addParameter(index.set("Index", 0, 0, 1));
        addParameter(stepsVec.set("Steps[]", {0}, {0}, {1}));
        addOutputParameter(output.set("Output", 0, 0, 1));

        // Register update listener
        listener = index.newListener([this](int &){ updateOutput(); });
    }

private:
    ofParameter<int> index;
    ofParameter<vector<float>> stepsVec;
    ofParameter<int> output;
    ofEventListener listener;
    int lastIndex = -1; // Track the last index processed
    bool gateOpen = false; // Track if the last output was a gate

    void updateOutput() {
        int currentIndex = index.get();
        if (currentIndex != lastIndex) { // Check if the index has changed
            lastIndex = currentIndex; // Update lastIndex for the next frame
            int modIndex = currentIndex % stepsVec.get().size(); // Ensure index wraps around
            float probability = stepsVec.get()[modIndex]; // Get probability from current step

            if (!gateOpen || probability == 1) { // Ensure we generate a gate for each step if probability is 1
                generateGate(probability);
            }
        } else if (gateOpen) {
            // Ensure a gate is followed by a 0 in the next frame
            output.set(0);
            gateOpen = false;
        }
        // If index hasn't changed and gate wasn't open, do nothing
    }

    void generateGate(float probability) {
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 gen(rd()); // Seed the generator
        std::uniform_real_distribution<> distr(0, 1); // Define the range

        bool gate = distr(gen) < probability; // Determine if the gate should be triggered
        output.set(gate ? 1 : 0); // Update the output based on the gate value
        gateOpen = gate; // Update gateOpen to track if we need to close the gate in the next frame
    }
};

#endif /* probSeq_h */
