#ifndef VERTICALPROFILE_H
#define VERTICALPROFILE_H

#include "ofxOceanodeNodeModel.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>
#include <string>

using namespace std;

class verticalProfile : public ofxOceanodeNodeModel {
public:
    verticalProfile() : ofxOceanodeNodeModel("Vertical Profile Table") {
        vector<string> columnNames = {"MPCode", "Leg", "Station", "latitude", "longitude", "sampling_time_start", "Pres", "depth", "MaxZ", "WATER_MASS_TYPE", "percent_waterTYPE", "a254", "SR", "LonghurstProvince", "Salinity_WOA13", "NO3_WOA13", "PO4_WOA13", "SiO4_WOA13", "PROVINCIA", "percentPAR", "Cast", "MLD", "conductivity", "salinity", "temperature", "oxygen", "oxygen_concentration", "fluorescence", "PAR_flat", "PAR_spherical", "turbidity", "backscattering_coef", "Oxygen", "sigma", "O2_umol_kg", "O2_corr_umol_kg", "O2_sat", "AOU_corr_umol/kg", "Chla_ugl", "Fmax1_resp_prok", "Fmax2_resp_euk", "Fmax3_tirosina", "Fmax4_triptofano", "TEP", "POC_uM", "Turb", "pmol_leu", "SE", "LNA", "HNA", "All_BT", "percentHNA", "cell size", "Bacterial cell C", "Biomass", "ugC_l_d", "d_1", "turnover_days", "HNF", "low_virus", "medium_virus", "high_virus", "all_virus", "VBR"};
        
        vector<string> stationNumbers = {"19", "30", "44", "49", "63", "76", "83", "92", "101","120","141"};
        
        
        addParameter(filepath.set("FilePath", ""));
        addParameter(open.set("OpenFile"));
        addParameterDropdown(selectedColumn, "Column", 0, columnNames);
        addParameterDropdown(selectedStation, "Station", 0, stationNumbers);
        addOutputParameter(outputData.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        openListener = open.newListener([&](void) {
            auto result = ofSystemLoadDialog("Select a text file", false);
            if(result.bSuccess) {
                filepath = result.filePath;
                readFile();
            }
        });

        auto updateOutput = [&](int &){
            if(!fileContent.empty()) {
                updateOutputData(selectedColumn, selectedStation);
            }
        };
        selectedColumn.newListener(updateOutput);
        selectedStation.newListener(updateOutput);
    }


    void readFile() {
        ifstream file(filepath.get());
        if (!file.is_open()) {
            ofLogError("verticalProfile") << "Failed to open file at " << filepath.get();
            return;
        }
        fileContent.clear();
        string line;
        getline(file, line);  // Read the first line with column headers
        istringstream iss(line);
        string value;
        vector<string> headers;
        while (getline(iss, value, ',')) {  // Assuming CSV format
            headers.push_back(value);
        }
        fileContent.push_back(headers);  // Store the headers
        
        // Find the index of the "Station" column
        stationColumnIndex = find(headers.begin(), headers.end(), "Station") - headers.begin();
        if (stationColumnIndex >= headers.size()) {
            ofLogError("verticalProfile") << "Station column not found";
            return;
        }

        // Read the rest of the file
        while (getline(file, line)) {
            istringstream iss(line);
            vector<string> rowValues;
            while (getline(iss, value, ',')) {
                rowValues.push_back(value);
            }
            fileContent.push_back(rowValues);
        }
    }

    
    void updateOutputData(int selectedColumnIndex, int selectedStationIndex) {
        if (selectedStationIndex < 0 || selectedStationIndex >= stationNumbers.size()) {
            cout << "Station index out of bounds." << endl;
            return; // Early exit if station index is invalid
        }
        string stationNumberStr = stationNumbers[selectedStationIndex];
        int stationNumber = 0;
        try {
            stationNumber = stoi(stationNumberStr);
        } catch (const std::exception& e) {
            cout << "Error converting station number: " << stationNumberStr << " to integer." << endl;
            return; // Exit if conversion fails
        }

        cout << "Processing station number (int): " << stationNumber << endl;

        vector<float> columnData;
        for (size_t i = 1; i < fileContent.size(); i++) {
            const auto& row = fileContent[i];
            if (row.size() > stationColumnIndex) {
                int fileStationNumber = 0;
                try {
                    fileStationNumber = stoi(row[stationColumnIndex]);
                } catch (const std::exception& e) {
                    cout << "Error converting file station number: " << row[stationColumnIndex] << " to integer at row " << i << "." << endl;
                    continue; // Skip this row if conversion fails
                }

                if (fileStationNumber == stationNumber) {
                    try {
                        float value = stof(row[selectedColumnIndex]);
                        columnData.push_back(value);
                    } catch (const std::exception& e) {
                        cout << "Error converting value: " << row[selectedColumnIndex] << " to float at row " << i << "." << endl;
                        continue; // Skip this value if conversion fails
                    }
                }
            }
        }

        if (columnData.empty()) {
            cout << "No data found for station " << stationNumber << " in column " << selectedColumnIndex << "." << endl;
        } else {
            cout << "Data found for station " << stationNumber << ": " << columnData.size() << " entries." << endl;
        }
        
        outputData.set(columnData);
    }


private:
    ofParameter<string> filepath;
    ofParameter<void> open;
    ofParameter<int> selectedColumn;
    ofParameter<int> selectedStation;
    ofParameter<vector<float>> outputData;
    size_t stationColumnIndex;

    vector<string> stationNumbers;
    vector<vector<string>> fileContent; // Changed to store strings to handle mixed data types

    ofEventListener openListener;
};

#endif /* VERTICALPROFILE_H */

