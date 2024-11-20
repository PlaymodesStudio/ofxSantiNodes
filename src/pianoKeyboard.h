#ifndef pianoKeyboard_h
#define pianoKeyboard_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>

class pianoKeyboard : public ofxOceanodeNodeModel {
public:
    pianoKeyboard() : ofxOceanodeNodeModel("Piano Keyboard") {}
    
    void setup() {
        description = "Displays a piano keyboard that highlights keys based on pitch and gate inputs";
        
        addParameter(pitch.set("Pitch[]", vector<float>(1, 60), vector<float>(1, 0), vector<float>(1, 127)));
        addParameter(gate.set("Gate[]", vector<float>(1, 0), vector<float>(1, 0), vector<float>(1, 1)));
        addParameter(width.set("Width", 400, 100, 1000));
        addParameter(height.set("Height", 100, 50, 300));
        addParameter(loNote.set("Lo Note", 48, 0, 127)); // C2
        addParameter(hiNote.set("Hi Note", 72, 0, 127)); // C4
        
        listeners.push(width.newListener([this](int &){
            updateKeyboardGeometry();
        }));
        
        listeners.push(height.newListener([this](int &){
            updateKeyboardGeometry();
        }));
        
        listeners.push(loNote.newListener([this](int &){
            updateKeyboardGeometry();
        }));
        
        listeners.push(hiNote.newListener([this](int &){
            updateKeyboardGeometry();
        }));
        
        addCustomRegion(customKeyboardRegion, [this]() {
            drawKeyboard();
        });
        
        updateKeyboardGeometry();
    }

private:
    ofParameter<vector<float>> pitch;
    ofParameter<vector<float>> gate;
    ofParameter<int> width;
    ofParameter<int> height;
    ofParameter<int> loNote;
    ofParameter<int> hiNote;
    ofEventListeners listeners;
    customGuiRegion customKeyboardRegion;
    
    struct KeyGeometry {
        bool isBlack;
        float x;
        float w;
    };
    
    vector<KeyGeometry> keyGeometry;
    
    void updateKeyboardGeometry() {
        keyGeometry.clear();
        
        int startNote = std::max(0, std::min(127, loNote.get()));
        int endNote = std::max(0, std::min(127, hiNote.get()));
        if(endNote < startNote) {
            int temp = endNote;
            endNote = startNote;
            startNote = temp;
        }
        
        // Count white keys for width calculation
        int whiteKeyCount = 0;
        for(int note = startNote; note <= endNote; note++) {
            int noteInOctave = note % 12;
            if(noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 4 ||
               noteInOctave == 5 || noteInOctave == 7 || noteInOctave == 9 ||
               noteInOctave == 11) {
                whiteKeyCount++;
            }
        }
        
        float whiteKeyWidth = width / (float)whiteKeyCount;
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        
        float currentX = 0;
        int whiteKeyIndex = 0;
        
        for(int note = startNote; note <= endNote; note++) {
            int noteInOctave = note % 12;
            KeyGeometry key;
            
            switch(noteInOctave) {
                case 0: // C
                case 2: // D
                case 4: // E
                case 5: // F
                case 7: // G
                case 9: // A
                case 11: // B
                    key.isBlack = false;
                    key.x = currentX;
                    key.w = whiteKeyWidth;
                    currentX += whiteKeyWidth;
                    whiteKeyIndex++;
                    break;
                    
                case 1: // C#
                case 3: // D#
                case 6: // F#
                case 8: // G#
                case 10: // A#
                    key.isBlack = true;
                    key.x = currentX - (blackKeyWidth / 2);
                    key.w = blackKeyWidth;
                    break;
            }
            
            keyGeometry.push_back(key);
        }
    }
    
    void drawKeyboard() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Create invisible button for interaction area
        ImGui::InvisibleButton("KeyboardArea", ImVec2(width, height));
        
        // First draw all white keys
        for(int i = 0; i < keyGeometry.size(); i++) {
            if(!keyGeometry[i].isBlack) {
                int note = loNote + i;
                ImVec2 keyPos(pos.x + keyGeometry[i].x, pos.y);
                ImVec2 keyPosEnd(keyPos.x + keyGeometry[i].w, pos.y + height);
                
                // Draw white key background
                drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(255, 255, 255, 255));
                drawList->AddRect(keyPos, keyPosEnd, IM_COL32(100, 100, 100, 255));
                
                // Check if this note is being played and draw red highlight with alpha
                float keyGate = 0.0f;
                for(size_t j = 0; j < pitch->size() && j < gate->size(); j++) {
                    int pitchValue = static_cast<int>(pitch->at(j) + 0.5f); // Round to nearest integer
                    if(pitchValue == note) {
                        keyGate = std::max(keyGate, gate->at(j));
                    }
                }
                
                if(keyGate > 0.0f) {
                    // Alpha ranges from 255 (transparent) to 0 (opaque) based on gate
                    uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - keyGate));
                    ImU32 highlightColor = IM_COL32(255, 0, 0, 255 - alpha);
                    drawList->AddRectFilled(keyPos, keyPosEnd, highlightColor);
                }
            }
        }
        
        // Then draw all black keys on top
        float blackKeyHeight = height * 0.6f;
        for(int i = 0; i < keyGeometry.size(); i++) {
            if(keyGeometry[i].isBlack) {
                int note = loNote + i;
                ImVec2 keyPos(pos.x + keyGeometry[i].x, pos.y);
                ImVec2 keyPosEnd(keyPos.x + keyGeometry[i].w, pos.y + blackKeyHeight);
                
                // Draw black key background
                drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(0, 0, 0, 255));
                drawList->AddRect(keyPos, keyPosEnd, IM_COL32(100, 100, 100, 255));
                
                // Check if this note is being played and draw red highlight with alpha
                float keyGate = 0.0f;
                for(size_t j = 0; j < pitch->size() && j < gate->size(); j++) {
                    int pitchValue = static_cast<int>(pitch->at(j) + 0.5f); // Round to nearest integer
                    if(pitchValue == note) {
                        keyGate = std::max(keyGate, gate->at(j));
                    }
                }
                
                if(keyGate > 0.0f) {
                    // Alpha ranges from 255 (transparent) to 0 (opaque) based on gate
                    uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - keyGate));
                    ImU32 highlightColor = IM_COL32(255, 0, 0, 255 - alpha);
                    drawList->AddRectFilled(keyPos, keyPosEnd, highlightColor);
                }
            }
        }
    }
};

#endif /* pianoKeyboard_h */
