#pragma once

#include "ofxOceanodeNodeModel.h"

class randomSeries : public ofxOceanodeNodeModel {
public:
    randomSeries() : ofxOceanodeNodeModel("Random Series") {}

    void setup() override {
        description = "This module generates consistent random number series based on provided seeds. Each series' length and the index of its output are defined by the 'length' and 'index' inputs, respectively. The 'Q' input quantizes the series values, with 'Int' toggling integer output and 'Urn' ensuring non-repeated values";
        
        addParameter(index.set("Index", {0}, {0}, {100}));
        addParameter(seed.set("Seed", {0}, {0}, {100}));
        addParameter(length.set("Length", {1}, {1}, {100}));
        addParameter(Q.set("Q", {0}, {0}, {100}));
        addParameter(Int.set("Int", false));
        addParameter(Urn.set("Urn", false));
        addOutputParameter(output.set("Output", {0.0}, {0.0}, {FLT_MAX}));

        listener = index.newListener([this](vector<int> &v){
            vector<float> out;
            int maxSize = max({index->size(), seed->size(), length->size(), Q->size()});
            for (int i = 0; i < maxSize; i++) {
                int cur_index = getValueFromParam(index, i);
                int cur_seed = getValueFromParam(seed, i);
                int cur_length = getValueFromParam(length, i);
                int cur_Q = getValueFromParam(Q, i);
                bool cur_Int = Int;
                bool cur_Urn = Urn;

                std::vector<float> series = generateSeries(cur_seed, cur_length, cur_Q, cur_Int, cur_Urn);

                int idx = cur_index % cur_length;

                if(!series.empty() && idx >= 0 && idx < series.size()) {
                    out.push_back(series[idx]);
                } else {
                    out.push_back(0.0f);
                }
            }
            output = out;
        });
    }

private:
    template<typename T>
    T getValueFromParam(const ofParameter<vector<T>> &param, int i) {
        if (i < param->size()) {
            return param->at(i);
        } else {
            return param->back();
        }
    }

    std::vector<float> generateSeries(int seed, int length, int Q, bool Int, bool Urn) {
        ofSeedRandom(seed);
        std::vector<float> series;
        std::set<float> uniqueNumbers;

        while(series.size() < length) {
            float value;
            if (Int) {
                value = static_cast<int>(ofRandom(Q + 1)); // Ensuring that the value is an integer.

                // For Urn functionality, check if the number already exists in the set.
                if (Urn && uniqueNumbers.find(value) != uniqueNumbers.end()) {
                    continue;
                }

            } else {
                if(Q == 0){ // No quantization
                    value = ofRandom(1.0);
                } else {
                    value = ofRandom(0, 1.0 * (Q - 1) / Q);
                    value = round(value * Q) / Q;
                }

                if (Urn && uniqueNumbers.find(value) != uniqueNumbers.end()) {
                    continue;
                }
            }
            series.push_back(value);
            if (Urn) uniqueNumbers.insert(value);
        }
        return series;
    }



    ofParameter<vector<int>> index, seed, length, Q;
    ofParameter<bool> Int, Urn;
    ofParameter<vector<float>> output;
    ofEventListener listener;
};
