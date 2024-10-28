#ifndef pianoRoll_h
#define pianoRoll_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <array>
#include <algorithm>

struct Note {
    int pitch;
    float start;
    float length;
    float velocity;
    bool selected;
    
    ofJson toJson() const {
        ofJson j;
        j["pitch"] = pitch;
        j["start"] = start;
        j["length"] = length;
        j["velocity"] = velocity;
        return j;
    }
    
    // Custom JSON deserialization
    static Note fromJson(const ofJson& j) {
        Note note;
        note.pitch = j["pitch"];
        note.start = j["start"];
        note.length = j["length"];
        note.velocity = j["velocity"];
        note.selected = false;
        return note;
    }
};

struct TimeSignature {
    int numerator;
    int denominator;
};

class pianoRoll : public ofxOceanodeNodeModel {
public:
    static const int NUM_SLOTS = 10;
    
    pianoRoll() : ofxOceanodeNodeModel("Piano Roll"), uniqueId(generateUniqueId()) {
        description = "A piano roll interface node that outputs note velocities based on user input.";
        
    }
    
    void setup() override {
        addParameter(octaves.set("Octaves", 2, 1, 8));
        addParameter(phasor.set("Phasor", 0.0f, 0.0f, 1.0f));
        addParameter(showWindow.set("Show", true));
        addParameter(quantizationResolution.set("Quantization", 16, 1, 64));
        addParameter(numBars.set("Bars", 4, 1, 16));
        addParameter(timeSignatureNum.set("Time Sig Num", 4, 1, 16));
        addParameter(timeSignatureDenom.set("Time Sig Denom", 4, 1, 16));
        addParameter(noteHeight.set("Note Height", 20, 5, 50));
        //addParameter(slot.set("Slot", 0, 0, NUM_SLOTS - 1));
        
        addOutputParameter(noteOutput.set("Notes Out", std::vector<float>(24, 0.0f), std::vector<float>(24, 0.0f), std::vector<float>(24, 1.0f)));
        
        listeners.push(octaves.newListener([this](int &o) { updateNoteVector(); }));
        listeners.push(quantizationResolution.newListener([this](int &q) { updateGrid(); }));
        listeners.push(numBars.newListener([this](int &b) { updateGrid(); }));
        listeners.push(timeSignatureNum.newListener([this](int &n) { updateGrid(); }));
        listeners.push(timeSignatureDenom.newListener([this](int &d) { updateGrid(); }));
        listeners.push(noteHeight.newListener([this](int &h) { /* Redraw only */ }));
        //listeners.push(slot.newListener([this](int &s) { switchSlot(s); }));
        
        /*
        for (int i = 0; i < NUM_SLOTS; ++i) {
                    allSlotData[i] = std::vector<Note>();
                }
        */
        
        updateNoteVector();
        updateGrid();
        //initializeAllSlotData();
        
    }
    
    void update(ofEventArgs &args) override {
        updateNoteOutput();
    }
    
    void draw(ofEventArgs &args) override {
        if (showWindow) {
            ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
            std::string windowName = "Piano Roll ##" + std::to_string(uniqueId);
            if (ImGui::Begin(windowName.c_str(), (bool *)&showWindow.get())) {
                ImGui::BeginChild("PianoRollChild", ImVec2(0, 0), true, ImGuiWindowFlags_NoMove);
                drawPianoRoll();
                ImGui::EndChild();
            }
            ImGui::End();
        }
    }
    
    /*
    void presetSave(ofJson &json) override {
           saveCurrentSlotData();
           for (int i = 0; i < NUM_SLOTS; i++) {
               ofJson slotJson = ofJson::array();
               for (const auto& note : allSlotData[i]) {
                   slotJson.push_back(note.toJson());
               }
               json["SlotData"][ofToString(i)] = slotJson;
           }
           json["CurrentSlot"] = slot.get();
           
           ofLogNotice("pianoRoll") << "Saving preset. Current slot: " << slot;
           for (int i = 0; i < NUM_SLOTS; i++) {
               ofLogNotice("pianoRoll") << "Slot " << i << " has " << allSlotData[i].size() << " notes";
           }
       }
    */
    
    
    void presetSave(ofJson &json) override {
            ofJson notesJson = ofJson::array();
            for (const auto& note : notes) {
                notesJson.push_back(note.toJson());
            }
            json["Notes"] = notesJson;
            
            ofLogNotice("pianoRoll") << "Saving preset with " << notes.size() << " notes";
        }
    
    /*
    void presetRecallAfterSettingParameters(ofJson &json) override {
            for (int i = 0; i < NUM_SLOTS; i++) {
                allSlotData[i].clear();
            }

            if (json.count("SlotData") == 1) {
                for (int i = 0; i < NUM_SLOTS; i++) {
                    string slotKey = ofToString(i);
                    if (json["SlotData"].contains(slotKey)) {
                        for (const auto& noteJson : json["SlotData"][slotKey]) {
                            allSlotData[i].push_back(Note::fromJson(noteJson));
                        }
                    }
                }
            }
            
            if (json.contains("CurrentSlot")) {
                int loadedSlot = json["CurrentSlot"].get<int>();
                switchSlot(loadedSlot);
            } else {
                switchSlot(0);
            }

            updateGrid();
            
            ofLogNotice("pianoRoll") << "Loaded preset. Current slot: " << slot;
            for (int i = 0; i < NUM_SLOTS; i++) {
                ofLogNotice("pianoRoll") << "Slot " << i << " has " << allSlotData[i].size() << " notes";
            }
        }
     */
    void presetRecallAfterSettingParameters(ofJson &json) override {
            notes.clear();
            if (json.contains("Notes")) {
                for (const auto& noteJson : json["Notes"]) {
                    notes.push_back(Note::fromJson(noteJson));
                }
            }
            
            updateGrid();
            updateNoteOutput();
            
            ofLogNotice("pianoRoll") << "Loaded preset with " << notes.size() << " notes";
        }

    void presetHasLoaded() override {
            updateNoteVector();
            updateGrid();
            updateNoteOutput();
            
            //ofLogNotice("pianoRoll") << "Preset has loaded. Current slot: " << slot;
            ofLogNotice("pianoRoll") << "Number of notes in current slot: " << notes.size();
        }
    
private:
    ofParameter<int> octaves;
    ofParameter<float> phasor;
    ofParameter<bool> showWindow;
    ofParameter<int> quantizationResolution;
    ofParameter<int> numBars;
    ofParameter<int> timeSignatureNum;
    ofParameter<int> timeSignatureDenom;
    ofParameter<int> noteHeight;
    //ofParameter<int> slot;
    
    int previousTotalSteps;
    float previousGridWidth;
    bool shiftPressed = false;
    bool ctrlPressed = false;
    bool isSelecting = false;
    ImVec2 selectionStart;
    ImVec2 selectionEnd;
    std::vector<Note*> selectedNotes;
    
    ofParameter<std::vector<float>> noteOutput;
    std::vector<Note> notes;
    //std::array<std::vector<Note>, NUM_SLOTS> allSlotData;
    int currentSlot = 0;
    ofEventListeners listeners;
    
    static const int NOTES_PER_OCTAVE = 12;
    const float TIME_WIDTH = 30.0f;
    const float NOTE_PADDING = 1.0f;
    const float RESIZE_AREA = 10.0f;
    
    bool isDragging = false;
    bool isResizing = false;
    bool isAdjustingVelocity = false;
    Note* draggingNote = nullptr;
    float dragStartX = 0.0f;
    float dragStartY = 0.0f;
    
    int totalSteps;
    float stepWidth;
    float gridWidth;
    
    const unsigned int uniqueId;
    
    static unsigned int generateUniqueId() {
        static unsigned int id = 0;
        return ++id;
    }
    
    void updateNoteVector() {
        int totalNotes = octaves * NOTES_PER_OCTAVE;
        noteOutput.setMin(std::vector<float>(totalNotes, 0.0f));
        noteOutput.setMax(std::vector<float>(totalNotes, 1.0f));
    }
    
    /*
    void switchSlot(int newSlot) {
            if (newSlot < 0 || newSlot >= NUM_SLOTS) return;
            
            saveCurrentSlotData();
            slot = newSlot;
            notes = allSlotData[slot];
            selectedNotes.clear();
            
            updateGrid();
            updateNoteOutput();
            
            ofLogNotice("pianoRoll") << "Switched to slot " << slot << ". Number of notes: " << notes.size();
        }

        void saveCurrentSlotData() {
            allSlotData[slot] = notes;
            ofLogNotice("pianoRoll") << "Saved " << notes.size() << " notes to slot " << slot;
        }
    */
    void updateNoteOutput() {
        int totalNotes = octaves * NOTES_PER_OCTAVE;
        std::vector<float> output(totalNotes, 0.0f);
        
        for (const auto& note : notes) {
            if (phasor >= note.start && phasor < note.start + note.length) {
                output[note.pitch] = note.velocity;  // Use the note's velocity
            }
        }
        
        noteOutput = output;
    }
    
    
    void updateGrid() {
        previousTotalSteps = totalSteps;
        totalSteps = quantizationResolution * numBars;
        stepWidth = gridWidth / totalSteps;
    }
    
    void updateGridAndAdjustNotes() {
        float oldTotalTime = previousTotalSteps * (60.0f / (timeSignatureNum * quantizationResolution));
        updateGrid();
        float newTotalTime = totalSteps * (60.0f / (timeSignatureNum * quantizationResolution));
        float timeRatio = newTotalTime / oldTotalTime;
        
        for (auto& note : notes) {
            note.start *= timeRatio;
            note.length *= timeRatio;
            
            if (note.start + note.length > 1.0f) {
                if (note.start < 1.0f) {
                    note.length = 1.0f - note.start;
                } else {
                    note.start = 1.0f - note.length;
                }
            }
        }
    }
    
    float quantizeTime(float time) {
        int step = std::round(time * totalSteps);
        return static_cast<float>(step) / totalSteps;
    }
    
    
    
    void drawPianoRoll() {
        
        shiftPressed = ImGui::GetIO().KeyShift;
        ctrlPressed = ImGui::GetIO().KeyCtrl;
        
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        float totalHeight = (octaves * NOTES_PER_OCTAVE * noteHeight);
        gridWidth = canvas_size.x - TIME_WIDTH;
        updateGrid();
        
        ImVec2 mouse_pos = ImGui::GetMousePos();
        int highlightedNote = getHighlightedNote(mouse_pos, canvas_pos, totalHeight);
        
        // Draw piano keys and horizontal lines
        for (int i = 0; i <= octaves * NOTES_PER_OCTAVE; ++i) {
                    bool isBlackKey = isNoteBlack(i);
                    ImU32 keyColor = isBlackKey ? IM_COL32(40, 40, 40, 255) : IM_COL32(220, 220, 220, 255);
                    float y = canvas_pos.y + totalHeight - (i + 1) * noteHeight;
                    
                    // Draw the base key color
                    draw_list->AddRectFilled(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + TIME_WIDTH, y + noteHeight), keyColor);
                    
                    // Draw the highlight if this is the highlighted note
                    if (i == highlightedNote) {
                        ImU32 highlightColor = IM_COL32(255, 255, 0, 25); // Yellow with some transparency
                        draw_list->AddRectFilled(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y + noteHeight), highlightColor);
                    }
                    
                    // Draw horizontal grid lines
                    draw_list->AddLine(ImVec2(canvas_pos.x + TIME_WIDTH, y), ImVec2(canvas_pos.x + canvas_size.x, y), IM_COL32(60, 60, 60, 255));
                    
                    // Draw black key outlines
                    if (isBlackKey) {
                        draw_list->AddRect(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + TIME_WIDTH, y + noteHeight), IM_COL32(0, 0, 0, 255));
                    }
                }
        
        // Draw vertical grid lines
        for (int i = 0; i <= totalSteps; ++i) {
            float x = canvas_pos.x + TIME_WIDTH + i * stepWidth;
            ImU32 lineColor = (i % quantizationResolution == 0) ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255);
            draw_list->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + totalHeight), lineColor);
        }
        
        // Draw selection rectangle
        if (isSelecting) {
            ImU32 selectionColor = IM_COL32(255, 255, 255, 100);
            draw_list->AddRect(selectionStart, selectionEnd, selectionColor);
        }
        
        
        // Draw notes with opacity based on velocity and highlight selected notes
        for (auto& note : notes) {
            float y = canvas_pos.y + totalHeight - (note.pitch + 1) * noteHeight;
            float x = canvas_pos.x + TIME_WIDTH + note.start * gridWidth;
            float width = note.length * gridWidth;
            ImU32 noteColor = note.selected ? IM_COL32(255, 0, 0, 255) : IM_COL32(100, 150, 250, (int)(note.velocity * 255));
            draw_list->AddRectFilled(
                                     ImVec2(x, y),
                                     ImVec2(x + width, y + noteHeight - NOTE_PADDING),
                                     noteColor
                                     );
            
            // If this note is being adjusted for velocity, display the value
            if (isAdjustingVelocity && draggingNote == &note) {
                char velocityText[16];
                snprintf(velocityText, sizeof(velocityText), "%.2f", note.velocity);
                ImVec2 textSize = ImGui::CalcTextSize(velocityText);
                ImVec2 textPos = ImVec2(x + width / 2 - textSize.x / 2, y - textSize.y - 2);
                draw_list->AddRectFilled(
                                         ImVec2(textPos.x - 2, textPos.y - 2),
                                         ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
                                         IM_COL32(0, 0, 0, 180)
                                         );
                draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), velocityText);
            }
        }
        
        
        // Handle mouse input for note creation, movement, selection, and velocity adjustment
        //ImVec2 mouse_pos = ImGui::GetMousePos();
        float mouseX = mouse_pos.x - canvas_pos.x - TIME_WIDTH;
        float mouseY = mouse_pos.y - canvas_pos.y;
        
        if (mouseX >= 0 && mouseX < gridWidth && mouseY >= 0 && mouseY < totalHeight) {
            int pitch = quantizePitch(mouseY, totalHeight);
            float time = quantizeTime(mouseX / gridWidth);
            
            // Right-click note deletion
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                auto it = std::remove_if(notes.begin(), notes.end(), [&](const Note& note) {
                    return note.pitch == pitch && time >= note.start && time < note.start + note.length;
                });
                if (it != notes.end()) {
                    // Remove the note from selectedNotes if it was selected
                    selectedNotes.erase(std::remove(selectedNotes.begin(), selectedNotes.end(), &(*it)), selectedNotes.end());
                    notes.erase(it, notes.end());
                }
            }
            
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                isDragging = true;
                isAdjustingVelocity = shiftPressed;
                draggingNote = nullptr;
                isResizing = false;
                dragStartX = time;
                dragStartY = mouseY;
                
                // Check if clicking on existing note or its resize area
                for (auto& note : notes) {
                    if (note.pitch == pitch && time >= note.start && time <= note.start + note.length) {
                        draggingNote = &note;
                        if (!isAdjustingVelocity) {
                            dragStartX = note.start;
                            // Check if near the right edge for resizing
                            if (time > note.start + note.length - RESIZE_AREA / gridWidth) {
                                isResizing = true;
                            }
                        }
                        break;
                    }
                }
                
                // Only create a new note if not clicking on an existing note or its resize area
                if (!draggingNote && !isAdjustingVelocity) {
                    notes.push_back({pitch, time, 1.0f / quantizationResolution, 0.7f, false}); // Default velocity 0.7, not selected
                    draggingNote = &notes.back();
                    isResizing = true;
                }
                
                // Handle selection
                if (ctrlPressed) {
                    if (draggingNote && !isResizing) {
                        draggingNote->selected = !draggingNote->selected;
                        auto it = std::find(selectedNotes.begin(), selectedNotes.end(), draggingNote);
                        if (it == selectedNotes.end()) {
                            selectedNotes.push_back(draggingNote);
                        } else {
                            selectedNotes.erase(it);
                        }
                    } else if (!draggingNote) {
                        // Start rectangle selection
                        isSelecting = true;
                        selectionStart = mouse_pos;
                        selectionEnd = mouse_pos;
                    }
                } else if (!isResizing && (!draggingNote || !draggingNote->selected)) {
                    // Deselect all notes if not resizing and not clicking on a selected note and not using Ctrl
                    for (auto& note : notes) {
                        note.selected = false;
                    }
                    selectedNotes.clear();
                    if (draggingNote) {
                        draggingNote->selected = true;
                        selectedNotes.push_back(draggingNote);
                    }
                }
            }
            else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isDragging) {
                if (isSelecting) {
                    // Update selection rectangle
                    selectionEnd = mouse_pos;
                } else if (isAdjustingVelocity) {
                    // Adjust velocity for all selected notes
                    float velocityChange = (dragStartY - mouseY) / (noteHeight * octaves * NOTES_PER_OCTAVE);
                    for (auto* note : selectedNotes) {
                        note->velocity = std::clamp(note->velocity + velocityChange, 0.0f, 1.0f);
                    }
                } else if (isResizing && draggingNote) {
                    // Resize note
                    float newLength = time - draggingNote->start;
                    if (newLength > 0) {
                        draggingNote->length = newLength;
                    }
                } else if (draggingNote) {
                    // Move all selected notes, maintaining relative positions
                    float timeDiff = time - dragStartX;
                    int pitchDiff = pitch - draggingNote->pitch;
                    
                    for (auto* note : selectedNotes) {
                        float newStart = note->start + timeDiff;
                        if (newStart >= 0 && newStart + note->length <= 1) {
                            note->start = newStart;
                            note->pitch = std::clamp(note->pitch + pitchDiff, 0, octaves * NOTES_PER_OCTAVE - 1);
                        }
                    }
                    dragStartX = time;
                    draggingNote->pitch = pitch;
                }
            }
            
            else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (isSelecting) {
                    // Finalize selection
                    isSelecting = false;
                    ImRect selectionRect(selectionStart, selectionEnd);
                    for (auto& note : notes) {
                        float noteStartX = canvas_pos.x + TIME_WIDTH + note.start * gridWidth;
                        float noteEndX = noteStartX + note.length * gridWidth;
                        float noteY = canvas_pos.y + totalHeight - (note.pitch + 1) * noteHeight;
                        ImRect noteRect(ImVec2(noteStartX, noteY), ImVec2(noteEndX, noteY + noteHeight));
                        if (selectionRect.Overlaps(noteRect)) {
                            note.selected = true;
                            if (std::find(selectedNotes.begin(), selectedNotes.end(), &note) == selectedNotes.end()) {
                                selectedNotes.push_back(&note);
                            }
                        }
                    }
                }
                isDragging = false;
                isAdjustingVelocity = false;
                draggingNote = nullptr;
                isResizing = false;
            }
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // Deselect all notes when clicking outside the piano roll area
            for (auto& note : notes) {
                note.selected = false;
            }
            selectedNotes.clear();
        }
        
        // Draw playhead
        float playheadX = canvas_pos.x + TIME_WIDTH + phasor * gridWidth;
        draw_list->AddLine(ImVec2(playheadX, canvas_pos.y), ImVec2(playheadX, canvas_pos.y + totalHeight), IM_COL32(255, 0, 0, 255), 2.0f);
    }
    
    bool isNoteBlack(int noteIndex) {
            int octavePosition = noteIndex % NOTES_PER_OCTAVE;
            return (octavePosition == 1 || octavePosition == 3 || octavePosition == 6 || octavePosition == 8 || octavePosition == 10);
        }

        int getHighlightedNote(const ImVec2& mouse_pos, const ImVec2& canvas_pos, float totalHeight) {
            if (mouse_pos.y < canvas_pos.y || mouse_pos.y > canvas_pos.y + totalHeight) {
                return -1; // Mouse is outside the vertical range of the piano roll
            }

            float relativeY = mouse_pos.y - canvas_pos.y;
            int noteIndex = (totalHeight - relativeY) / noteHeight;
            return std::clamp(noteIndex, 0, octaves * NOTES_PER_OCTAVE - 1);
        }
    
    int quantizePitch(float y, float totalHeight) {
        int pitch = (totalHeight - y) / noteHeight;
        return std::clamp(pitch, 0, octaves * NOTES_PER_OCTAVE - 1);
    }
    
    /*
    void initializeAllSlotData() {
        for (auto& slotNotes : allSlotData) {
            slotNotes.clear();
        }
    }
     */
    
    
};

#endif /* pianoRoll_h */
