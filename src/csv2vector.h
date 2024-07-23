#ifndef csv2vector_h
#define csv2vector_h

#include "ofxOceanodeNodeModel.h"
#include <sstream>
#include <string>
#include <vector>
#include <limits>

class csv2vector : public ofxOceanodeNodeModel {
public:
    csv2vector() : ofxOceanodeNodeModel("CSV to Vector") {
        addParameter(csvInput.set("CSV Input", "0,1,2,3"));
        addOutputParameter(output.set("Output",
                                      vector<float>(),  // default value
                                      vector<float>(1, -std::numeric_limits<float>::max()),  // min value
                                      vector<float>(1, std::numeric_limits<float>::max())));  // max value

        listeners.push(csvInput.newListener([this](string &s){
            parseCSV(s);
        }));
    }

    void setup() override {
        parseCSV(csvInput);
    }

private:
    ofParameter<string> csvInput;
    ofParameter<vector<float>> output;
    ofEventListeners listeners;

    void parseCSV(const string &csv) {
        vector<float> result;
        stringstream ss(csv);
        string item;

        while (getline(ss, item, ',')) {
            // Trim whitespace
            item.erase(0, item.find_first_not_of(" \t\n\r\f\v"));
            item.erase(item.find_last_not_of(" \t\n\r\f\v") + 1);

            try {
                float value = stof(item);
                result.push_back(value);
            } catch (const std::invalid_argument& ia) {
                ofLogWarning("csvToVector") << "Invalid number in CSV: " << item;
            } catch (const std::out_of_range& oor) {
                ofLogWarning("csvToVector") << "Number out of range in CSV: " << item;
            }
        }

        if (result.empty()) {
            result.push_back(0);  // Ensure the output is never empty
        }

        output = result;
    }
};

#endif /* csv2vector */
