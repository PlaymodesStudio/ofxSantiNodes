#ifndef permutations_h
#define permutations_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class permutations : public ofxOceanodeNodeModel {
public:
    permutations() : ofxOceanodeNodeModel("Permutations") {
    }

    void setup() override {
        description = "Generates permutations from an input vector based on a given set size. To avoid excessive computations, the input vector size is limited to 12. The sort toggle allows for successive pair swaps between adjacent indexs";
        
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(setSize.set("Set Size", 2, 1, 12));
        addParameter(index.set("Index", 0, 0, 127));
        
        addOutputParameter(num.set("Num", 0));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(enableSorting.set("Sort", false));


        inputListener = input.newListener([this](vector<float> &values) {
            calculatePermutations();
        });
        setSizeListener = setSize.newListener([this](int &value) {
            calculatePermutations();
        });
        indexListener = index.newListener([this](int &value) {
            calculatePermutations();
        });
    }
    
    const size_t maxInputSize = 12; // Adjust this value as needed


    void calculatePermutations() {
        const auto& in = input.get();
        
        if(in.size() > maxInputSize) {
            vector<float> temp = in;
            temp.resize(maxInputSize);
            input = temp;
        }
        
        allPermutations.clear();

        if (in.size() < setSize) {
            num = 0;
            output = std::vector<float>();
            return;
        }

        // Generate combinations of indices for the input size taken setSize at a time
        std::vector<int> indices(in.size());
        std::iota(indices.begin(), indices.end(), 0); // fill with 0, 1, 2, ...

        std::vector<int> currentCombination(indices.begin(), indices.begin() + setSize);

        if(enableSorting) {
            do {
                std::vector<float> subsetValues;
                for (int idx : currentCombination) {
                    subsetValues.push_back(in[idx]);
                }
                
                // Generate permutations in a way that only one element is different between successive permutations
                allPermutations.push_back(subsetValues); // Add the current subset
                
                std::sort(subsetValues.begin(), subsetValues.end());
                do {
                    allPermutations.push_back(subsetValues);
                } while (std::next_permutation(subsetValues.begin(), subsetValues.end()));

            } while (next_combination(indices, currentCombination));
        }
        else {
            do {
                std::vector<float> subsetValues;
                for (int idx : currentCombination) {
                    subsetValues.push_back(in[idx]);
                }
                
                do {
                    allPermutations.push_back(subsetValues);
                } while (std::next_permutation(subsetValues.begin(), subsetValues.end()));

            } while (next_combination(indices, currentCombination));
        }

        num = allPermutations.size();

        if (index >= 0 && index < allPermutations.size()) {
            output = allPermutations[index];
        }
    }



    
    bool next_combination(const std::vector<int>& v, std::vector<int>& combination) {
        int n = v.size();
        int r = combination.size();
        if (r == 0 || n == 0 || r > n) {
            return false;
        }
        for (int i = r - 1; i >= 0; i--) {
            if (combination[i] != v[i + n - r]) {
                int nextIndex = std::distance(v.begin(), std::find(v.begin(), v.end(), combination[i]));
                combination[i] = v[nextIndex + 1];
                for (int j = i + 1; j < r; j++) {
                    combination[j] = v[j - i + nextIndex + 1];
                }
                return true;
            }
        }
        return false;
    }




private:
    ofParameter<vector<float>> input;
    ofParameter<int> setSize;
    ofParameter<int> index;
    ofParameter<bool> enableSorting;
    
    ofParameter<int> num;
    ofParameter<vector<float>> output;

    std::vector<std::vector<float>> allPermutations;

    ofEventListener inputListener;
    ofEventListener setSizeListener;
    ofEventListener indexListener;
};

#endif /* permutations_h */
