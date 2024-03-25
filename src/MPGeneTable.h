#ifndef GENETABLE_H
#define GENETABLE_H

#include "ofxOceanodeNodeModel.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>

class geneTable : public ofxOceanodeNodeModel {
public:
    geneTable() : ofxOceanodeNodeModel("Gene Table") {
        // Predefined list of sample points for dropdown
        vector<string> samplePoints = {
            "MP0311", "MP0313", "MP0315", "MP0317", "MP0319", "MP0321", "MP0323", "MP0528", "MP0530",
            "MP0532", "MP0534", "MP0536", "MP0538", "MP0540", "MP0778", "MP0780", "MP0782", "MP0784",
            "MP0786", "MP0788", "MP0790", "MP0878", "MP0880", "MP0882", "MP0884", "MP0886", "MP0888",
            "MP1154", "MP1162", "MP1164", "MP1166", "MP1174", "MP1176", "MP1178", "MP1409", "MP1411",
            "MP1413", "MP1415", "MP1417", "MP1419", "MP1421", "MP1517", "MP1519", "MP1521", "MP1523",
            "MP1525", "MP1527", "MP1529", "MP1672", "MP1674", "MP1676", "MP1678", "MP1680", "MP1682",
            "MP1684", "MP1845", "MP1847", "MP1849", "MP1851", "MP1853", "MP1855", "MP1857", "MP2231",
            "MP2233", "MP2235", "MP2237", "MP2239", "MP2241", "MP2243", "MP2809", "MP2811", "MP2813",
            "MP2815", "MP2817", "MP2819", "MP2821"
        };
        addParameter(filepath.set("FilePath", ""));
        addParameter(open.set("OpenFile"));
        addParameterDropdown(selectedSamplePoint, "Sample", 0, samplePoints);
        addOutputParameter(outputData.set("Output", {0}, {0}, {1}));

        openListener = open.newListener([&](void) {
            auto result = ofSystemLoadDialog("Select a text file", false);
            if(result.bSuccess) {
                filepath = result.filePath;
                readFile();
            }
        });

        // The listener will trigger after the file is loaded to update the output based on the selected sample point
        samplePointListener = selectedSamplePoint.newListener([&](int &index) {
            if(!fileContent.empty()) {
                updateOutputData(index + 1); // +1 to skip header row in fileContent
            }
        });
    }

private:
    void readFile() {
        std::ifstream file(filepath.get());
        if (!file.is_open()) {
            ofLogError("geneTable") << "Failed to open file at " << filepath.get();
            return;
        }
        std::string line;
        fileContent.clear();
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::vector<float> rowValues;
            std::string value;
            while (std::getline(iss, value, ',')) {
                try {
                    rowValues.push_back(std::stof(value));
                } catch (...) {
                    ofLogWarning("geneTable") << "Conversion to float failed for value: " << value;
                }
            }
            fileContent.push_back(rowValues);
        }
    }

    void updateOutputData(int columnIndex) {
        std::vector<float> columnData;
        for (auto& row : fileContent) {
            if (columnIndex < row.size()) {
                columnData.push_back(row[columnIndex]);
            }
            // Handle missing data if necessary
        }
        outputData.set(columnData);
    }
    
    void loadBeforeConnections(ofJson &json) override {
        // Ensure to deserialize the filepath parameter from the saved preset JSON
        ofDeserialize(json, filepath);
        // If the filepath is not empty after deserialization, proceed to load the file
        if (!filepath.get().empty()) {
            readFile(); // Call readFile to load and process the file based on the deserialized filepath
        }
    }

    ofParameter<std::string> filepath;
    ofParameter<void> open;
    ofParameter<int> selectedSamplePoint;
    ofParameter<std::vector<float>> outputData;

    std::vector<std::vector<float>> fileContent;

    ofEventListener openListener;
    ofEventListener samplePointListener;
};

#endif /* GENETABLE_H */
