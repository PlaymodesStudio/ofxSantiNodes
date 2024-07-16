#ifndef probSeq_h
#define probSeq_h

#include "ofxOceanodeNodeModel.h"
#include <random>

class probSeq : public ofxOceanodeNodeModel {
public:
    probSeq() : ofxOceanodeNodeModel("Probabilistic Step Sequencer"), gen(rd()), dist(0, 1) {
        // Initialize the seed to zero, which means no seed by default
        seed = 0;
    }
    ~probSeq() {}

    void setup() override {
        // Parameter setup
        addParameter(index.set("Index", 0, 0, 1));
        addParameter(stepsVec.set("Steps[]", {0}, {0}, {1}));
        addParameter(seed.set("Seed", 0, 0, INT_MAX));
        addOutputParameter(output.set("Output", 0, 0, 1));

        // Register update listener
        listener = index.newListener([this](int &){ updateOutput(); });
        
        // Register seed listener
        seedListener = seed.newListener([this](int &) { resetGenerator(); });

        // Set index maximum based on stepsVec size
        stepsVecListener = stepsVec.newListener([this](vector<float> &steps) {
            index.setMax(steps.size() - 1);
        });

        // Initialize index
        index = 0;
    }

private:
    ofParameter<int> index;
    ofParameter<vector<float>> stepsVec;
    ofParameter<int> seed;
    ofParameter<int> output;
    ofEventListener listener;
    ofEventListener stepsVecListener;
    ofEventListener seedListener;
    int lastIndex = -1; // Track the last index processed
    bool gateOpen = false; // Track if the last output was a gate

    std::random_device rd; // Obtain a random number from hardware
    std::mt19937 gen; // Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> dist; // Uniform distribution

    void updateOutput() {
        int currentIndex = index.get();
        int stepsSize = stepsVec->size();

        if (stepsSize > 0) {
            int modIndex = currentIndex % stepsSize; // Ensure index wraps around

            if (currentIndex != lastIndex) { // Check if the index has changed
                lastIndex = currentIndex; // Update lastIndex for the next frame
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
    }

    void generateGate(float probability) {
        bool gate = dist(gen) < probability; // Determine if the gate should be triggered
        output.set(gate ? 1 : 0); // Update the output based on the gate value
        gateOpen = gate; // Update gateOpen to track if we need to close the gate in the next frame
    }

    void resetGenerator() {
        if (seed != 0) {
            gen.seed(seed); // Set the generator with the specified seed
        } else {
            gen.seed(rd()); // Use a random seed from hardware if seed is 0
        }
    }
};

#endif /* probSeq_h */
