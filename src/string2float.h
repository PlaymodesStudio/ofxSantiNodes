#ifndef string2float_h
#define string2float_h

#include "ofxOceanodeNodeModel.h"

class string2float : public ofxOceanodeNodeModel {
public:
    string2float() : ofxOceanodeNodeModel("String to Float") {}
    
    void setup() override {
        description = "Converts a string of numbers into a vector of floats using a custom separator.";
        
        // Add input parameter for string
        addParameter(input.set("Input", ""));
        
        // Add parameter for separator
        addParameter(separator.set("Separator", ","));
        
        // Add output vector parameter
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
        
        // Add listener for input changes
        listener = input.newListener([this](string &s){
            processInput();
        });
        
        // Add listener for separator changes
        separatorListener = separator.newListener([this](string &s){
            processInput();
        });
    }
    
private:
    void processInput() {
        vector<float> result;
        string str = input.get();
        string sep = separator.get();
        
        // Return empty vector if input is empty
        if(str.empty()) {
            output = result;
            return;
        }
        
        // Handle case where separator is empty (treat each character as a number)
        if(sep.empty()) {
            for(char c : str) {
                if(isdigit(c) || c == '.' || c == '-') {
                    result.push_back(ofToFloat(string(1, c)));
                }
            }
            output = result;
            return;
        }
        
        // Split string by separator and convert to floats
        size_t pos = 0;
        string token;
        while ((pos = str.find(sep)) != string::npos) {
            token = str.substr(0, pos);
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if(!token.empty()) {
                try {
                    result.push_back(ofToFloat(token));
                } catch(...) {
                    // Skip invalid numbers
                }
            }
            str.erase(0, pos + sep.length());
        }
        
        // Process the last token
        str.erase(0, str.find_first_not_of(" \t\r\n"));
        str.erase(str.find_last_not_of(" \t\r\n") + 1);
        if(!str.empty()) {
            try {
                result.push_back(ofToFloat(str));
            } catch(...) {
                // Skip invalid numbers
            }
        }
        
        output = result;
    }
    
    ofParameter<string> input;
    ofParameter<string> separator;
    ofParameter<vector<float>> output;
    ofEventListener listener;
    ofEventListener separatorListener;
};

#endif /* string2float_h */
