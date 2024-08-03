#ifndef noteMatrix_h
#define noteMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include "imgui_internal.h"

class noteMatrix : public ofxOceanodeNodeModel {
public:
    noteMatrix() : ofxOceanodeNodeModel("Note Matrix") {}

    void setup() {
        description="Creates a multi-row step sequencer with variable subdivisions, driven by a phasor input";
        addParameter(gridX.set("Grid X[]", vector<int>(1, 16), vector<int>(1, 1), vector<int>(1, 64)));
        addParameter(gridY.set("Grid Y", 8, 1, 32));
        addParameter(phasorInput.set("Phasor", 0.0f, 0.0f, 1.0f));
        addParameter(output.set("Output", vector<int>()));
        addParameter(matrixWidth.set("w", 300, 100, 1000));
        addParameter(rowHeight.set("h", 20, 10, 100));
        addParameter(slot.set("Slot", 0, 0, NUM_SLOTS - 1));

        matrix.resize(gridY, vector<bool>(16, false));

        listeners.push(gridX.newListener([this](vector<int> &){
            resizeMatrix();
        }));
        listeners.push(gridY.newListener([this](int &){
            resizeMatrix();
        }));
        listeners.push(phasorInput.newListener([this](float &){
            updateOutput();
        }));
        listeners.push(matrixWidth.newListener([this](int &){
            updateOutput();
        }));
        listeners.push(rowHeight.newListener([this](int &){
            updateOutput();
        }));

        output.setMin(vector<int>(1, 0));
        output.setMax(vector<int>(1, 1));

        addCustomRegion(customMatrixRegion, [this]() {
            drawCustomGui();
        });
        
        int y = std::max(1, gridY.get());
           int x = getGridXForRow(0);
           storage[0] = vector<vector<bool>>(y, vector<bool>(x, false));
           
           listeners.push(slot.newListener([this](int &){
               switchSlot();
           }));
    }

    void update(ofEventArgs &a) {
        updateOutput();
    }
    
    void presetSave(ofJson &json) override {
        json["currentSlot"] = slot.get();
        for (const auto& pair : storage) {
            vector<vector<int>> serializedMatrix;
            for (const auto &row : pair.second) {
                vector<int> serializedRow;
                for (bool cell : row) {
                    serializedRow.push_back(cell ? 1 : 0);
                }
                serializedMatrix.push_back(serializedRow);
            }
            json["slotData_" + ofToString(pair.first)] = serializedMatrix;
        }
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        storage.clear();
        
        if (json.count("currentSlot") > 0) {
            slot = json["currentSlot"].get<int>();
        }
        
        for (auto it = json.begin(); it != json.end(); ++it) {
            if (it.key().substr(0, 9) == "slotData_") {
                int slotIndex = ofToInt(it.key().substr(9));
                auto &jsonMatrix = it.value();
                if (jsonMatrix.is_array()) {
                    vector<vector<bool>> slotMatrix;
                    for (const auto &jsonRow : jsonMatrix) {
                        if (jsonRow.is_array()) {
                            vector<bool> row;
                            for (const auto &cell : jsonRow) {
                                row.push_back(cell.get<int>() != 0);
                            }
                            slotMatrix.push_back(row);
                        }
                    }
                    storage[slotIndex] = slotMatrix;
                }
            }
        }
        
        loadSlotData(slot);
        resizeMatrix();
        updateOutput();
    }

private:
    ofParameter<vector<int>> gridX;
    ofParameter<int> gridY;
    ofParameter<float> phasorInput;
    ofParameter<vector<int>> output;
    ofParameter<int> matrixWidth;
    ofParameter<int> rowHeight;
    vector<vector<bool>> matrix;
    ofEventListeners listeners;
    customGuiRegion customMatrixRegion;
    vector<vector<vector<bool>>> allSlotData;
    ofParameter<int> slot;
    static const int NUM_SLOTS = 10;
    std::map<int, vector<vector<bool>>> storage;


    void switchSlot() {
        loadSlotData(slot);
        updateOutput();
    }

    void saveCurrentSlotData() {
        storage[slot] = matrix;
    }

    void loadSlotData(int slotIndex) {
        if (storage.find(slotIndex) != storage.end()) {
            matrix = storage[slotIndex];
        } else {
            // If the slot doesn't exist, create an empty matrix
            int y = std::max(1, gridY.get());
            matrix.resize(y);
            for (int i = 0; i < y; ++i) {
                int x = getGridXForRow(i);
                matrix[i].resize(x, false);
            }
        }
    }
    
    void resizeMatrix() {
        int y = std::max(1, gridY.get());
        
        for (auto& pair : storage) {
            auto& slotMatrix = pair.second;
            slotMatrix.resize(y);
            for (int i = 0; i < y; ++i) {
                int x = getGridXForRow(i);
                if (slotMatrix[i].size() != x) {
                    vector<bool> resizedRow(x, false);
                    for (int j = 0; j < std::min(x, static_cast<int>(slotMatrix[i].size())); ++j) {
                        resizedRow[j] = slotMatrix[i][j];
                    }
                    slotMatrix[i] = resizedRow;
                }
            }
        }
        
        loadSlotData(slot);
        updateOutput();
    }

    void updateOutput() {
        int y = std::max(1, gridY.get());
        float phasor = ofClamp(phasorInput.get(), 0.0f, 1.0f);
        
        vector<int> outp(y, 0);
        for (int i = 0; i < y && i < matrix.size(); ++i) {
            int x = getGridXForRow(i);
            int currentColumn = static_cast<int>(phasor * x) % x;
            if (currentColumn < matrix[i].size() && matrix[i][currentColumn]) {
                outp[i] = 1;
            }
        }
        output = outp;
    }

    void drawCustomGui() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        int y = std::max(1, gridY.get());
        float totalWidth = matrixWidth.get();
        float rowHeightValue = rowHeight.get();

        // Create a larger clickable area
        ImGui::InvisibleButton("MatrixArea", ImVec2(totalWidth, y * rowHeightValue));
        bool isMatrixHovered = ImGui::IsItemHovered();

        // Handle mouse interactions
        static bool isDragging = false;
        static bool initialCellState = false;

        if (isMatrixHovered) {
            if (ImGui::IsMouseClicked(0)) {
                isDragging = true;
                ImVec2 mousePos = ImGui::GetMousePos();
                int cellY = static_cast<int>((mousePos.y - pos.y) / rowHeightValue);
                float cellWidth = totalWidth / getGridXForRow(cellY);
                int cellX = static_cast<int>((mousePos.x - pos.x) / cellWidth);

                if (cellY >= 0 && cellY < y && cellX >= 0 && cellX < getGridXForRow(cellY)) {
                    initialCellState = !matrix[cellY][cellX];
                    matrix[cellY][cellX] = initialCellState;
                    updateOutput();
                }
                saveCurrentSlotData();
            }
            else if (ImGui::IsMouseDragging(0) && isDragging) {
                ImVec2 mousePos = ImGui::GetMousePos();
                int cellY = static_cast<int>((mousePos.y - pos.y) / rowHeightValue);
                float cellWidth = totalWidth / getGridXForRow(cellY);
                int cellX = static_cast<int>((mousePos.x - pos.x) / cellWidth);

                if (cellY >= 0 && cellY < y && cellX >= 0 && cellX < getGridXForRow(cellY)) {
                    matrix[cellY][cellX] = initialCellState;
                    updateOutput();
                }
                saveCurrentSlotData();
            }
        }

        if (ImGui::IsMouseReleased(0)) {
            isDragging = false;
        }

        // Draw grid and cells
        for (int i = 0; i < y && i < matrix.size(); ++i) {
            int x = getGridXForRow(i);
            float cellWidth = totalWidth / x;
            for (int j = 0; j < x && j < matrix[i].size(); ++j) {
                ImVec2 cellPos(pos.x + j * cellWidth, pos.y + i * rowHeightValue);
                ImVec2 cellPosEnd(cellPos.x + cellWidth, cellPos.y + rowHeightValue);
                
                if (matrix[i][j]) {
                    drawList->AddRectFilled(cellPos, cellPosEnd, IM_COL32(255, 255, 255, 255));
                }
                drawList->AddRect(cellPos, cellPosEnd, IM_COL32(100, 100, 100, 255));
            }
        }

        // Draw phasor position
        float phasor = ofClamp(phasorInput.get(), 0.0f, 1.0f);
        for (int i = 0; i < y; ++i) {
            float phasorX = phasor * totalWidth;
            ImVec2 phasorStart(pos.x + phasorX, pos.y + i * rowHeightValue);
            ImVec2 phasorEnd(phasorStart.x, pos.y + (i + 1) * rowHeightValue);
            drawList->AddLine(phasorStart, phasorEnd, IM_COL32(255, 0, 0, 255), 2.0f);
        }
    }

    int getGridXForRow(int row) {
        if (gridX->size() == 1) {
            return gridX->at(0);
        }
        return (row < gridX->size()) ? gridX->at(row) : gridX->back();
    }
};

#endif /* noteMatrix_h */
