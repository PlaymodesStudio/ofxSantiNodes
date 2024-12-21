#ifndef VECTOR_FILE_H
#define VECTOR_FILE_H

#include "ofxOceanodeNodeModel.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>

class vectorFile : public ofxOceanodeNodeModel {
public:
    vectorFile() : ofxOceanodeNodeModel("Vector File") {
        // Add parameters
        addParameter(open.set("OpenFile"));
        addParameter(filepath.set("FilePath", ""));
        addParameter(totalLines.set("Total Lines", 0, 0, INT_MAX));
        addParameter(line.set("Line", 0, 0, INT_MAX));
        addParameter(input.set("Input", {0}, {-FLT_MIN}, {FLT_MAX}));
        addParameter(add.set("Add"));
        addParameter(output.set("Output", {0}, {-FLT_MIN}, {FLT_MAX}));
        

        // Setup listeners
        openListener = open.newListener([&](void) {
            auto result = ofSystemLoadDialog("Select a text file", false);
            if(result.bSuccess) {
                filepath = result.filePath;
                currentFilePath = result.filePath;
                readFile();
            }
        });

        addListener = add.newListener([&](void) {
            if(!currentFilePath.empty()) {
                appendLine();
            }
        });

        lineListener = line.newListener([&](int &lineNum) {
            updateOutput(lineNum);
        });
    }

    // Read file and update fileContent vector
    void readFile() {
        std::ifstream file(currentFilePath);
        if (!file.is_open()) {
            ofLogError("Vector File") << "Failed to open file at " << currentFilePath;
            return;
        }

        std::string textLine;
        fileContent.clear();
        while (std::getline(file, textLine)) {
            std::istringstream iss(textLine);
            std::string value;
            std::vector<float> lineValues;
            while (std::getline(iss, value, ',')) {
                try {
                    float val = std::stof(value);
                    lineValues.push_back(val);
                } catch (std::invalid_argument const &e) {
                    ofLogWarning("Vector File") << "Failed to convert string to float: " << value;
                } catch (std::out_of_range const &e) {
                    ofLogWarning("Vector File") << "Float out of range: " << value;
                }
            }
            fileContent.push_back(lineValues);
        }

        // Update line parameter max value
        int maxLines = std::max(0, static_cast<int>(fileContent.size()) - 1);
        line.setMax(maxLines);
        
        // Update total lines count
        totalLines = static_cast<int>(fileContent.size());
        
        // Update output with current line if valid
        int currentLine = line.get();
        if(currentLine >= 0 && currentLine < static_cast<int>(fileContent.size())) {
            updateOutput(currentLine);
        }
    }

    // Append new line to file
    void appendLine() {
        std::ofstream file(currentFilePath, std::ios::app);
        if (!file.is_open()) {
            ofLogError("Vector File") << "Failed to open file for writing at " << currentFilePath;
            return;
        }

        // Add input vector to fileContent
        fileContent.push_back(input.get());

        // Write the new line to file
        const auto& newLine = input.get();
        for (size_t i = 0; i < newLine.size(); ++i) {
            file << newLine[i];
            if (i < newLine.size() - 1) file << ",";
        }
        file << "\n";

        // Update line parameter max value
        int maxLines = std::max(0, static_cast<int>(fileContent.size()) - 1);
        line.setMax(maxLines);
        
        // Update total lines count
        totalLines = static_cast<int>(fileContent.size());
    }

    // Update the output parameter with values from the selected line
    void updateOutput(int lineNum) {
        if (lineNum >= 0 && lineNum < static_cast<int>(fileContent.size())) {
            output.set(fileContent[lineNum]);
        } else {
            output.set(vector<float>()); // Clear output if line number is invalid
        }
    }

    void loadBeforeConnections(ofJson &json) override {
        ofDeserialize(json, filepath);
        if (!filepath.get().empty()) {
            currentFilePath = filepath.get();
            readFile();
        }
    }

private:
    ofParameter<std::string> filepath;
    ofParameter<void> open;
    ofParameter<int> line;
    ofParameter<void> add;
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> output;
    ofParameter<int> totalLines;

    ofEventListener openListener;
    ofEventListener addListener;
    ofEventListener lineListener;

    std::string currentFilePath;
    std::vector<std::vector<float>> fileContent;
};

#endif /* VECTOR_FILE_H */
