#pragma once

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

class midiKeyboardEmulator : public ofxOceanodeNodeModel {
public:
    midiKeyboardEmulator()
    : ofxOceanodeNodeModel("MIDI Keyboard Emulator") {}

    void setup() override {
        color = ofColor(80, 150, 255);
        description = "Turns the computer keyboard into a polyphonic MIDI-style keyboard. "
                      "Use A-L (plus ; and ') for white keys and W/E/T/Y/U/O/P for black keys. "
                      "Z and X move the octave down and up. Pitch[] and Gate[] contain one voice "
                      "per simultaneously held note.";

        addParameter(octave.set("Octave", 4, -1, 8));
        addOutputParameter(pitchOutput.set("Pitch[]", {}, {0}, {127}));
        addOutputParameter(gateOutput.set("Gate[]", {}, {0}, {1}));

        octaveListener = octave.newListener([this](int&) {
            transposeHeldNotes();
        });

        keyPressedListener = ofEvents().keyPressed.newListener(
            this, &midiKeyboardEmulator::onKeyPressed);
        keyReleasedListener = ofEvents().keyReleased.newListener(
            this, &midiKeyboardEmulator::onKeyReleased);

        addCustomRegion(keyboardRegion.set("Keyboard", [this]() {
            drawKeyboard();
        }), [this]() {
            drawKeyboard();
        });
    }

private:
    struct KeyMapping {
        char key;
        int semitone;
    };

    struct HeldKey {
        char key;
        int midiNote;
    };

    // This is the same chromatic layout used by many DAW typing keyboards.
    static constexpr std::array<KeyMapping, 18> keyMappings {{
        {'a', 0}, {'w', 1}, {'s', 2}, {'e', 3}, {'d', 4}, {'f', 5},
        {'t', 6}, {'g', 7}, {'y', 8}, {'h', 9}, {'u', 10}, {'j', 11},
        {'k', 12}, {'o', 13}, {'l', 14}, {'p', 15}, {';', 16}, {'\'', 17}
    }};

    ofParameter<int> octave;
    ofParameter<std::vector<float>> pitchOutput;
    ofParameter<std::vector<float>> gateOutput;
    customGuiRegion keyboardRegion;

    ofEventListener keyPressedListener;
    ofEventListener keyReleasedListener;
    ofEventListener octaveListener;
    std::vector<HeldKey> heldKeys;

    static char normalizedKey(const ofKeyEventArgs& args) {
        int key = args.key;
        if(key >= 'A' && key <= 'Z') key = std::tolower(key);
        if(key >= 32 && key <= 126) return static_cast<char>(key);

        if(args.codepoint >= 32 && args.codepoint <= 126) {
            char codepoint = static_cast<char>(args.codepoint);
            return static_cast<char>(std::tolower(static_cast<unsigned char>(codepoint)));
        }
        return 0;
    }

    static int semitoneForKey(char key) {
        for(const auto& mapping : keyMappings) {
            if(mapping.key == key) return mapping.semitone;
        }
        return -1;
    }

    int baseMidiNote() const {
        // MIDI octave convention: C4 is note 60 and C-1 is note 0.
        return (octave.get() + 1) * 12;
    }

    bool isHeld(char key) const {
        return std::any_of(heldKeys.begin(), heldKeys.end(), [key](const HeldKey& held) {
            return held.key == key;
        });
    }

    bool noteIsHeld(int note) const {
        return std::any_of(heldKeys.begin(), heldKeys.end(), [note](const HeldKey& held) {
            return held.midiNote == note;
        });
    }

    void onKeyPressed(ofKeyEventArgs& args) {
        if(args.isRepeat) return;
        if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput) return;

        // Do not turn application shortcuts such as Cmd/Ctrl+S into notes.
        if(ofGetKeyPressed(OF_KEY_SUPER) || ofGetKeyPressed(OF_KEY_CONTROL) ||
           ofGetKeyPressed(OF_KEY_ALT)) {
            return;
        }

        const char key = normalizedKey(args);
        if(key == 'z') {
            octave.set(std::max(octave.getMin(), octave.get() - 1));
            return;
        }
        if(key == 'x') {
            octave.set(std::min(octave.getMax(), octave.get() + 1));
            return;
        }

        const int semitone = semitoneForKey(key);
        if(semitone < 0 || isHeld(key)) return;

        const int note = baseMidiNote() + semitone;
        if(note < 0 || note > 127) return;

        heldKeys.push_back({key, note});
        publishHeldNotes();
    }

    void onKeyReleased(ofKeyEventArgs& args) {
        const char key = normalizedKey(args);
        const auto it = std::find_if(heldKeys.begin(), heldKeys.end(), [key](const HeldKey& held) {
            return held.key == key;
        });
        if(it == heldKeys.end()) return;

        // Emit an explicit note-off before compacting the voice vectors. The final
        // stored vector size still exactly matches the number of held keys.
        std::vector<float> releaseGates(heldKeys.size(), 1.0f);
        releaseGates[static_cast<size_t>(std::distance(heldKeys.begin(), it))] = 0.0f;
        gateOutput.set(releaseGates);

        heldKeys.erase(it);
        publishHeldNotes();
    }

    void publishHeldNotes() {
        std::vector<float> pitches;
        pitches.reserve(heldKeys.size());
        for(const auto& held : heldKeys) pitches.push_back(static_cast<float>(held.midiNote));

        pitchOutput.set(pitches);
        gateOutput.set(std::vector<float>(heldKeys.size(), 1.0f));
    }

    void transposeHeldNotes() {
        if(heldKeys.empty()) return;

        // Release the old pitches before retriggering the same physical keys in
        // the newly selected octave.
        gateOutput.set(std::vector<float>(heldKeys.size(), 0.0f));
        for(auto& held : heldKeys) {
            held.midiNote = baseMidiNote() + semitoneForKey(held.key);
        }
        publishHeldNotes();
    }

    static bool isBlack(int semitone) {
        const int pitchClass = semitone % 12;
        return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
               pitchClass == 8 || pitchClass == 10;
    }

    static const char* labelForSemitone(int semitone) {
        for(const auto& mapping : keyMappings) {
            if(mapping.semitone == semitone) {
                static thread_local char label[2];
                label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(mapping.key)));
                label[1] = '\0';
                return label;
            }
        }
        return "";
    }

    void drawKeyboard() {
        constexpr int displayedSemitones = 18;
        constexpr float fallbackHeight = 72.0f;

        const float zoom = ofxOceanodeShared::getZoomLevel();
        const auto& context = ofxOceanodeShared::getCustomRegionRenderContext();
        const float fallbackWidth = static_cast<float>(
            ofxOceanodeShared::getNodeWidthText() + ofxOceanodeShared::getNodeWidthWidget());
        const float width = context.active ? std::max(1.0f, context.width) : fallbackWidth * zoom;
        const float height = context.active ? std::max(1.0f, context.height) : fallbackHeight * zoom;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##midiKeyboardEmulator", ImVec2(width, height));
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        int whiteCount = 0;
        for(int semitone = 0; semitone < displayedSemitones; ++semitone) {
            if(!isBlack(semitone)) ++whiteCount;
        }

        const float whiteWidth = width / static_cast<float>(whiteCount);
        const float blackWidth = whiteWidth * 0.62f;
        const float blackHeight = height * 0.62f;
        const int base = baseMidiNote();

        // White keys form the background.
        int whiteIndex = 0;
        for(int semitone = 0; semitone < displayedSemitones; ++semitone) {
            if(isBlack(semitone)) continue;

            const float x0 = origin.x + whiteIndex * whiteWidth;
            const float x1 = origin.x + (whiteIndex + 1) * whiteWidth;
            const bool active = noteIsHeld(base + semitone);
            drawList->AddRectFilled(
                ImVec2(x0, origin.y), ImVec2(x1, origin.y + height),
                active ? IM_COL32(70, 150, 255, 255) : IM_COL32(242, 242, 242, 255));
            drawList->AddRect(
                ImVec2(x0, origin.y), ImVec2(x1, origin.y + height),
                IM_COL32(70, 70, 70, 255));

            const char* label = labelForSemitone(semitone);
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2((x0 + x1 - textSize.x) * 0.5f, origin.y + height - textSize.y - 4.0f * zoom),
                active ? IM_COL32(255, 255, 255, 255) : IM_COL32(55, 55, 55, 255), label);
            ++whiteIndex;
        }

        // Black keys sit between the surrounding white keys.
        int whitesBefore = 0;
        for(int semitone = 0; semitone < displayedSemitones; ++semitone) {
            if(!isBlack(semitone)) {
                ++whitesBefore;
                continue;
            }

            const float centerX = origin.x + whitesBefore * whiteWidth;
            const float x0 = centerX - blackWidth * 0.5f;
            const float x1 = centerX + blackWidth * 0.5f;
            const bool active = noteIsHeld(base + semitone);
            drawList->AddRectFilled(
                ImVec2(x0, origin.y), ImVec2(x1, origin.y + blackHeight),
                active ? IM_COL32(70, 150, 255, 255) : IM_COL32(25, 25, 25, 255));
            drawList->AddRect(
                ImVec2(x0, origin.y), ImVec2(x1, origin.y + blackHeight),
                IM_COL32(8, 8, 8, 255));

            const char* label = labelForSemitone(semitone);
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2(centerX - textSize.x * 0.5f, origin.y + blackHeight - textSize.y - 4.0f * zoom),
                IM_COL32(245, 245, 245, 255), label);
        }
    }
};
