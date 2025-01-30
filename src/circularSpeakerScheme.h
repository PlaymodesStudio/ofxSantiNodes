#ifndef circularSpeakerScheme_h
#define circularSpeakerScheme_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

class circularSpeakerScheme : public ofxOceanodeNodeModel {
public:
    circularSpeakerScheme() : ofxOceanodeNodeModel("Circular Speaker Scheme") {}
    
    void setup() override {
        description = "Displays a circular arrangement of speaker boxes with volume indicators.";
        
        // Add parameters
        addParameter(numSpeakers.set("Num", 8, 1, 64));
        addParameter(rotation.set("Rot", 0, 0, 1));
        addParameter(volume.set("Volume", {1}, {0}, {1}));
        addParameter(size.set("Size", 240, 100, 500));
        addParameter(boxSize.set("Box Size", 0.15, 0.05, 0.3));
        addOutputParameter(speakerAngle.set("Angle", 0, 0, 360));
        
        // Add display region
        addCustomRegion(displayRegion.set("Display Region", [this](){
            drawScheme();
        }), [this](){
            drawScheme();
        });

        // Add listeners
        listeners.push(numSpeakers.newListener([this](int &){
            updateAngle();
        }));
    }

private:
    ofParameter<int> numSpeakers;
    ofParameter<float> rotation;
    ofParameter<vector<float>> volume;
    ofParameter<int> size;
    ofParameter<float> boxSize;
    ofParameter<float> speakerAngle;
    customGuiRegion displayRegion;
    ofEventListeners listeners;

    void updateAngle() {
        if(numSpeakers > 0) {
            speakerAngle = 360.0f / numSpeakers;
        }
    }

    void drawScheme() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float displaySize = size.get();
        float centerX = pos.x + displaySize/2;
        float centerY = pos.y + displaySize/2;
        
        // Create invisible button for interaction area
        ImGui::InvisibleButton("SchemeArea", ImVec2(displaySize, displaySize));
        
        // Draw background
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + displaySize, pos.y + displaySize),
            IM_COL32(0, 0, 0, 255)
        );
        
        // Draw background cross
        drawList->AddLine(
            ImVec2(centerX, pos.y),
            ImVec2(centerX, pos.y + displaySize),
            IM_COL32(40, 40, 40, 255)
        );
        drawList->AddLine(
            ImVec2(pos.x, centerY),
            ImVec2(pos.x + displaySize, centerY),
            IM_COL32(40, 40, 40, 255)
        );
        
        // Draw center point
        drawList->AddCircleFilled(
            ImVec2(centerX, centerY),
            2,
            IM_COL32(100, 100, 100, 255)
        );

        if(numSpeakers <= 0) return;

        // Calculate speaker box size and radius
        float actualBoxSize = displaySize * boxSize;  // Size of each speaker box
        float radius = (displaySize - actualBoxSize) * 0.4f;  // Radius of speaker arrangement
        
        // Get current volume array and ensure it matches speaker count
        vector<float> currentVolume = volume.get();
        while(currentVolume.size() < numSpeakers) {
            currentVolume.push_back(0.0f);
        }

        // Draw speakers
        for(int i = 0; i < numSpeakers; i++) {
            // Calculate position
            float angle = ((float)i / numSpeakers) * 2 * M_PI - (M_PI / 2) + (rotation * 2 * M_PI);
            float x = centerX + radius * cos(angle);
            float y = centerY + radius * sin(angle);
            
            // Calculate speaker box corners
            ImVec2 boxMin(x - actualBoxSize/2, y - actualBoxSize/2);
            ImVec2 boxMax(x + actualBoxSize/2, y + actualBoxSize/2);
            
            // Get volume for this speaker (safely)
            float speakerVolume = (i < currentVolume.size()) ? currentVolume[i] : 0.0f;
            speakerVolume = std::max(0.0f, std::min(1.0f, speakerVolume));
            
            // Calculate color based on volume
            int brightness = (int)(speakerVolume * 255);
            
            // Draw volume indicator (filled rectangle)
            drawList->AddRectFilled(
                boxMin,
                boxMax,
                IM_COL32(brightness, brightness, brightness, 255),
                2.0f  // corner radius
            );
            
            // Draw speaker number
            char label[8];
            snprintf(label, sizeof(label), "%d", i + 1);
            ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2(x - textSize.x/2, y - textSize.y/2),
                IM_COL32(255 - brightness, 255 - brightness, 255 - brightness, 255),
                label
            );
        }
    }
};

#endif /* circularSpeakerScheme_h */
