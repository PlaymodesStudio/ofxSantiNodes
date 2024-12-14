#ifndef CSV_STRINGS_H
#define CSV_STRINGS_H

#include "ofxOceanodeNodeModel.h"

class csvStrings : public ofxOceanodeNodeModel {
public:
    csvStrings() : ofxOceanodeNodeModel("CSV Strings") {}
    
    void setup() {
        input.set("Input", "Fb7, CM9, csus6");
        index.set("Index", 0, 0, 0);
        size.set("Size", 0);
        output.set("Output", "");
        
        addParameter(input);
        addParameter(index);
        addParameter(size);
        addParameter(output);
        
        listeners.push(input.newListener([this](string &s){
            processInput();
        }));
        
        listeners.push(index.newListener([this](int &i){
            updateOutput();
        }));
        
        processInput(); // Initial processing
    }

private:
    ofParameter<string> input;
    ofParameter<int> index;
    ofParameter<int> size;
    ofParameter<string> output;
    ofEventListeners listeners;
    vector<string> strings;
    
    void processInput() {
        strings.clear();
        string currentInput = input.get();
        
        // Split the input string by commas
        vector<string> tokens = ofSplitString(currentInput, ",");
        
        // Clean and store each token
        for(const auto& token : tokens) {
            string cleanToken = ofTrim(token);
            if(!cleanToken.empty()) {
                strings.push_back(cleanToken);
            }
        }
        
        // Update size parameter
        size.set(strings.size());
        
        // Update index max value based on new size
        index.setMax(std::max(0, (int)strings.size() - 1));
        
        // Update output
        updateOutput();
    }
    
    void updateOutput() {
        int currentIndex = index.get();
        if(currentIndex >= 0 && currentIndex < strings.size()) {
            output.set(strings[currentIndex]);
        } else {
            output.set("");
        }
    }
};

#endif /* CSV_STRINGS_H */
