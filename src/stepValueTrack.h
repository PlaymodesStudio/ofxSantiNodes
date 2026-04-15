//
//  stepValueTrack.h
//  ofxSantiNodes
//
//  Timeline track that lets you draw and drag stepped float values.
//  Each step width is determined by the timeline's grid quantization.
//  Click+drag to paint values; right-click to erase a step.
//  Outputs the current value (mapped to [Min, Max]) at the playhead.
//

#ifndef stepValueTrack_h
#define stepValueTrack_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"
#include <map>
#include <cmath>

class stepValueTrack : public ofxOceanodeNodeModel, public transportTrack {
public:
    static constexpr int64_t TICKS_PER_BEAT = 24;  // same as MIDI / ppqTimeline

    stepValueTrack() : ofxOceanodeNodeModel("Step Value Track") {
        color = ofColor(180, 220, 255);
    }

    ~stepValueTrack() {
        if(currentTimeline) currentTimeline->unsubscribeTrack(this);
    }

    void setup() override {
        refreshTimelineList();
        addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
        addParameter(trackName.set("Track Name", "Steps " + ofToString(getNumIdentifier())));
        addParameter(outMin.set("Min",  0.0f, -1000.0f, 1000.0f));
        addParameter(outMax.set("Max",  1.0f, -1000.0f, 1000.0f));
        addParameter(clipStart   .set("Clip Start",   0.f,   0.f, 999.f));
        addParameter(clipEnd     .set("Clip End",    999.f,  0.f, 999.f));
        addParameter(clipDuration.set("Clip Dur",      4.f, 0.25f, 64.f));
        addParameter(clipLoop    .set("Loop",         false));

        addOutputParameter(valueOutput.set("Value", 0.0f, 0.0f, 1.0f));

        listeners.push(timelineSelect.newListener([this](int &){ updateSubscription(); }));
        valueOutput.setSerializable(false);

        updateSubscription();
    }

    void update(ofEventArgs &) override {
        if(!currentTimeline) { valueOutput = outMin.get(); return; }

        double beat  = currentTimeline->getBeatPosition();
        double dur   = (double)clipDuration.get();
        bool   lp    = clipLoop.get();
        double cs0   = getClipStartAt(0); // primary clip start
        bool   inClip = false;
        int nClips = getClipCount();
        for(int ci = 0; ci < nClips; ci++) {
            double cs = getClipStartAt(ci);
            double ce = getClipEndAt(ci);
            if(beat >= cs && beat < ce) {
                double local = beat - cs;
                if(lp && dur > 0.001) local = std::fmod(local, dur);
                beat = cs0 + local; // remap to primary clip space
                inClip = true;
                break;
            }
        }
        if(!inClip) { valueOutput = outMin.get(); return; }

        int64_t beatTick = (int64_t)std::round(beat * (double)TICKS_PER_BEAT);

        // Find the step whose range contains beatTick.
        // A step holds its value from its own tick until the next step's tick starts.
        // This is grid-independent: changing quantization never drops existing data.
        float v = 0.f;
        if(!steps.empty()) {
            auto it = steps.upper_bound(beatTick);
            if(it != steps.begin()) {
                --it;
                v = it->second;
            }
        }
        valueOutput = outMin.get() + v * (outMax.get() - outMin.get());
    }

    // ── transportTrack interface ──────────────────────────────────────────────
    std::string getTrackName() override { return trackName.get(); }
    float  getHeight()  const override  { return trackHeight; }
    void   setHeight(float h) override  { trackHeight = ofClamp(h, MIN_H, MAX_H); }
    bool   isCollapsed() const override { return collapsed; }
    void   setCollapsed(bool c) override { collapsed = c; }

    // ── Main draw ─────────────────────────────────────────────────────────────
    void drawInTimeline(ImDrawList* dl, ImVec2 /*pos*/, ImVec2 sz,
                        double viewStart, double viewEnd) override
    {
		float zoom = ofxOceanodeShared::getZoomLevel();
        std::string btnId = "##svt" + ofToString(getNumIdentifier());
        ImGui::InvisibleButton(btnId.c_str(), sz);

        ImVec2 p  = ImGui::GetItemRectMin();
        ImVec2 ep = ImGui::GetItemRectMax();
        ImVec2 s  = ImGui::GetItemRectSize();
        const float W = s.x, H = s.y;

        double visLen = viewEnd - viewStart;
        if(visLen <= 0.001) return;

        int    gTicks      = currentTimeline ? currentTimeline->getGridTicks()    : 24;
        double playhead    = currentTimeline ? currentTimeline->getBeatPosition()  : 0.0;
        double beatsPerBar = 4.0;
        if(currentTimeline) {
            beatsPerBar = double(currentTimeline->getNumerator()) *
                          (4.0 / double(currentTimeline->getDenominator()));
        }

        int64_t effTicks = gTicks > 0 ? (int64_t)gTicks : TICKS_PER_BEAT;
        double  stepSize = (double)effTicks / (double)TICKS_PER_BEAT;  // beats

        auto beatToX = [&](double b) {
            return p.x + float((b - viewStart) / visLen) * W;
        };
        auto xToBeat = [&](float x) {
            return viewStart + double(x - p.x) / W * visLen;
        };
        auto yToVal = [&](float y) -> float {
            return ofClamp(1.0f - (y - p.y) / H, 0.f, 1.f);
        };

        // ── Background ────────────────────────────────────────────────────────
        dl->AddRectFilled(p, ep, IM_COL32(35, 35, 45, 255));

        // ── Horizontal reference lines (25 / 50 / 75 %) ───────────────────────
        for(float ref : {0.25f, 0.5f, 0.75f}) {
            float ry = ep.y - ref * H;
            dl->AddLine(ImVec2(p.x, ry), ImVec2(ep.x, ry), IM_COL32(55, 55, 72, 130), 0.5f);
        }

        // ── Bar lines ─────────────────────────────────────────────────────────
        int vSBar = (int)std::floor(viewStart / beatsPerBar);
        int vEBar = (int)std::ceil (viewEnd   / beatsPerBar);
        for(int bar = vSBar; bar <= vEBar; bar++) {
            float bx = beatToX(bar * beatsPerBar);
            if(bx < p.x - 2 || bx > ep.x + 2) continue;
            dl->AddLine(ImVec2(bx, p.y), ImVec2(bx, ep.y), IM_COL32(80, 80, 105, 170), 1.5f);
        }

        // ── Current-grid reference lines (faint, insertion orientation only) ────
        {
            int64_t firstTick = snapToStepTick(viewStart - stepSize, effTicks);
            int64_t lastTick  = snapToStepTick(viewEnd   + stepSize, effTicks);
            for(int64_t tick = firstTick; tick <= lastTick; tick += effTicks) {
                float gx = beatToX((double)tick / (double)TICKS_PER_BEAT);
                if(gx >= p.x && gx <= ep.x)
                    dl->AddLine(ImVec2(gx, p.y), ImVec2(gx, ep.y),
                                IM_COL32(50, 50, 65, 60), 0.5f);
            }
        }

        // ── Stored steps — iterated from map directly, grid-independent ────────
        // Each step spans from its tick to the next step's tick (staircase model).
        // Changing the timeline quantization never hides or removes existing data.
        if(!steps.empty()) {
            int64_t viewStartTick = (int64_t)std::floor(viewStart * (double)TICKS_PER_BEAT);
            int64_t viewEndTick   = (int64_t)std::ceil (viewEnd   * (double)TICKS_PER_BEAT);
            int64_t playTick      = (int64_t)std::round(playhead  * (double)TICKS_PER_BEAT);

            // Start one step before view so steps that begin before but extend into view are drawn
            auto it = steps.upper_bound(viewStartTick);
            if(it != steps.begin()) --it;

            for(; it != steps.end(); ++it) {
                if(it->first > viewEndTick) break;

                // Width: span to the next stored step; last step gets current grid width
                auto   nextIt  = std::next(it);
                int64_t endTick = (nextIt != steps.end()) ? nextIt->first
                                                          : it->first + effTicks;
                double b  = (double)it->first / (double)TICKS_PER_BEAT;
                double nb = (double)endTick    / (double)TICKS_PER_BEAT;

                float x1 = std::max(beatToX(b),  p.x);
                float x2 = std::min(beatToX(nb), ep.x);
                if(x2 <= x1 + 0.5f) continue;

                float v    = it->second;
                float topY = ep.y - v * H;

                bool  active = (playTick >= it->first && playTick < endTick);
                ImU32 fill   = active ? IM_COL32(130, 210, 255, 230)
                                      : IM_COL32(80,  160, 220, 185);
                ImU32 edge   = IM_COL32(110, 185, 245, 90);

                dl->AddRectFilled(ImVec2(x1+1, topY),   ImVec2(x2-1, ep.y-1), fill, 2.f);
                dl->AddRect      (ImVec2(x1+1, topY),   ImVec2(x2-1, ep.y-1), edge, 2.f, 0, 1.f);
            }
        }

        // ── Playhead ──────────────────────────────────────────────────────────
        float phX = beatToX(playhead);
        if(phX >= p.x && phX <= ep.x)
            dl->AddLine(ImVec2(phX, p.y), ImVec2(phX, ep.y), IM_COL32(255, 80, 80, 255), 2.5f);

        // ── Border ────────────────────────────────────────────────────────────
        dl->AddRect(p, ep, IM_COL32(60, 60, 75, 255));

        // ── Interactions ──────────────────────────────────────────────────────
        ImVec2 mouse = ImGui::GetMousePos();
        bool hover   = ImGui::IsItemHovered();
        bool lDown   = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool lClick  = hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool rClick  = hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        bool released= ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        bool inTrack = (mouse.x >= p.x && mouse.x <= ep.x &&
                        mouse.y >= p.y  && mouse.y <= ep.y);

        // Left-click begins painting; held drag continues
        if(lClick)    isPainting = true;
        if(released)  isPainting = false;

        if(isPainting && lDown && inTrack) {
            int64_t key = snapToStepTick(xToBeat(mouse.x), effTicks);
            float   v   = yToVal(mouse.y);
            steps[key]  = v;
        }

        // Right-click erases whichever step contains the clicked beat
        if(rClick && !steps.empty()) {
            int64_t clickTick = (int64_t)std::round(xToBeat(mouse.x) * (double)TICKS_PER_BEAT);
            auto it = steps.upper_bound(clickTick);
            if(it != steps.begin()) {
                --it;
                steps.erase(it);
            }
        }
    }

    // Clip window virtuals
    bool   hasClipWindow()        const override { return true; }
    double getClipStart()         const override { return clipStart.get(); }
    double getClipEnd()           const override { return clipEnd.get(); }
    double getClipDuration()      const override { return clipDuration.get(); }
    bool   getClipLoop()          const override { return clipLoop.get(); }
    void   setClipStart(double v)    override { clipStart.set((float)v); }
    void   setClipEnd(double v)      override { clipEnd.set((float)v); }
    void   setClipDuration(double v) override { clipDuration.set((float)ofClamp(v, 0.25, 64.0)); }
    void   setClipLoop(bool v)       override { clipLoop.set(v); }

    // Mini content — draw step bars inside clip bar
    void drawMiniContent(ImDrawList* dl, ImVec2 p1, ImVec2 p2,
                         double viewBeat0, double viewBeat1,
                         double clipOrigin = -1.0) override
    {
		float zoom = ofxOceanodeShared::getZoomLevel();
        if(steps.empty()) return;
        double barLen = viewBeat1 - viewBeat0;
        if(barLen <= 0.001) return;
        float W = p2.x - p1.x;
        float H = p2.y - p1.y;
        if(W <= 1.f || H <= 1.f) return;

        double cs_primary = (double)clipStart.get();
        double cs  = (clipOrigin >= 0.0) ? clipOrigin : cs_primary;
        double dur = (double)clipDuration.get();
        bool   lp  = clipLoop.get();

        int repStart = 0, repEnd = 0;
        if(lp && dur > 0.001) {
            repStart = std::max(0, (int)std::floor((viewBeat0 - cs) / dur) - 1);
            repEnd   = (int)std::ceil((viewBeat1 - cs) / dur);
        }

        int64_t effTicks = currentTimeline ? currentTimeline->getGridTicks() : 6;
        if(effTicks <= 0) effTicks = 6;

        dl->PushClipRect(p1, p2, true);
        for(int rep = repStart; rep <= repEnd; rep++) {
            double offset = rep * dur;
            for(auto it = steps.begin(); it != steps.end(); ++it) {
                double b  = (double)it->first / (double)TICKS_PER_BEAT;
                // When looping, only show steps within the primary clip period
                if(lp && dur > 0.001 && (b < cs_primary || b >= cs_primary + dur)) continue;
                auto nextIt = std::next(it);
                int64_t endTick = (nextIt != steps.end()) ? nextIt->first : it->first + effTicks;
                double nb = (double)endTick / (double)TICKS_PER_BEAT;
                float x1 = p1.x + float((b  - cs_primary + offset) / barLen) * W;
                float x2 = p1.x + float((nb - cs_primary + offset) / barLen) * W;
                if(x2 < p1.x || x1 > p2.x) continue;
                float topY = p2.y - it->second * H;
                dl->AddRectFilled(ImVec2(std::max(x1,p1.x)+1, topY),
                                  ImVec2(std::min(x2,p2.x)-1, p2.y-1),
                                  IM_COL32(110, 185, 245, 160), 1.f);
            }
        }
        dl->PopClipRect();
    }

    // Content stretching — scale all step positions relative to primary clip start
    void beginContentStretch() override {
        stepsSnapshot = steps;
    }
    void applyContentStretch(double factor) override {
        if(stepsSnapshot.empty()) return;
        factor = std::max(0.01, factor);
        double cs0 = (double)clipStart.get();
        int64_t cs0Ticks = (int64_t)std::round(cs0 * (double)TICKS_PER_BEAT);
        steps.clear();
        for(auto& kv : stepsSnapshot) {
            int64_t localTick = kv.first - cs0Ticks;
            int64_t newTick = cs0Ticks + (int64_t)std::round((double)localTick * factor);
            steps[newTick] = kv.second;
        }
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        if(json.count("steps")) {
            steps.clear();
            for(auto& d : json["steps"])
                if(d.size() >= 2)
                    steps[(int64_t)(long long)(float)d[0]] = (float)d[1];
        }
        if(json.count("trackHeight")) trackHeight = ofClamp((float)json["trackHeight"], MIN_H, MAX_H);
        if(json.count("collapsed"))   collapsed   = json["collapsed"];
        if(json.count("clipStart"))    clipStart    = (float)json["clipStart"];
        if(json.count("clipEnd"))      clipEnd      = (float)json["clipEnd"];
        if(json.count("clipDuration")) clipDuration = (float)json["clipDuration"];
        if(json.count("clipLoop"))     clipLoop     = (bool) json["clipLoop"];
        loadExtraClips(json);
    }

    void presetSave(ofJson &json) override {
        std::vector<std::vector<float>> sd;
        for(auto& kv : steps)
            sd.push_back({(float)(long long)kv.first, kv.second});
        json["steps"]        = sd;
        json["trackHeight"]  = trackHeight;
        json["collapsed"]    = collapsed;
        json["clipStart"]    = clipStart.get();
        json["clipEnd"]      = clipEnd.get();
        json["clipDuration"] = clipDuration.get();
        json["clipLoop"]     = clipLoop.get();
        saveExtraClips(json);
    }

private:
    // ── Parameters ────────────────────────────────────────────────────────────
    ofParameter<int>         timelineSelect;
    ofParameter<std::string> trackName;
    ofParameter<float>       outMin;
    ofParameter<float>       clipStart;
    ofParameter<float>       clipEnd;
    ofParameter<float>       clipDuration;
    ofParameter<bool>        clipLoop;
    ofParameter<float>       outMax;
    ofParameter<float>       valueOutput;

    ppqTimeline*             currentTimeline = nullptr;
    std::vector<std::string> timelineOptions;
    ofEventListeners         listeners;

    // ── Step data: MIDI-tick position → normalized value [0, 1] ──────────────
    std::map<int64_t, float> steps;
    std::map<int64_t, float> stepsSnapshot; // for beginContentStretch

    bool isPainting = false;

    // ── Track geometry ────────────────────────────────────────────────────────
    float trackHeight = 120.0f;
    bool  collapsed   = false;
    static constexpr float MIN_H = 60.0f;
    static constexpr float MAX_H = 400.0f;

    // ── Helpers ───────────────────────────────────────────────────────────────
    // Converts a beat position to the tick index of the step it falls in.
    // Uses integer floor division so negative beats are handled correctly.
    static int64_t snapToStepTick(double beat, int64_t gridTicks) {
        int64_t t = (int64_t)std::round(beat * (double)TICKS_PER_BEAT);
        if(gridTicks <= 0) return t;
        // Floor division towards -∞
        int64_t q = t / gridTicks;
        if(t < 0 && t % gridTicks != 0) q--;
        return q * gridTicks;
    }

    void refreshTimelineList() {
        timelineOptions.clear();
        timelineOptions.push_back("None");
        for(auto* tl : ppqTimeline::getTimelines())
            timelineOptions.push_back("Timeline " + ofToString(tl->getNumIdentifier()));
        timelineSelect.set("Timeline", 0, 0, (int)timelineOptions.size()-1);
    }

    void updateSubscription() {
        if(currentTimeline) currentTimeline->unsubscribeTrack(this);
        int idx = timelineSelect.get() - 1;
        auto& tls = ppqTimeline::getTimelines();
        if(idx >= 0 && idx < (int)tls.size()) {
            currentTimeline = tls[idx];
            currentTimeline->subscribeTrack(this);
        } else {
            currentTimeline = nullptr;
        }
    }
};

#endif /* stepValueTrack_h */
