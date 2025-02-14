#ifndef probabilityDropdownList_h
#define probabilityDropdownList_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"

class probabilityDropdownList : public ofxOceanodeNodeModel {
public:
    probabilityDropdownList() : ofxOceanodeNodeModel("Probability Dropdown List") {
        description = "Generates a space-separated list of strings based on probability values for selected elements";
    }
    
    void setup() override {
        // Predefined lists for each category
        rhythmOptions = {
            "none",
            "kick_pattern_1",
            "snare_groove",
            "hihat_16th",
            "percussion_latin"
        };
        
        textureOptions = {
            "none",
            "pad_warm",
            "strings_sus",
            "noise_filtered",
            "grain_cloud"
        };
        
        harmonyOptions = {
            "none",
            "chord_maj7",
            "arp_minor",
            "bass_walking",
            "progression_ii_V_I"
        };

        fxOptions = {
            "none",
            "reverb_long",
            "delay_tape",
            "distortion",
            "filter_sweep",
            "bitcrush"
        };

        // Add parameters
        addParameter(widgetWidth.set("Width", 200, 100, 500));
        addParameter(numRows.set("Rows", 8, 1, 32));
        addParameter(globalProb.set("Global Prob", 100, 0, 100));
        
        // Initialize cell data
        cellContents.resize(32);  // Maximum size
        probabilities.resize(32, 0);
        lastResults.resize(32, false);  // Track last randomization results
        
        // Add trigger and output parameters
        addParameter(trigger.set("GO"));
        addOutputParameter(output.set("Output", ""));  // String output
        addOutputParameter(numElements.set("Num Elements", 0, 0, 32));  // Count output as output parameter
        
        // Add custom region for ImGui interface
        addCustomRegion(guiRegion, [this](){
            drawGui();
        });
        
        // Setup trigger listener for output generation
        listeners.push(trigger.newListener([this]() {
            generateOutput();
        }));

        // Setup listener for row count changes
        listeners.push(numRows.newListener([this](int &n) {
            if(cellContents.size() > n) {
                cellContents.resize(n);
                probabilities.resize(n);
                lastResults.resize(n);
            }
        }));
    }

private:
    void drawGui() {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
        
        float currentWidth = widgetWidth.get();
        ImGui::SetNextItemWidth(currentWidth);
        
        // Draw the main table
        if (ImGui::BeginTable("##ProbabilityTable", 5, ImGuiTableFlags_Borders, ImVec2(currentWidth, 0))) {
            ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Prob%", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Clear", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Move", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableHeadersRow();

            for (int row = 0; row < numRows; row++) {
                ImGui::TableNextRow();
                
                // Element column
                ImGui::TableSetColumnIndex(0);
                string displayText = cellContents[row].second.empty() ? "none" : cellContents[row].second;
                ImGui::PushID(row);  // Push unique ID for the popup
                if (ImGui::Selectable(displayText.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                    ImGui::OpenPopup("dropdown_menu");
                }

                // Dropdown menu for element selection
                if (ImGui::BeginPopup("dropdown_menu")) {
                    if (ImGui::BeginMenu("Rhythm")) {
                        for (const auto& rhythm : rhythmOptions) {
                            if (ImGui::MenuItem(rhythm.c_str())) {
                                cellContents[row] = std::make_pair("rhythm", rhythm);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Texture")) {
                        for (const auto& texture : textureOptions) {
                            if (ImGui::MenuItem(texture.c_str())) {
                                cellContents[row] = std::make_pair("texture", texture);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Harmony")) {
                        for (const auto& harmony : harmonyOptions) {
                            if (ImGui::MenuItem(harmony.c_str())) {
                                cellContents[row] = std::make_pair("harmony", harmony);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("FX")) {
                        for (const auto& fx : fxOptions) {
                            if (ImGui::MenuItem(fx.c_str())) {
                                cellContents[row] = std::make_pair("fx", fx);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();  // Pop the unique ID

                // Probability column
                ImGui::TableSetColumnIndex(1);
                int prob = probabilities[row];
                if (ImGui::DragInt(("##prob" + ofToString(row)).c_str(), &prob, 1, 0, 100)) {
                    probabilities[row] = prob;
                }

                // Result column
                ImGui::TableSetColumnIndex(2);
                ImGui::PushStyleColor(ImGuiCol_Button, lastResults[row] ?
                    ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::Button(("##result" + ofToString(row)).c_str(), ImVec2(20, 20));
                ImGui::PopStyleColor();

                // Clear button column
                ImGui::TableSetColumnIndex(3);
                if (ImGui::Button(("X##" + ofToString(row)).c_str())) {
                    cellContents[row] = std::make_pair("", "");
                    probabilities[row] = 0;
                }

                // Move buttons column
                ImGui::TableSetColumnIndex(4);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                // Group arrows horizontally but keep up/down directions
                float arrowSize = 10.0f;
                ImGui::BeginGroup();
                if (row > 0) {  // Up arrow
                    if (ImGui::ArrowButton(("##up" + ofToString(row)).c_str(), ImGuiDir_Up)) {
                        moveCell(row, row - 1);
                    }
                } else {
                    ImGui::InvisibleButton(("##up" + ofToString(row)).c_str(), ImVec2(arrowSize, arrowSize));
                }
                ImGui::SameLine(0, 2.0f);  // Small spacing between arrows
                if (row < numRows - 1) {  // Down arrow
                    if (ImGui::ArrowButton(("##down" + ofToString(row)).c_str(), ImGuiDir_Down)) {
                        moveCell(row, row + 1);
                    }
                } else {
                    ImGui::InvisibleButton(("##down" + ofToString(row)).c_str(), ImVec2(arrowSize, arrowSize));
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            ImGui::EndTable();
        }
        
        ImGui::PopStyleVar();
    }
    
    void generateOutput() {
        string result;
        int count = 0;
        
        // First check global probability
        float globalRandom = ofRandom(100);
        bool globalPass = (globalRandom < globalProb);
        
        // If global probability fails, clear everything and return
        if (!globalPass) {
            output = "";
            numElements = 0;
            // Mark all cells as failed
            for(int i = 0; i < numRows; i++) {
                lastResults[i] = false;
            }
            return;
        }
        
        // If we passed global probability, process each cell
        for(int i = 0; i < numRows; i++) {
            // Skip empty cells or cells with "none"
            if(cellContents[i].second.empty() || cellContents[i].second == "none") {
                lastResults[i] = false;
                continue;
            }
            
            // Generate random number (0-100)
            float random = ofRandom(100);
            
            // Check individual probability
            bool succeeded = (random < probabilities[i]);
            lastResults[i] = succeeded;
            if(succeeded) {
                if(!result.empty()) result += " ";  // Add space separator if not first item
                result += cellContents[i].second;
                count++;
            }
        }
        
        // Update output parameters
        output = result;
        numElements = count;
    }
    
    void moveCell(int fromIdx, int toIdx) {
        if (fromIdx < 0 || fromIdx >= numRows || toIdx < 0 || toIdx >= numRows) return;
        
        // Swap all cell data
        std::swap(cellContents[fromIdx], cellContents[toIdx]);
        std::swap(probabilities[fromIdx], probabilities[toIdx]);
        std::swap(lastResults[fromIdx], lastResults[toIdx]);
    }

    vector<string> rhythmOptions;
    vector<string> textureOptions;
    vector<string> harmonyOptions;
    vector<string> fxOptions;
    
    vector<pair<string, string>> cellContents;  // (category, value)
    vector<int> probabilities;
    vector<bool> lastResults;  // Track last randomization results
    
    ofParameter<void> trigger;
    ofParameter<string> output;      // String output
    ofParameter<int> numElements;    // Count output
    ofParameter<int> widgetWidth;
    ofParameter<int> numRows;
    ofParameter<int> globalProb;
    customGuiRegion guiRegion;
    ofEventListeners listeners;
};

#endif /* probabilityDropdownList_h */
