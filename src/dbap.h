#ifndef dbap_h
#define dbap_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

class dbap : public ofxOceanodeNodeModel {
public:
    dbap() : ofxOceanodeNodeModel("DBAP") {}
    
    void setup() override {
        description = "Distance-Based Amplitude Panning (DBAP) node that calculates normalized distances between source positions and speaker positions.";
        
        // Input parameters for sources
        addParameter(sourceX.set("X", {0.5f}, {0}, {1}));
        addParameter(sourceY.set("Y", {0.5f}, {0}, {1}));
        
        // Input parameters for speakers
        addParameter(speakerX.set("Speaker X", {0.5f}, {0}, {1}));
        addParameter(speakerY.set("Speaker Y", {0.5f}, {0}, {1}));
        
        // Display parameters
        addParameter(size.set("Size", 240, 100, 500));
        addParameter(rolloff.set("Rolloff", 6.0f, 0.0f, 12.0f));
        
        // Output parameter
        addOutputParameter(distance.set("Distance", {0}, {0}, {1}));
        
        // Add display region
        addCustomRegion(displayRegion.set("Display Region", [this](){
            drawDbap();
        }), [this](){
            drawDbap();
        });

        // Add listeners for parameter changes
        listeners.push(sourceX.newListener([this](vector<float> &){
            calculateDistances();
        }));
        
        listeners.push(sourceY.newListener([this](vector<float> &){
            calculateDistances();
        }));
        
        listeners.push(speakerX.newListener([this](vector<float> &){
            calculateDistances();
        }));
        
        listeners.push(speakerY.newListener([this](vector<float> &){
            calculateDistances();
        }));
        
        listeners.push(rolloff.newListener([this](float &){
            calculateDistances();
        }));
    }

private:
    ofParameter<vector<float>> sourceX;
    ofParameter<vector<float>> sourceY;
    ofParameter<vector<float>> speakerX;
    ofParameter<vector<float>> speakerY;
    ofParameter<vector<float>> distance;
    ofParameter<int> size;
    ofParameter<float> rolloff;
    customGuiRegion displayRegion;
    ofEventListeners listeners;

    void calculateDistances() {
        vector<float> distances;
        
        // Get current positions
        const auto& srcX = sourceX.get();
        const auto& srcY = sourceY.get();
        const auto& spkX = speakerX.get();
        const auto& spkY = speakerY.get();
        
        if(srcX.empty() || srcY.empty() || spkX.empty() || spkY.empty() ||
           srcX.size() != srcY.size() || spkX.size() != spkY.size()) {
            distance = distances;
            return;
        }

        // For each source
        for(size_t i = 0; i < srcX.size(); i++) {
            float sourceXPos = srcX[i];
            float sourceYPos = srcY[i];
            
            // Calculate distances to each speaker
            vector<float> sourceDistances;
            float sumSquaredAmplitudes = 0.0f;
            
            for(size_t j = 0; j < spkX.size(); j++) {
                float dx = (sourceXPos - spkX[j]);
                float dy = (sourceYPos - spkY[j]);
                float dist = sqrt(dx*dx + dy*dy);
                
                // Apply rolloff factor
                float amplitude = 1.0f / pow(std::max(dist, 0.000001f), rolloff);
                sourceDistances.push_back(amplitude);
                sumSquaredAmplitudes += amplitude * amplitude;
            }
            
            // Normalize amplitudes
            float normalizationFactor = sqrt(sumSquaredAmplitudes);
            if(normalizationFactor > 0) {
                for(float& amp : sourceDistances) {
                    amp /= normalizationFactor;
                }
            }
            
            // Add to final distances vector
            distances.insert(distances.end(), sourceDistances.begin(), sourceDistances.end());
        }
        
        distance = distances;
    }

    void drawDbap() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float displaySize = size.get();
        
        // Create invisible button for interaction area
        ImGui::InvisibleButton("DbapArea", ImVec2(displaySize, displaySize));
        
        // Draw background
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + displaySize, pos.y + displaySize),
            IM_COL32(0, 0, 0, 255)
        );
        
        // Draw grid
        const int gridLines = 10;
        for(int i = 0; i <= gridLines; i++) {
            float x = pos.x + (displaySize * i / gridLines);
            float y = pos.y + (displaySize * i / gridLines);
            
            // Vertical lines
            drawList->AddLine(
                ImVec2(x, pos.y),
                ImVec2(x, pos.y + displaySize),
                IM_COL32(40, 40, 40, 255)
            );
            
            // Horizontal lines
            drawList->AddLine(
                ImVec2(pos.x, y),
                ImVec2(pos.x + displaySize, y),
                IM_COL32(40, 40, 40, 255)
            );
        }

        // Draw speakers
        const float speakerSize = 10.0f;
        const auto& spkX = speakerX.get();
        const auto& spkY = speakerY.get();
        
        for(size_t i = 0; i < spkX.size() && i < spkY.size(); i++) {
            float x = pos.x + spkX[i] * displaySize;
            float y = pos.y + spkY[i] * displaySize;
            
            // Draw speaker as a square
            drawList->AddRectFilled(
                ImVec2(x - speakerSize/2, y - speakerSize/2),
                ImVec2(x + speakerSize/2, y + speakerSize/2),
                IM_COL32(200, 200, 200, 255)
            );
            
            // Draw speaker number
            char label[8];
            snprintf(label, sizeof(label), "%zu", i + 1);
            ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2(x - textSize.x/2, y - textSize.y/2),
                IM_COL32(0, 0, 0, 255),
                label
            );
        }
        
        // Draw sources
        const float sourceSize = 8.0f;
        const auto& srcX = sourceX.get();
        const auto& srcY = sourceY.get();
        
        for(size_t i = 0; i < srcX.size() && i < srcY.size(); i++) {
            float x = pos.x + srcX[i] * displaySize;
            float y = pos.y + srcY[i] * displaySize;
            
            // Draw source as a circle
            drawList->AddCircleFilled(
                ImVec2(x, y),
                sourceSize,
                IM_COL32(255, 100, 100, 255)
            );
            
            // Draw source number
            char label[8];
            snprintf(label, sizeof(label), "%zu", i + 1);
            ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2(x - textSize.x/2, y - textSize.y/2),
                IM_COL32(255, 255, 255, 255),
                label
            );
        }
    }
};

#endif /* dbap_h */
