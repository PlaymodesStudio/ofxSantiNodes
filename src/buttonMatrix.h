#ifndef buttonMatrix_h
#define buttonMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <limits>

class buttonMatrix : public ofxOceanodeNodeModel {
public:
    buttonMatrix() : ofxOceanodeNodeModel("Button Matrix") {}

    void setup() {
        addParameter(x.set("X", 3, 1, 16));
        addParameter(y.set("Y", 2, 1, 16));
        addParameter(buttonSize.set("Button Size", 30.0f, 10.0f, 100.0f));
        addParameter(defaultValue.set("Default", 0.0f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
        addParameter(buttonValues.set("Button Values", vector<float>(6, 0.0f)));
        addParameter(indexOn.set("Index On", -1, -1, 255));  // Assuming a maximum of 256 buttons (16x16)
        addParameter(output.set("Output", 0.0f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
        addParameter(vecout.set("VecOut", vector<int>(6, 0)));

        // Set explicit min and max for vector parameters
        buttonValues.setMin(vector<float>(1, -std::numeric_limits<float>::max()));
        buttonValues.setMax(vector<float>(1, std::numeric_limits<float>::max()));
        vecout.setMin(vector<int>(1, 0));
        vecout.setMax(vector<int>(1, 1));

        listeners.push(x.newListener([this](int &){
            resizeMatrix();
        }));
        listeners.push(y.newListener([this](int &){
            resizeMatrix();
        }));
        listeners.push(defaultValue.newListener([this](float &){
            updateOutputs();
        }));
        listeners.push(buttonValues.newListener([this](vector<float> &){
            updateMatrix();
        }));
        listeners.push(indexOn.newListener([this](int &){
            updateActiveButton();
        }));

        addCustomRegion(customMatrixRegion, [this]() {
            drawCustomGui();
        });

        ofAddListener(ofEvents().mouseReleased, this, &buttonMatrix::mouseReleased);

        // Initialize with default values
        resizeMatrix();
    }

    ~buttonMatrix() {
        ofRemoveListener(ofEvents().mouseReleased, this, &buttonMatrix::mouseReleased);
    }

    void presetSave(ofJson &json) override {
        json["buttonValues"] = buttonValues.get();
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        if (json.count("buttonValues") > 0 && json["buttonValues"].is_array()) {
            vector<float> loadedValues;
            for (const auto &value : json["buttonValues"]) {
                if (value.is_number()) {
                    loadedValues.push_back(value.get<float>());
                }
            }
            buttonValues = loadedValues;
        }
    }

private:
    ofParameter<int> x;
    ofParameter<int> y;
    ofParameter<float> buttonSize;
    ofParameter<float> defaultValue;
    ofParameter<vector<float>> buttonValues;
    ofParameter<int> indexOn;
    ofParameter<float> output;
    ofParameter<vector<int>> vecout;
    int activeButton = -1;
    bool isMousePressed = false;
    ofEventListeners listeners;
    customGuiRegion customMatrixRegion;

    void resizeMatrix() {
        int totalSize = x.get() * y.get();
        vector<float> currentValues = buttonValues.get();
        currentValues.resize(totalSize, 0.0f);
        buttonValues = currentValues;
        indexOn.setMax(totalSize - 1);  // Update the maximum allowed index
        updateOutputs();
    }

    void updateMatrix() {
        updateOutputs();
    }

    void updateActiveButton() {
        int index = indexOn.get();
        int totalSize = x.get() * y.get();
        if (index >= 0 && index < totalSize) {
            activeButton = index;
        } else {
            activeButton = -1;
        }
        updateOutputs();
    }

    void updateOutputs() {
        int totalSize = x.get() * y.get();
        vector<float> currentValues = buttonValues.get();
        currentValues.resize(totalSize, 0.0f);
        
        vector<int> outputVec(totalSize, 0);
        
        if (activeButton >= 0 && activeButton < totalSize) {
            outputVec[activeButton] = 1;
            output = currentValues[activeButton];
        } else {
            output = defaultValue.get();
        }
        
        vecout = outputVec;
    }

    void drawCustomGui() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        float currentButtonSize = buttonSize.get();
        float spacing = 5.0f;
        
        int currentX = x.get();
        int currentY = y.get();
        vector<float> currentValues = buttonValues.get();
        
        for (int i = 0; i < currentY; ++i) {
            for (int j = 0; j < currentX; ++j) {
                ImVec2 buttonPos(pos.x + j * (currentButtonSize + spacing), pos.y + i * (currentButtonSize + spacing));
                ImVec2 buttonPosEnd(buttonPos.x + currentButtonSize, buttonPos.y + currentButtonSize);
                
                int buttonIndex = i * currentX + j;
                bool isActive = (buttonIndex == activeButton);
                
                ImU32 buttonColor = isActive ? IM_COL32(100, 255, 100, 255) : IM_COL32(200, 200, 200, 255);
                drawList->AddRectFilled(buttonPos, buttonPosEnd, buttonColor);
                
                // Draw the value in the middle of the button
                if (buttonIndex < currentValues.size()) {
                    char valueBuf[16];
                    snprintf(valueBuf, sizeof(valueBuf), "%.2f", currentValues[buttonIndex]);
                    ImVec2 textSize = ImGui::CalcTextSize(valueBuf);
                    ImVec2 textPos(buttonPos.x + (currentButtonSize - textSize.x) * 0.5f, buttonPos.y + (currentButtonSize - textSize.y) * 0.5f);
                    drawList->AddText(textPos, IM_COL32(0, 0, 0, 255), valueBuf);
                }
                
                // Handle button interactions
                ImGui::SetCursorScreenPos(buttonPos);
                ImGui::InvisibleButton(("##button" + std::to_string(buttonIndex)).c_str(), ImVec2(currentButtonSize, currentButtonSize));
                
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    indexOn = buttonIndex;
                    isMousePressed = true;
                }
            }
        }
    }

    void mouseReleased(ofMouseEventArgs &args) {
        if (isMousePressed) {
            indexOn = -1;
            isMousePressed = false;
            updateActiveButton();
        }
    }
};

#endif /* buttonMatrix_h */
