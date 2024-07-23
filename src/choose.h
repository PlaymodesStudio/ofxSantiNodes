#ifndef choose_h
#define choose_h

#include "ofxOceanodeNodeModel.h"
#include <random>
#include <algorithm>
#include <numeric>

class choose : public ofxOceanodeNodeModel {
public:
    choose() : ofxOceanodeNodeModel("Choose"), gen(std::random_device{}()) {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(weights.set("Weights", {1.0f}, {0.0f}, {1.0f}));
        addParameter(trigger.set("Trigger", {0}, {0}, {1}));
        addParameter(urn.set("Urn", false));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(trigger.newListener([this](vector<int> &t){
            chooseValues(t);
        }));

        listeners.push(input.newListener([this](vector<float> &i){
            resetUrn();
        }));

        listeners.push(urn.newListener([this](bool &u){
            resetUrn();
        }));

        // Initialize lastTrigger with the current trigger value
        lastTrigger = trigger.get();
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> weights;
    ofParameter<vector<int>> trigger;
    ofParameter<bool> urn;
    ofParameter<vector<float>> output;

    vector<int> lastTrigger;
    vector<int> urnSequence;
    size_t urnIndex;
    vector<int> lastChosenIndices;

    std::mt19937 gen;
    std::uniform_real_distribution<> dist{0.0f, 1.0f};

    ofEventListeners listeners;

    void chooseValues(const vector<int>& newTrigger) {
        if (input->empty()) return;

        vector<float> currentOutput = output.get();
        
        // Ensure all vectors have the correct size
        size_t size = newTrigger.size();
        currentOutput.resize(size, 0.0f);
        lastTrigger.resize(size, 0);
        lastChosenIndices.resize(size, -1);

        for (size_t i = 0; i < size; ++i) {
            if (lastTrigger[i] == 0 && newTrigger[i] == 1) {
                currentOutput[i] = chooseValue(i);
            }
        }

        output = currentOutput;
        lastTrigger = newTrigger;
    }

    float chooseValue(size_t index) {
        if (urn) {
            if (urnIndex >= urnSequence.size()) {
                resetUrn();
            }
            
            int chosenIndex = urnSequence[urnIndex++];
            
            // If it's the same as the last chosen index and there are other options, choose the next one
            if (chosenIndex == lastChosenIndices[index] && input->size() > 1) {
                urnIndex %= urnSequence.size(); // Wrap around if necessary
                chosenIndex = urnSequence[urnIndex++];
            }
            
            lastChosenIndices[index] = chosenIndex;
            return input->at(chosenIndex);
        } else {
            std::vector<float> normalizedWeights;
            
            if (weights->empty() || weights->size() == 1) {
                normalizedWeights.assign(input->size(), 1.0f / input->size());
            } else {
                float sum = std::accumulate(weights->begin(), weights->end(), 0.0f);
                
                if (sum == 0) {
                    normalizedWeights.assign(input->size(), 1.0f / input->size());
                } else {
                    normalizedWeights.resize(std::min(weights->size(), input->size()));
                    for (size_t i = 0; i < normalizedWeights.size(); ++i) {
                        normalizedWeights[i] = weights->at(i) / sum;
                    }
                }
            }

            float r = dist(gen);
            float cumulativeProbability = 0.0f;
            
            for (size_t i = 0; i < normalizedWeights.size(); ++i) {
                cumulativeProbability += normalizedWeights[i];
                if (r <= cumulativeProbability) {
                    return input->at(i);
                }
            }
            
            return input->back(); // Fallback in case of rounding errors
        }
    }

    void resetUrn() {
        urnSequence.clear();
        for (size_t i = 0; i < input->size(); ++i) {
            urnSequence.push_back(i);
        }
        std::shuffle(urnSequence.begin(), urnSequence.end(), gen);
        urnIndex = 0;
    }
};

#endif /* choose_h */
