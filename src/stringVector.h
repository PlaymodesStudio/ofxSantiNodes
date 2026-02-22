#ifndef stringVector_h
#define stringVector_h

#include "ofxOceanodeNodeModel.h"

class stringVector : public ofxOceanodeNodeModel {
public:
    stringVector() : ofxOceanodeNodeModel("String Vector") {}

    void setup() override {
        description = "Splits a comma-separated string into a vector of strings.";
        addParameter(input.set("Input", ""));
        addOutputParameter(output.set("Output", vector<string>{}));

        inputListener = input.newListener([this](string &s) {
            compute(s);
        });
    }

private:
    void compute(const string &s) {
        vector<string> result;
        stringstream ss(s);
        string token;
        while(getline(ss, token, ',')) {
            // trim leading and trailing spaces
            size_t start = token.find_first_not_of(' ');
            size_t end   = token.find_last_not_of(' ');
            if(start != string::npos)
                result.push_back(token.substr(start, end - start + 1));
            else
                result.push_back("");
        }
        output = result;
    }

    ofParameter<string>          input;
    ofParameter<vector<string>>  output;
    ofEventListener              inputListener;
};

#endif /* stringVector_h */
