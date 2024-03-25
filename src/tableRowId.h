
#ifndef TABLEROWID_H
#define TABLEROWID_H

#include "ofxOceanodeNodeModel.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>

class tableRowId : public ofxOceanodeNodeModel {
public:
    tableRowId() : ofxOceanodeNodeModel("Table Row Id") {
        addParameter(filepath.set("FilePath", ""));
        addParameter(open.set("OpenFile"));
        addParameter(save.set("SaveFile"));
        addParameter(saveAs.set("SaveAsFile"));
        addParameter(input.set("Input", {0}, {-FLT_MIN}, {FLT_MAX}));
        addParameter(writeRow.set("WriteRow"));
        addParameter(rRow.set("rRow", 0, 0, 1080));
        addParameter(wRow.set("wRow", 0, 0, 1080));
        addParameter(outputRow.set("Out R", {0}, {-FLT_MIN}, {FLT_MAX}));
        addParameter(rowSize.set("RowSize", 0, 0, INT_MAX));
        addParameter(rCol.set("rCol", 0, 0, 1080));
        addParameter(outputCol.set("Out C", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(colSize.set("ColSize", 0, 0, INT_MAX));
        addParameter(rowFilter.set("RowFilt", ""));
        addParameter(colFilter.set("ColFilt", ""));


        openListener = open.newListener([&](void) {
            auto result = ofSystemLoadDialog("Select a text file", false);
            if(result.bSuccess) {
                filepath = result.filePath;
                currentFilePath = result.filePath;
                readFile();
                updateRowMax();
            }
        });

        saveListener = save.newListener([&](void) {
            if(!currentFilePath.empty()) {
                writeFile(currentFilePath);
            }
        });

        saveAsListener = saveAs.newListener([&](void) {
            auto result = ofSystemSaveDialog("data.txt", "Save your file");
            if(result.bSuccess) {
                filepath = result.filePath;
                writeFile(result.filePath);
            }
        });

        writeRowListener = writeRow.newListener([&](void) {
            writeRowToFile();
        });

        rRowListener = rRow.newListener([&](int &rowNum) {
            updateRowOutput(rowNum);
        });
        
        rColListener = rCol.newListener([&](int &colNum) {
            updateColumnOutput();
        });
        
        rowFilter.addListener(this, &tableRowId::updateColumnOutputBasedOnFilter);
        colFilter.newListener(this, &tableRowId::adjustRColBasedOnFilter);
    }

    void updateRowMax() {
          // Use fileContent's size to update rRow and wRow's max values
          int newSize = static_cast<int>(fileContent.size()) - 1; // Adjust for 0-based indexing
          rRow.setMax(newSize);
          wRow.setMax(newSize + 1); // wRow can potentially add a new row, hence newSize + 1
      }
    
    void updateColumnMax() {
        int maxColumns = 0;
        for (const auto& row : fileContent) {
            if (row.size() > maxColumns) {
                maxColumns = row.size();
            }
        }
        // Set the maximum value for rCol based on the widest row
        // Subtract 1 because column indices are 0-based
        rCol.setMax(maxColumns > 0 ? maxColumns - 1 : 0);
    }
    
    // Read file and update fileContent vector
    void readFile() {
        std::ifstream file(currentFilePath);
        if (!file.is_open()) {
            ofLogError("Table") << "Failed to open file at " << currentFilePath;
            return;
        }
        std::string line;
        fileContent.clear();
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string value;
            std::vector<float> rowValues;
            while (std::getline(iss, value, ',')) { //use comma as separator between values
                try {
                    float val = std::stof(value);
                    rowValues.push_back(val);
                } catch (std::invalid_argument const &e) {
                    ofLogWarning("Table") << "Failed to convert string to float: " << value;
                } catch (std::out_of_range const &e) {
                    ofLogWarning("Table") << "Float out of range: " << value;
                }
            }
            fileContent.push_back(rowValues);
        }
        
        rowSize = static_cast<int>(fileContent.size());
        
        updateRowMax(); // Update rRow and wRow max values based on the new file content
        updateColumnMax(); // Also update rCol's max value after reading file
        updateColSize();
        
        if(rRow.get() >= 0 && rRow.get() < fileContent.size()) {
            updateRowOutput(rRow.get());
        }
        else {
            outputRow.set(vector<float>()); // Clear the output if the selected row is out of the current content range
        }
        
    }
    void updateColSize() {
            int maxColumns = 0;
            for (const auto& row : fileContent) {
                maxColumns = std::max(maxColumns, static_cast<int>(row.size()));
            }
            colSize = maxColumns; // Set colSize to the maximum number of columns found
        }

    // Write current content to file
    void writeFile(const std::string& path) {
        std::ofstream file(path);
        if (!file.is_open()) {
            ofLogError("Table") << "Failed to open file for writing at " << path;
            return;
        }
        for (auto & rRow : fileContent) {
            for (size_t i = 0; i < rRow.size(); ++i) {
                file << rRow[i];
                if (i < rRow.size() - 1) file << ","; //use comma as separator between values
            }
            file << "\n";
        }
    }
    
    void writeRowToFile() {
        if(wRow.get() >= 0 && wRow.get() <= fileContent.size()) {
            // Ensure fileContent is large enough to include the wRow index
            if (wRow.get() == fileContent.size()) {
                // Adding a new row at the end of fileContent
                fileContent.push_back(input.get());
            } else {
                // Replacing an existing row
                fileContent[wRow.get()] = input.get();
            }

            // Write the modified content back to the file
            writeFile(currentFilePath);
            updateColumnMax(); // Update rCol's max after modifying fileContent
            updateColSize(); // Update rCol's max after modifying fileContent

            rowSize = static_cast<int>(fileContent.size());
            rRow.setMax(static_cast<int>(fileContent.size()) - 1);
            wRow.setMax(static_cast<int>(fileContent.size()));
        } else {
            ofLogWarning("Table") << "wRow is out of range: " << wRow.get();
        }
    }
    
    void adjustRColBasedOnFilter(string& filter) {
        if (fileContent.empty()) return; // Ensure there's data

        auto& headerRow = fileContent.front(); // Assuming the first row contains headers

        // Convert header row values to strings and search for a match with 'filter'
        for (size_t colIndex = 0; colIndex < headerRow.size(); ++colIndex) {
            // Assuming headerRow[colIndex] can be converted to a string
            // Adjust this part according to your data structure
            string colName = std::to_string(headerRow[colIndex]); // Example conversion, adjust as needed

            if (colName == filter) {
                rCol = colIndex; // Found a match, update rCol
                return;
            }
        }

        // Optional: Handle case where no match is found, e.g., reset rCol or leave it unchanged
    }

    void updateColumnOutputBasedOnFilter(string&) {
        updateColumnOutput();
    }

    // Update the output parameter with values from the selected row
    void updateColumnOutput() {
        // Temporary vector to hold filtered column data
        std::vector<float> tempOutputCol;

        // Only apply filtering if rowFilter is not empty and not the default "None" value
        if (!rowFilter.get().empty() && rowFilter.get() != "None") {
            // Iterate through each row to check the first column against rowFilter
            for (size_t i = 1; i < fileContent.size(); i++) { // Assuming the first row contains headers
                std::string firstColumnValue = std::to_string(fileContent[i][0]);
                if (firstColumnValue.find(rowFilter.get()) != std::string::npos) {
                    // If rCol is valid for this row, add the value to tempOutputCol
                    if (rCol.get() < fileContent[i].size()) {
                        tempOutputCol.push_back(fileContent[i][rCol.get()]);
                    }
                    // Optional: handle missing data for this row
                }
            }
        } else {
            // If rowFilter is empty or "None", populate tempOutputCol as before without filtering
            for (const auto& row : fileContent) {
                if (rCol.get() < row.size()) {
                    tempOutputCol.push_back(row[rCol.get()]);
                }
                // Optional: handle missing data for this row
            }
        }

        // Update outputCol with the filtered or unfiltered data
        outputCol.set(tempOutputCol);
    }

    
    
    
    void updateRowOutput(int rowNum) {
           if (rowNum >= 0 && rowNum < fileContent.size()) {
               outputRow.set(fileContent[rowNum]);
               // Call updateColumnOutput to refresh column data based on the current rCol
               updateColumnOutput();
           } else {
               outputRow.set(vector<float>()); // Clear the output if the selected row is out of range
               outputCol.set(vector<float>()); // Also clear the column output
           }
       }
    
    void loadBeforeConnections(ofJson &json) override {
           ofDeserialize(json, filepath); // Make sure to deserialize filepath
           if (!filepath.get().empty()) {
               currentFilePath = filepath; // Update currentFilePath
               readFile(); // Automatically load the file based on the saved path
           }
       }


private:
    ofParameter<std::string> filepath;
    ofParameter<void> open;
    ofParameter<void> save;
    ofParameter<void> saveAs;
    ofParameter<void> writeRow;
    ofParameter<int> rRow;
    ofParameter<int> wRow;
    ofParameter<int> rowSize;
    ofParameter<int> colSize;
    ofParameter<int> rCol;
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> outputRow;
    ofParameter<vector<float>> outputCol;
    ofParameter<string> rowFilter;
    ofParameter<string> colFilter;


    ofEventListener openListener;
    ofEventListener saveListener;
    ofEventListener saveAsListener;
    ofEventListener writeRowListener;
    ofEventListener rRowListener;
    ofEventListener rColListener;
    

    std::string currentFilePath;
    std::vector<std::vector<float>> fileContent;
};

#endif /* TABLEROWID_H */