#ifndef binPermute_h
#define binPermute_h

#include "ofxOceanodeNodeModel.h"

class binPermute : public ofxOceanodeNodeModel {
public:
    binPermute() : ofxOceanodeNodeModel("Bin Permute") {}

    void setup() override {
        description = "Bin Permute calculates all possible binary combinations of a vector based on the 'size' parameter. The 'index' parameter selects which permutation to output. The 'num' parameter shows the total number of permutations.";
        addParameter(size.set("Size", 3, 1, 32));
        addParameter(index.set("Index", 0, 0, 0)); // Temporarily set max to 0; will update after setup
        addParameter(num.set("Num", 0, 0, std::numeric_limits<int>::max()));
        
        addOutputParameter(output.set("Output", {}, {0}, {1}));

        sizeListener = size.newListener([this](int & newSize) {
            num = pow(2, newSize); // Calculates the total number of permutations
            index.setMax(num - 1);
            generateOutput();
        });

        indexListener = index.newListener([this](int & newIndex) {
            generateOutput();
        });
    }

private:
    ofParameter<int> size;
    ofParameter<int> index;
    ofParameter<int> num;
    ofParameter<vector<float>> output;

    ofEventListener sizeListener;
    ofEventListener indexListener;

    void generateOutput() {
        vector<float> permutation(size, 0);
        int tmpIndex = index;
        for (int i = 0; i < size; ++i) {
            permutation[i] = (tmpIndex >> i) & 1; // Generate permutation based on index
        }
        std::reverse(permutation.begin(), permutation.end()); // Reverse to match the expected order
        output = permutation;
    }
};

#endif /* binPermute_h */
