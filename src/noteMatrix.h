#ifndef noteMatrix_h
#define noteMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>

class noteMatrix : public ofxOceanodeNodeModel {
public:
    noteMatrix() : ofxOceanodeNodeModel("Note Matrix") {}

    void setup() {
        addParameter(gridX.set("Grid X", 16, 1, 64));
        addParameter(gridY.set("Grid Y", 8, 1, 32));
        addParameter(phasorInput.set("Phasor", 0.0f, 0.0f, 1.0f));
        addParameter(matrixOutput.set("Output", vector<int>()));

        matrix.resize(gridY, vector<bool>(gridX, false));

        listeners.push(gridX.newListener([this](int &){
            resizeMatrix();
        }));
        listeners.push(gridY.newListener([this](int &){
            resizeMatrix();
        }));
        listeners.push(phasorInput.newListener([this](float &){
            updateOutput();
        }));

        matrixOutput.setMin(vector<int>(1, 0));
        matrixOutput.setMax(vector<int>(1, 1));

        // Add custom region for drawing the matrix
        addCustomRegion(customMatrixRegion, [this]() {
            drawCustomGui();
        });
    }

    void update(ofEventArgs &a) {
        updateOutput();
    }
    
    void presetSave(ofJson &json) override {
            vector<vector<int>> serializedMatrix;
            for (const auto &row : matrix) {
                vector<int> serializedRow;
                for (bool cell : row) {
                    serializedRow.push_back(cell ? 1 : 0);
                }
                serializedMatrix.push_back(serializedRow);
            }
            json["matrix"] = serializedMatrix;
        }

        void presetRecallAfterSettingParameters(ofJson &json) override {
            if (json.count("matrix") > 0) {
                auto &jsonMatrix = json["matrix"];
                if (jsonMatrix.is_array()) {
                    matrix.clear();
                    for (const auto &jsonRow : jsonMatrix) {
                        if (jsonRow.is_array()) {
                            vector<bool> row;
                            for (const auto &cell : jsonRow) {
                                row.push_back(cell.get<int>() != 0);
                            }
                            matrix.push_back(row);
                        }
                    }
                    resizeMatrix();
                    updateOutput();
                }
            }
        }

private:
    ofParameter<int> gridX, gridY;
    ofParameter<float> phasorInput;
    ofParameter<vector<int>> matrixOutput;
    vector<vector<bool>> matrix;
    ofEventListeners listeners;
    customGuiRegion customMatrixRegion;

    void resizeMatrix() {
            int x = std::max(1, gridX.get());
            int y = std::max(1, gridY.get());
            
            vector<vector<bool>> newMatrix(y, vector<bool>(x, false));
            for (int i = 0; i < std::min(y, static_cast<int>(matrix.size())); ++i) {
                for (int j = 0; j < std::min(x, static_cast<int>(matrix[i].size())); ++j) {
                    newMatrix[i][j] = matrix[i][j];
                }
            }
            matrix = newMatrix;
            
            updateOutput();
        }

    void updateOutput() {
            int x = std::max(1, gridX.get());
            int y = std::max(1, gridY.get());
            float phasor = ofClamp(phasorInput.get(), 0.0f, 1.0f);
            int currentColumn = static_cast<int>(phasor * x) % x;
            
            vector<int> output(y, 0);
            for (int i = 0; i < y && i < matrix.size(); ++i) {
                if (currentColumn < matrix[i].size() && matrix[i][currentColumn]) {
                    output[i] = 1;
                }
            }
            matrixOutput = output;
        }

    void drawCustomGui() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float cellWidth = 20.0f;
        float cellHeight = 20.0f;

        int x = std::max(1, gridX.get());
        int y = std::max(1, gridY.get());

        // Create a larger clickable area
        ImGui::InvisibleButton("MatrixArea", ImVec2(x * cellWidth, y * cellHeight));
        bool isMatrixHovered = ImGui::IsItemHovered();

        // Handle mouse interactions
        static bool isDragging = false;
        static bool initialCellState = false;

        if (isMatrixHovered) {
            if (ImGui::IsMouseClicked(0)) {
                isDragging = true;
                ImVec2 mousePos = ImGui::GetMousePos();
                int cellX = static_cast<int>((mousePos.x - pos.x) / cellWidth);
                int cellY = static_cast<int>((mousePos.y - pos.y) / cellHeight);

                if (cellX >= 0 && cellX < x && cellY >= 0 && cellY < y) {
                    initialCellState = !matrix[cellY][cellX];
                    matrix[cellY][cellX] = initialCellState;
                    updateOutput();
                }
            }
            else if (ImGui::IsMouseDragging(0) && isDragging) {
                ImVec2 mousePos = ImGui::GetMousePos();
                int cellX = static_cast<int>((mousePos.x - pos.x) / cellWidth);
                int cellY = static_cast<int>((mousePos.y - pos.y) / cellHeight);

                if (cellX >= 0 && cellX < x && cellY >= 0 && cellY < y) {
                    matrix[cellY][cellX] = initialCellState;
                    updateOutput();
                }
            }
        }

        if (ImGui::IsMouseReleased(0)) {
            isDragging = false;
        }

        // Draw grid and cells
        for (int i = 0; i < y && i < matrix.size(); ++i) {
            for (int j = 0; j < x && j < matrix[i].size(); ++j) {
                ImVec2 cellPos(pos.x + j * cellWidth, pos.y + i * cellHeight);
                ImVec2 cellPosEnd(cellPos.x + cellWidth, cellPos.y + cellHeight);
                
                if (matrix[i][j]) {
                    drawList->AddRectFilled(cellPos, cellPosEnd, IM_COL32(255, 255, 255, 255));
                }
                drawList->AddRect(cellPos, cellPosEnd, IM_COL32(100, 100, 100, 255));
            }
        }

        // Draw phasor position
        float phasor = ofClamp(phasorInput.get(), 0.0f, 1.0f);
        int phasorX = static_cast<int>(phasor * x) % x;
        ImVec2 phasorStart(pos.x + phasorX * cellWidth, pos.y);
        ImVec2 phasorEnd(phasorStart.x, pos.y + y * cellHeight);
        drawList->AddLine(phasorStart, phasorEnd, IM_COL32(255, 0, 0, 255), 2.0f);
    }
};
#endif /* noteMatrix_h */
