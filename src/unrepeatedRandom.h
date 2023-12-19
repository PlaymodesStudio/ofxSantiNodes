#ifndef UnrepeatedRandom_h
#define UnrepeatedRandom_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <random>
#include <algorithm>

class UnrepeatedRandom : public ofxOceanodeNodeModel {
public:
    UnrepeatedRandom() : ofxOceanodeNodeModel("Unrepeated Random") {
        description = "This module generates random numbers in a given range. "
                      "It has a sequential mode where it ensures all numbers within the range "
                      "are generated before repeating. It supports multiple triggers and provides "
                      "synchronized random number arrays.";
    }

    void setup() override {
        addParameter(trigger.set("Trigger", {0}, {0}, {1}));
        addParameter(evenTrig.set("EvenTrig", {0}, {0}, {1}));
        addParameter(min.set("Min", 0, 0, 100));
        addParameter(max.set("Max", 10, 0, 100));
        addParameter(sequentialMode.set("Sequential Mode", false));
        addOutputParameter(output.set("Output", {0}, {0}, {100}));

        triggerListener = trigger.newListener([this](vector<float>& trigger){
            generateRandomWrapper(trigger, previousTriggerTrigger);
        });

        evenTrigListener = evenTrig.newListener([this](vector<float>& evenTrigger){
            generateRandomWrapper(evenTrigger, previousTriggerEvenTrig);
        });
    }

    void generateRandomWrapper(vector<float>& triggerSource, vector<float>& previousTriggerSource) {
        std::lock_guard<std::mutex> lock(mutex);
        int newSize = triggerSource.size();
        outputVec.resize(newSize);
        sequences.resize(newSize);
        previousTriggerSource.resize(newSize, 0);
        
        for(int i = 0; i < newSize; i++) {
            if(triggerSource[i] != previousTriggerSource[i]) {
                generateRandom(i);
            }
        }
        previousTriggerSource = triggerSource;
        output = outputVec;
    }

    void generateRandom(int index) {
            if(!sequentialMode) {
                int randNum = min + (rand() % static_cast<int>(max - min + 1));
                while(std::count(outputVec.begin(), outputVec.end(), randNum)) {
                    randNum = min + (rand() % static_cast<int>(max - min + 1));
                }
                outputVec[index] = randNum;
            }
            else {
                if(sequences[index].size() == 0) {
                    for(int i = min; i <= max; i++) {
                        sequences[index].push_back(i);
                    }
                    std::shuffle(sequences[index].begin(), sequences[index].end(), std::mt19937(std::random_device()()));
                }
                outputVec[index] = sequences[index].back();
                sequences[index].pop_back();
            }
        }

private:
    vector<float> previousTriggerTrigger;  // renamed
    vector<float> previousTriggerEvenTrig;  // new
    vector<int> outputVec;
    vector<vector<int>> sequences;
    ofParameter<vector<float>> trigger;
    ofParameter<vector<float>> evenTrig;
    ofParameter<int> min;
    ofParameter<int> max;
    ofParameter<bool> sequentialMode;
    ofParameter<vector<int>> output;
    ofEventListener triggerListener;
    ofEventListener evenTrigListener;

    std::mutex mutex;
};

#endif /* UnrepeatedRandom_h */
