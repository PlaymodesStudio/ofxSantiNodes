#ifndef choose_h
#define choose_h

#include "ofxOceanodeNodeModel.h"
#include <random>
#include <algorithm>
#include <numeric>
#include <set>

class choose : public ofxOceanodeNodeModel {
public:
    choose() : ofxOceanodeNodeModel("Choose"), gen(std::random_device{}()) {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(weights.set("Weights", {1.0f}, {0.0f}, {1.0f}));
        addParameter(trigger.set("Trigger", {0}, {0}, {1}));
        addParameter(urn.set("URN Seq", false));
        addParameter(unique.set("Unique", false));
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

        lastTrigger = trigger.get();
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> weights;
    ofParameter<vector<int>> trigger;
    ofParameter<bool> urn;
    ofParameter<bool> unique;
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
        
        size_t size = newTrigger.size();
        currentOutput.resize(size, 0.0f);
        lastTrigger.resize(size, 0);
        lastChosenIndices.resize(size, -1);

        if (unique) {
            // For unique selection, handle all triggered positions together
            std::vector<size_t> triggeredPositions;
            for (size_t i = 0; i < size; ++i) {
                if (lastTrigger[i] == 0 && newTrigger[i] == 1) {
                    triggeredPositions.push_back(i);
                }
            }

            if (!triggeredPositions.empty()) {
                auto uniqueValues = chooseUniqueValues(triggeredPositions.size());
                for (size_t i = 0; i < triggeredPositions.size(); ++i) {
                    currentOutput[triggeredPositions[i]] = uniqueValues[i];
                }
            }
        } else {
            // Original non-unique behavior
            for (size_t i = 0; i < size; ++i) {
                if (lastTrigger[i] == 0 && newTrigger[i] == 1) {
                    currentOutput[i] = chooseValue(i);
                }
            }
        }

        output = currentOutput;
        lastTrigger = newTrigger;
    }

    // New method to choose multiple unique values
    vector<float> chooseUniqueValues(size_t count) {
        vector<float> result;
        count = std::min(count, input->size());  // Can't select more unique values than we have

        if (urn) {
            // In urn mode, just take the next 'count' values from the urn
            if (urnIndex + count > urnSequence.size()) {
                resetUrn();
            }
            for (size_t i = 0; i < count; ++i) {
                result.push_back(input->at(urnSequence[urnIndex++]));
            }
        } else {
            // For weighted selection, we'll need to modify the weights as we go
            vector<float> availableIndices(input->size());
            std::iota(availableIndices.begin(), availableIndices.end(), 0);
            vector<float> currentWeights = weights->size() == 1 ?
                vector<float>(input->size(), weights->front()) :
                vector<float>(weights->begin(), weights->begin() + std::min(weights->size(), input->size()));

            for (size_t i = 0; i < count; ++i) {
                float weightSum = std::accumulate(currentWeights.begin(), currentWeights.end(), 0.0f);
                if (weightSum == 0) {
                    // If all weights are zero, use uniform distribution for remaining selections
                    size_t remainingCount = availableIndices.size() - i;
                    std::fill(currentWeights.begin(), currentWeights.end(), 1.0f / remainingCount);
                    weightSum = 1.0f;
                }

                float r = dist(gen) * weightSum;
                float cumulativeWeight = 0.0f;
                size_t selectedIndex = availableIndices.back();  // Default to last element

                for (size_t j = 0; j < availableIndices.size(); ++j) {
                    cumulativeWeight += currentWeights[j];
                    if (r <= cumulativeWeight) {
                        selectedIndex = availableIndices[j];
                        // Remove the selected index and its weight
                        availableIndices.erase(availableIndices.begin() + j);
                        currentWeights.erase(currentWeights.begin() + j);
                        break;
                    }
                }

                result.push_back(input->at(selectedIndex));
            }
        }

        return result;
    }

    float chooseValue(size_t index) {
        // Original chooseValue method remains unchanged
        if (urn) {
            if (urnIndex >= urnSequence.size()) {
                resetUrn();
            }
            
            int chosenIndex = urnSequence[urnIndex++];
            
            if (chosenIndex == lastChosenIndices[index] && input->size() > 1) {
                urnIndex %= urnSequence.size();
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
            
            return input->back();
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
