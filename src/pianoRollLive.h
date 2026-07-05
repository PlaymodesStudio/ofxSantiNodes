#ifndef pianoRollLive_h
#define pianoRollLive_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class pianoRollLive : public ofxOceanodeNodeModel {
public:
    pianoRollLive() : ofxOceanodeNodeModel("Piano Roll Live") {
        description =
            "Displays incoming pitch, gate, and velocity vectors as a scrolling live piano roll.\n"
            "Velocity is shown as dimmer-to-brighter notes in a separate dockable window.";
    }

    void setup() override {
        addParameter(pitchInput.set("Pitch", std::vector<float>{60.0f}, std::vector<float>{0.0f}, std::vector<float>{127.0f}));
        addParameter(gateInput.set("Gate", std::vector<float>{0.0f}, std::vector<float>{0.0f}, std::vector<float>{1.0f}));
        addParameter(velocityInput.set("Velocity", std::vector<float>{1.0f}, std::vector<float>{0.0f}, std::vector<float>{1.0f}));
        addParameter(highlightScale.set("HighlightScale", std::vector<int>{}, std::vector<int>{0}, std::vector<int>{11}));
        addParameter(showWindow.set("Show", false));

        addInspectorParameter(timeWindowMs.set("Window Ms", 8000, 250, 60000));
        addInspectorParameter(minPitch.set("Min Pitch", 36, 0, 127));
        addInspectorParameter(maxPitch.set("Max Pitch", 84, 0, 127));
        addInspectorParameter(showStrips.set("Show Strips", true));
        addInspectorParameter(reverseScroll.set("Reverse Scroll", false));
        addInspectorParameter(widgetWidth.set("Widget Width", 320.0f, 120.0f, 1600.0f));
        addInspectorParameter(widgetHeight.set("Widget Height", 140.0f, 60.0f, 800.0f));
        addInspectorParameter(windowWidth.set("Window Width", 980.0f, 320.0f, 2400.0f));
        addInspectorParameter(windowHeight.set("Window Height", 520.0f, 180.0f, 1800.0f));

        auto customRegion = addCustomRegion(
            ofParameter<std::function<void()>>().set("Piano Roll Live", [this](){ drawWidget(); }),
            ofParameter<std::function<void()>>().set("Piano Roll Live", [this](){ drawWidget(); })
        );
        customRegion->setFlags(customRegion->getFlags() | ofxOceanodeParameterFlags_NoGuiWidget);
    }

    void update(ofEventArgs &) override {
        const uint64_t nowMs = ofGetElapsedTimeMillis();
        const size_t voiceCount = std::max({pitchInput.get().size(), gateInput.get().size(), velocityInput.get().size(), activeVoices.size()});
        activeVoices.resize(voiceCount);

        for(size_t i = 0; i < voiceCount; i++) {
            const bool gateOn = getVectorValue(gateInput.get(), i, 0.0f) > 0.5f;
            const int pitch = ofClamp((int)std::round(getVectorValue(pitchInput.get(), i, 60.0f)), 0, 127);
            const float velocity = ofClamp(getVectorValue(velocityInput.get(), i, 1.0f), 0.0f, 1.0f);
            auto &voice = activeVoices[i];

            if(gateOn) {
                if(!voice.active) {
                    startVoiceNote(voice, (int)i, pitch, velocity, nowMs);
                } else if(voice.pitch != pitch) {
                    finishVoiceNote(voice, nowMs);
                    startVoiceNote(voice, (int)i, pitch, velocity, nowMs);
                } else {
                    voice.velocity = velocity;
                    if(isValidNoteIndex(voice.noteIndex)) {
                        notes[voice.noteIndex].endMs = nowMs;
                        notes[voice.noteIndex].velocity = velocity;
                    }
                }
            } else if(voice.active) {
                finishVoiceNote(voice, nowMs);
            }
        }

        pruneOldNotes(nowMs);
    }

    void draw(ofEventArgs &) override {
        if(!showWindow.get()) return;

        ImGui::SetNextWindowSize(ImVec2(windowWidth.get(), windowHeight.get()), ImGuiCond_FirstUseEver);
        std::string title =
            (canvasID == "Canvas" ? "" : canvasID + "/") +
            "Piano Roll Live " + ofToString(getNumIdentifier());

        bool open = showWindow.get();
        if(ImGui::Begin(title.c_str(), &open)) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            drawPianoRoll(std::max(avail.x, 220.0f), std::max(avail.y, 120.0f));
        }
        ImGui::End();
        if(!open) showWindow = false;
    }

private:
    struct LiveNote {
        uint64_t startMs = 0;
        uint64_t endMs = 0;
        int pitch = 60;
        float velocity = 1.0f;
        int voice = 0;
        bool active = false;
    };

    struct ActiveVoiceState {
        bool active = false;
        int pitch = 60;
        float velocity = 1.0f;
        int noteIndex = -1;
    };

    ofParameter<std::vector<float>> pitchInput;
    ofParameter<std::vector<float>> gateInput;
    ofParameter<std::vector<float>> velocityInput;
    ofParameter<std::vector<int>> highlightScale;
    ofParameter<bool> showWindow;
    ofParameter<int> timeWindowMs;
    ofParameter<int> minPitch;
    ofParameter<int> maxPitch;
    ofParameter<bool> showStrips;
    ofParameter<bool> reverseScroll;
    ofParameter<float> widgetWidth;
    ofParameter<float> widgetHeight;
    ofParameter<float> windowWidth;
    ofParameter<float> windowHeight;

    std::vector<LiveNote> notes;
    std::vector<ActiveVoiceState> activeVoices;

    float getVectorValue(const std::vector<float> &values, size_t index, float fallback) const {
        if(values.empty()) return fallback;
        if(values.size() == 1) return values[0];
        return index < values.size() ? values[index] : values.back();
    }

    bool isValidNoteIndex(int noteIndex) const {
        return noteIndex >= 0 && noteIndex < (int)notes.size();
    }

    void startVoiceNote(ActiveVoiceState &voice, int voiceIndex, int pitch, float velocity, uint64_t nowMs) {
        LiveNote note;
        note.startMs = nowMs;
        note.endMs = nowMs;
        note.pitch = pitch;
        note.velocity = velocity;
        note.voice = voiceIndex;
        note.active = true;
        notes.push_back(note);

        voice.active = true;
        voice.pitch = pitch;
        voice.velocity = velocity;
        voice.noteIndex = (int)notes.size() - 1;
    }

    void finishVoiceNote(ActiveVoiceState &voice, uint64_t nowMs) {
        if(isValidNoteIndex(voice.noteIndex)) {
            notes[voice.noteIndex].endMs = std::max(notes[voice.noteIndex].startMs + 1, nowMs);
            notes[voice.noteIndex].active = false;
        }
        voice.active = false;
        voice.noteIndex = -1;
    }

    void pruneOldNotes(uint64_t nowMs) {
        const uint64_t keepAfterMs = nowMs > (uint64_t)timeWindowMs.get() * 2 ? nowMs - (uint64_t)timeWindowMs.get() * 2 : 0;
        notes.erase(std::remove_if(notes.begin(), notes.end(), [keepAfterMs](const LiveNote &note) {
            return !note.active && note.endMs < keepAfterMs;
        }), notes.end());

        for(auto &voice : activeVoices) {
            voice.active = false;
            voice.noteIndex = -1;
        }
        for(size_t i = 0; i < notes.size(); i++) {
            if(notes[i].active && notes[i].voice >= 0 && notes[i].voice < (int)activeVoices.size()) {
                activeVoices[notes[i].voice].active = true;
                activeVoices[notes[i].voice].pitch = notes[i].pitch;
                activeVoices[notes[i].voice].velocity = notes[i].velocity;
                activeVoices[notes[i].voice].noteIndex = (int)i;
            }
        }
    }

    bool isBlackKey(int midiNote) const {
        int pitchClass = midiNote % 12;
        return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
    }

    float getActiveVelocityForPitch(int midiNote) const {
        float activeVelocity = 0.0f;
        for(const auto &note : notes) {
            if(note.active && note.pitch == midiNote) {
                activeVelocity = std::max(activeVelocity, note.velocity);
            }
        }
        return activeVelocity;
    }

    bool isHighlightedScalePitch(int midiNote) const {
        const int pitchClass = ((midiNote % 12) + 12) % 12;
        for(int scaleDegree : highlightScale.get()) {
            if((((scaleDegree % 12) + 12) % 12) == pitchClass) return true;
        }
        return false;
    }

    std::string midiToLabel(int midiNote) const {
        static const char *names[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
        return std::string(names[midiNote % 12]) + ofToString((midiNote / 12) - 1);
    }

    void determinePitchRange(int &outMinPitch, int &outMaxPitch) const {
        outMinPitch = std::min(minPitch.get(), maxPitch.get());
        outMaxPitch = std::max(minPitch.get(), maxPitch.get());
    }

    void drawWidget() {
        float zoom = ofxOceanodeShared::getZoomLevel();
        const auto &customRegionContext = ofxOceanodeShared::getCustomRegionRenderContext();
        const float width = customRegionContext.active
            ? std::max(1.0f, customRegionContext.width)
            : widgetWidth.get() * zoom;
        const float height = customRegionContext.active
            ? std::max(1.0f, customRegionContext.height)
            : widgetHeight.get() * zoom;
        drawPianoRoll(width, height);
    }

    void drawPianoRoll(float width, float height) {
        const uint64_t nowMs = ofGetElapsedTimeMillis();
        const uint64_t windowMs = std::max(1, timeWindowMs.get());
        const uint64_t windowStartMs = nowMs > windowMs ? nowMs - windowMs : 0;
        const bool keyboardOnRight = !reverseScroll.get();

        int rangeMinPitch = 48;
        int rangeMaxPitch = 72;
        determinePitchRange(rangeMinPitch, rangeMaxPitch);

        const int pitchCount = std::max(1, rangeMaxPitch - rangeMinPitch + 1);
        const float keyboardWidth = std::min(64.0f, width * 0.14f);
        const float rollWidth = std::max(1.0f, width - keyboardWidth);
        const float rowHeight = height / pitchCount;

        ImVec2 start = ImGui::GetCursorScreenPos();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const float rollLeftX = keyboardOnRight ? start.x : start.x + keyboardWidth;
        const float rollRightX = keyboardOnRight ? start.x + rollWidth : start.x + width;
        const float keyboardLeftX = keyboardOnRight ? start.x + rollWidth : start.x;
        const float keyboardRightX = keyboardLeftX + keyboardWidth;

        drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height), IM_COL32(16, 16, 16, 255));

        for(int i = 0; i < pitchCount; i++) {
            const int pitch = rangeMaxPitch - i;
            const float y = start.y + i * rowHeight;
            const bool blackKey = isBlackKey(pitch);
            const float keyRightX = keyboardLeftX + (blackKey ? keyboardWidth * 0.75f : keyboardWidth);
            const ImU32 whiteKeyBedColor = IM_COL32(230, 230, 230, 255);
            const float activeVelocity = getActiveVelocityForPitch(pitch);
            const bool activePitch = activeVelocity > 0.0f;
            const bool scalePitch = isHighlightedScalePitch(pitch);
            if(showStrips.get()) {
                const ImU32 rowColor = blackKey ? IM_COL32(28, 28, 28, 255) : IM_COL32(36, 36, 36, 255);
                drawList->AddRectFilled(ImVec2(rollLeftX, y), ImVec2(rollRightX, y + rowHeight), rowColor);
            }
            if(blackKey) {
                drawList->AddRectFilled(ImVec2(keyRightX, y), ImVec2(keyboardRightX, y + rowHeight), whiteKeyBedColor);
            }
            const int yellowBrightness = (int)ofClamp(190.0f + activeVelocity * 65.0f, 0.0f, 255.0f);
            const ImU32 keyColor = activePitch
                ? (blackKey
                    ? IM_COL32(yellowBrightness - 75, yellowBrightness - 75, 12, 255)
                    : IM_COL32(yellowBrightness, yellowBrightness, 40, 255))
                : (scalePitch
                    ? (blackKey ? IM_COL32(95, 150, 105, 255) : IM_COL32(150, 220, 165, 255))
                    : (blackKey ? IM_COL32(24, 24, 24, 255) : whiteKeyBedColor));
            drawList->AddRectFilled(ImVec2(keyboardLeftX, y), ImVec2(keyRightX, y + rowHeight),
                                    keyColor);
            drawList->AddLine(ImVec2(keyboardLeftX, y), ImVec2(keyboardRightX, y),
                              blackKey ? IM_COL32(120, 120, 120, 120) : IM_COL32(70, 70, 70, 120), 1.0f);
            drawList->AddLine(ImVec2(keyboardLeftX, y + rowHeight), ImVec2(keyboardRightX, y + rowHeight),
                              blackKey ? IM_COL32(6, 6, 6, 255) : IM_COL32(120, 120, 120, 210), 1.0f);
            if(blackKey) {
                drawList->AddLine(ImVec2(keyRightX, y), ImVec2(keyRightX, y + rowHeight), IM_COL32(0, 0, 0, 230), 1.0f);
            }
            if(showStrips.get()) {
                drawList->AddLine(ImVec2(start.x, y), ImVec2(start.x + width, y), IM_COL32(70, 70, 70, 100));
            }

            if(rowHeight >= 11.0f) {
                const ImU32 textColor = activePitch
                    ? (blackKey ? IM_COL32(245, 235, 140, 255) : IM_COL32(30, 30, 20, 255))
                    : (scalePitch
                        ? (blackKey ? IM_COL32(225, 245, 228, 255) : IM_COL32(35, 70, 40, 255))
                        : (blackKey ? IM_COL32(210, 210, 210, 255) : IM_COL32(30, 30, 30, 255)));
                drawList->AddText(ImVec2(keyboardLeftX + 4.0f, y + 1.0f), textColor, midiToLabel(pitch).c_str());
            }
        }

        const float dividerX = keyboardOnRight ? keyboardLeftX : keyboardRightX;
        drawList->AddLine(ImVec2(dividerX, start.y), ImVec2(dividerX, start.y + height), IM_COL32(100, 100, 100, 160), 1.5f);

        drawList->PushClipRect(ImVec2(rollLeftX, start.y), ImVec2(rollRightX, start.y + height), true);
        for(const auto &note : notes) {
            const uint64_t noteEndMs = note.active ? nowMs : note.endMs;
            if(noteEndMs < windowStartMs) continue;
            if(note.pitch < rangeMinPitch || note.pitch > rangeMaxPitch) continue;

            const double startOffsetMs = static_cast<double>(note.startMs) - static_cast<double>(windowStartMs);
            const double endOffsetMs = static_cast<double>(noteEndMs) - static_cast<double>(windowStartMs);
            const float startNorm = ofClamp((float)(startOffsetMs / (double)windowMs), 0.0f, 1.0f);
            const float endNorm = ofClamp((float)(endOffsetMs / (double)windowMs), 0.0f, 1.0f);
            const float startMappedNorm = reverseScroll.get() ? (1.0f - startNorm) : startNorm;
            const float endMappedNorm = reverseScroll.get() ? (1.0f - endNorm) : endNorm;
            float startX = rollLeftX + startMappedNorm * rollWidth;
            float endX = rollLeftX + endMappedNorm * rollWidth;
            float x1 = std::min(startX, endX);
            float x2 = std::max(startX, endX);
            if(note.active) {
                if(reverseScroll.get()) x1 = rollLeftX;
                else x2 = rollRightX;
            }
            if(x2 <= x1) x2 = x1 + 1.5f;

            const int pitchIndex = rangeMaxPitch - note.pitch;
            const float y1 = start.y + pitchIndex * rowHeight + 1.0f;
            const float y2 = y1 + std::max(2.0f, rowHeight - 2.0f);

            const int brightness = (int)ofClamp(40.0f + note.velocity * 215.0f, 0.0f, 255.0f);
            const ImU32 fillColor = IM_COL32(brightness, brightness, brightness, note.active ? 255 : 230);
            const ImU32 outlineColor = IM_COL32(std::min(255, brightness + 20), std::min(255, brightness + 20), std::min(255, brightness + 20), 255);
            drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), fillColor, 2.0f);
            drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), outlineColor, 2.0f, 0, note.active ? 2.0f : 1.0f);
        }
        drawList->PopClipRect();

        drawList->AddRect(ImVec2(start.x, start.y), ImVec2(start.x + width, start.y + height), IM_COL32(110, 110, 110, 180));
        ImGui::Dummy(ImVec2(width, height));
    }
};

#endif /* pianoRollLive_h */
