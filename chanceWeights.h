// ChanceWeights.h

#ifndef chanceWeights_h
#define chanceWeights_h

#include "ofxOceanodeNodeModel.h"
#include <random>

class chanceWeights : public ofxOceanodeNodeModel {
public:
    chanceWeights() : ofxOceanodeNodeModel("Chance Weights"), gen(seed.get()), dis(0.0, 1.0) {
        seed.addListener(this, &chanceWeights::seedChanged);
    }

    void setup() override {
        description = "Uses a vector of weights to probabilistically select which values from the input vector will be passed to the output. Each weight should be a float between 0 and 1 and represents the probability that the corresponding value in the input vector will be output.";

        addParameter(seed.set("Seed", 0, 0, INT_MAX));
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(weights.set("Weights", {0}, {0}, {1}));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(input.newListener([this](vector<float>& vf) {
            calculate();
        }));

        listeners.push(weights.newListener([this](vector<float>& vf) {
            calculate();
        }));
    }

    void calculate() {
        vector<float> out;
        if(input.get().size() != weights.get().size()) return;
        for(size_t i = 0; i < input.get().size(); i++){
            float chance = dis(gen);  // Generate a random float between 0.0 and 1.0
            out.push_back((chance < weights.get()[i]) ? input.get()[i] : 0);
        }
        output = out;
    }

    void seedChanged(int & newSeed) {
        gen.seed(newSeed);
    }

private:
    ofParameter<int> seed;
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> weights;
    ofParameter<vector<float>> output;

    ofEventListeners listeners;

    std::mt19937 gen; //Standard mersenne_twister_engine
    std::uniform_real_distribution<> dis; //Uniform distribution between 0.0 and 1.0
};

#endif /* chanceWeights_h */
