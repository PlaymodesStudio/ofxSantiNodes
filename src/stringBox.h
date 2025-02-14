#ifndef stringBox_h
#define stringBox_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"

class stringBox : public ofxOceanodeNodeModel {
public:
    stringBox() : ofxOceanodeNodeModel("String Box") {}
    
    void setup() override {
        description = "Displays text in a resizable box with adjustable font size.";
        
        // Add parameter for input text
        addParameter(input.set("Input", ""));
        
        // Add parameters for box dimensions
        addParameter(width.set("Width", 300, 100, 1000));
        addParameter(height.set("Height", 200, 50, 1000));
        addParameter(fontSize.set("Font Size", 14, 8, 72));
        
        // Create custom region for text display
        addCustomRegion(textRegion.set("Text Region", [this](){
            drawTextBox();
        }), [this](){
            drawTextBox();
        });
    }
    
private:
    void drawTextBox() {
        // Set font size for this widget
        float fontScale = fontSize/14.0f; // 14 is ImGui's default font size
        ImGui::SetWindowFontScale(fontScale);
        
        // Create child window for scrolling
        ImGui::BeginChild("ScrollRegion", ImVec2(width, height), true,
                         ImGuiWindowFlags_HorizontalScrollbar);
        
        // Display the text
        ImGui::TextWrapped("%s", input.get().c_str());
        
        ImGui::EndChild();
        
        // Reset font scale to default
        ImGui::SetWindowFontScale(1.0f);
    }
    
    ofParameter<string> input;
    ofParameter<float> width;
    ofParameter<float> height;
    ofParameter<float> fontSize;
    customGuiRegion textRegion;
};

#endif /* stringBox_h */
