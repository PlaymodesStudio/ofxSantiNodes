#ifndef trackScheduler_h
#define trackScheduler_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// trackScheduler
//
// A bird's-eye arrangement overview that subscribes to a ppqTimeline and
// displays every other subscribed track's clip window as a colored horizontal
// bar. Dragging a bar moves it; dragging its left/right edge resizes it.
//
// Only tracks that implement hasClipWindow() = true are shown.
// ─────────────────────────────────────────────────────────────────────────────
class trackScheduler : public ofxOceanodeNodeModel, public transportTrack {
public:
    trackScheduler() : ofxOceanodeNodeModel("Track Scheduler") {
        color = ofColor(200, 200, 255);
    }

    ~trackScheduler() {
        if(currentTimeline) currentTimeline->unsubscribeTrack(this);
    }

    void setup() override {
        refreshTimelineList();
        addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
        addParameter(trackName.set("Track Name",
                                   "Scheduler " + ofToString(getNumIdentifier())));
        listeners.push(timelineSelect.newListener([this](int&){ updateSubscription(); }));
        updateSubscription();
    }

    // No per-frame output — this node is purely visual
    void update(ofEventArgs&) override {}

    // ── transportTrack interface ───────────────────────────────────────────
    std::string getTrackName() override { return trackName.get(); }
    float  getHeight()  const override  { return trackHeight; }
    void   setHeight(float h) override  { trackHeight = ofClamp(h, MIN_H, MAX_H); }
    bool   isCollapsed() const override { return collapsed; }
    void   setCollapsed(bool c) override { collapsed = c; }

    // ── Preset ────────────────────────────────────────────────────────────
    void presetSave(ofJson& json) override {
        json["trackHeight"] = trackHeight;
        json["collapsed"]   = collapsed;
    }
    void presetRecallAfterSettingParameters(ofJson& json) override {
        if(json.count("trackHeight")) trackHeight = ofClamp((float)json["trackHeight"], MIN_H, MAX_H);
        if(json.count("collapsed"))   collapsed   = json["collapsed"];
    }

    // ── Draw ──────────────────────────────────────────────────────────────
    void drawInTimeline(ImDrawList* dl, ImVec2 /*pos*/, ImVec2 sz,
                        double viewStart, double viewEnd) override
    {
        std::string btnId = "##tsch" + ofToString(getNumIdentifier());
        ImGui::InvisibleButton(btnId.c_str(), sz);

        ImVec2 p  = ImGui::GetItemRectMin();
        ImVec2 s  = ImGui::GetItemRectSize();
        ImVec2 ep = ImGui::GetItemRectMax();

        double visLen = viewEnd - viewStart;
        if(visLen <= 0.001) return;

        // Coordinate helpers
        auto beatToX = [&](double b) {
            return p.x + float((b - viewStart) / visLen) * s.x;
        };
        auto xToBeat = [&](float x) {
            return viewStart + double(x - p.x) / s.x * visLen;
        };

        double playhead = currentTimeline ? currentTimeline->getBeatPosition() : 0.0;

        // Collect tracks that have a clip window (excluding ourselves)
        std::vector<transportTrack*> clipTracks;
        if(currentTimeline) {
            for(auto* t : currentTimeline->getSubscribedTracks()) {
                if(t == this) continue;
                if(t->hasClipWindow()) clipTracks.push_back(t);
            }
        }

        // ── Background ────────────────────────────────────────────────────
        dl->AddRectFilled(p, ep, IM_COL32(22, 24, 32, 255));

        // Grid reference lines
        if(currentTimeline) {
            int num  = currentTimeline->getNumerator();
            int den  = currentTimeline->getDenominator();
            int gTicks = currentTimeline->getGridTicks();
            double bpb = double(num) * (4.0 / double(den));  // beats per bar

            int barS = (int)std::floor(viewStart / bpb);
            int barE = (int)std::ceil (viewEnd   / bpb) + 1;

            dl->PushClipRect(p, ep, true);
            for(int bar = barS; bar <= barE; bar++) {
                float bx = beatToX(bar * bpb);
                if(bx >= p.x && bx <= ep.x)
                    dl->AddLine(ImVec2(bx, p.y), ImVec2(bx, ep.y),
                                IM_COL32(65, 70, 95, 130), 1.f);

                if(gTicks > 0) {
                    double gb = gTicks / 24.0;
                    for(double b = bar * bpb + gb; b < (bar + 1) * bpb - gb * 0.5; b += gb) {
                        float gx = beatToX(b);
                        if(gx >= p.x && gx <= ep.x)
                            dl->AddLine(ImVec2(gx, p.y), ImVec2(gx, ep.y),
                                        IM_COL32(45, 50, 68, 65), 0.5f);
                    }
                }
            }
            dl->PopClipRect();
        }

        dl->AddRect(p, ep, IM_COL32(55, 60, 80, 255));

        if(clipTracks.empty()) {
            dl->AddText(ImVec2(p.x+8, p.y+s.y/2-6),
                        IM_COL32(100,110,140,200),
                        "No clip-window tracks subscribed to this timeline.");
            drawPlayhead(dl, p, ep, beatToX(playhead));
            return;
        }

        // Resize trackHeight to fit all lanes if needed
        float needed = (float)clipTracks.size() * LANE_H + 4.f;
        if(trackHeight < needed) trackHeight = std::min(needed, MAX_H);

        float laneH  = s.y / (float)clipTracks.size();

        bool altHeld = ImGui::GetIO().KeyAlt;
        bool cmdHeld = ImGui::GetIO().KeySuper;

        for(int i = 0; i < (int)clipTracks.size(); i++) {
            transportTrack* t = clipTracks[i];
            float ly1 = p.y + i * laneH;
            float ly2 = ly1 + laneH;
            float lyCtr = (ly1 + ly2) * 0.5f;

            // Subtle lane separator
            if(i > 0)
                dl->AddLine(ImVec2(p.x, ly1), ImVec2(ep.x, ly1),
                            IM_COL32(45,50,65,255), 1.f);

            int nClips = t->getClipCount();
            ImU32 fill = trackColor(i, false);
            ImU32 edge = trackColor(i, true);
            bool nameDrawn = false;

            for(int ci = 0; ci < nClips; ci++) {
                double cs = t->getClipStartAt(ci);
                double ce = t->getClipEndAt(ci);

                float barX1 = std::max(beatToX(cs), p.x);
                float barX2 = std::min(beatToX(ce), ep.x);

                if(barX2 > barX1) {
                    bool isActive = (playhead >= cs && playhead < ce);
                    ImU32 fillCol = isActive ? edge : fill;
                    // Extra clips slightly dimmer
                    if(ci > 0) fillCol = IM_COL32(
                        (fillCol >> 0  & 0xFF),
                        (fillCol >> 8  & 0xFF),
                        (fillCol >> 16 & 0xFF), 180);

                    dl->AddRectFilled(ImVec2(barX1, ly1+3), ImVec2(barX2, ly2-3), fillCol, 3.f);

                    // Mini content preview
                    {
                        ImVec2 mp1(barX1 + 2.f, ly1 + 5.f);
                        ImVec2 mp2(barX2 - 2.f, ly2 - 5.f);
                        t->drawMiniContent(dl, mp1, mp2, xToBeat(barX1), xToBeat(barX2), cs);
                    }

                    ImU32 edgeCol = (dragTrackIdx == i && dragClipIdx == ci)
                                   ? IM_COL32(255,255,255,220) : edge;
                    dl->AddRect(ImVec2(barX1, ly1+3), ImVec2(barX2, ly2-3), edgeCol, 3.f, 0, 1.5f);

                    // Track name badge — only on the first (leftmost) visible clip
                    if(!nameDrawn) {
                        float nameX = std::max(barX1 + 4.f, p.x + 4.f);
                        float nameY = lyCtr - 6.f;
                        dl->PushClipRect(ImVec2(barX1+2, ly1+2), ImVec2(barX2-2, ly2-2), true);
                        dl->AddText(ImVec2(nameX+1, nameY+1),
                                    IM_COL32(0,0,0,160), t->getTrackName().c_str());
                        dl->AddText(ImVec2(nameX, nameY),
                                    IM_COL32(220,230,255,210), t->getTrackName().c_str());
                        dl->PopClipRect();
                        nameDrawn = true;
                    }

                    // Left (start) handle — green
                    {
                        float hx = beatToX(cs);
                        if(hx >= p.x - HANDLE_ZONE && hx <= ep.x + HANDLE_ZONE) {
                            ImU32 hcol = (dragTrackIdx == i && dragClipIdx == ci && dragMode == DRAG_START)
                                         ? IM_COL32(150,255,150,255) : IM_COL32(100,230,120,220);
                            dl->AddLine(ImVec2(hx, ly1+1), ImVec2(hx, ly2-1), hcol, 2.f);
                            dl->AddTriangleFilled(ImVec2(hx, ly1+3),
                                                 ImVec2(hx+7, lyCtr),
                                                 ImVec2(hx, ly2-3), hcol);
                        }
                    }
                    // Right (end) handle — orange / yellow when Alt
                    if(ce < 9998.0) {
                        float hx = beatToX(ce);
                        if(hx >= p.x - HANDLE_ZONE && hx <= ep.x + HANDLE_ZONE) {
                            bool stretchActive = (dragTrackIdx == i && dragClipIdx == ci && dragMode == DRAG_STRETCH);
                            ImU32 hcol = stretchActive                                              ? IM_COL32(255,255, 80,255)
                                       : (dragTrackIdx == i && dragClipIdx == ci && dragMode == DRAG_END) ? IM_COL32(255,200,100,255)
                                       : altHeld                                                    ? IM_COL32(240,240, 80,200)
                                                                                                    : IM_COL32(255,160, 60,220);
                            dl->AddLine(ImVec2(hx, ly1+1), ImVec2(hx, ly2-1), hcol, 2.f);
                            dl->AddTriangleFilled(ImVec2(hx, ly1+3),
                                                 ImVec2(hx-7, lyCtr),
                                                 ImVec2(hx, ly2-3), hcol);
                        }
                    }
                } else {
                    // Bar off-screen — marker line at cs
                    float hx = beatToX(cs);
                    if(hx >= p.x && hx <= ep.x)
                        dl->AddLine(ImVec2(hx, ly1+3), ImVec2(hx, ly2-3),
                                    trackColor(i, true), 2.f);
                }
            }
        }

        // Playhead
        drawPlayhead(dl, p, ep, beatToX(playhead));

        // ── Interactions ──────────────────────────────────────────────────
        ImVec2 mouse = ImGui::GetMousePos();
        bool   hover  = ImGui::IsItemHovered();
        bool   lClick = hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool   drag   = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        bool   rel    = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        if(lClick) {
            int lane = (int)((mouse.y - p.y) / laneH);
            if(lane >= 0 && lane < (int)clipTracks.size()) {
                transportTrack* t = clipTracks[lane];
                int nClips = t->getClipCount();
                bool handled = false;

                for(int ci = 0; ci < nClips && !handled; ci++) {
                    double cs = t->getClipStartAt(ci);
                    double ce = t->getClipEndAt(ci);
                    float csX = beatToX(cs);
                    float ceX = beatToX(ce);

                    if(std::abs(mouse.x - csX) <= HANDLE_ZONE) {
                        dragTrackIdx = lane; dragClipIdx = ci; dragMode = DRAG_START;
                        handled = true;
                    } else if(ce < 9998.0 && std::abs(mouse.x - ceX) <= HANDLE_ZONE) {
                        dragTrackIdx = lane; dragClipIdx = ci;
                        if(altHeld) {
                            dragMode    = DRAG_STRETCH;
                            dragStartCs = cs;
                            dragStartCe = ce;
                            dragStartCd = t->getClipDuration();
                            t->beginContentStretch();
                        } else {
                            dragMode = DRAG_END;
                        }
                        handled = true;
                    } else if(mouse.x >= csX && mouse.x <= ceX) {
                        if(cmdHeld) {
                            // Cmd+drag: create a new copy of this clip and drag it
                            int newIdx  = t->addClip(cs, ce);
                            dragTrackIdx   = lane;
                            dragClipIdx    = newIdx;
                            dragMode       = DRAG_COPY;
                            dragBeatAnchor = snapBeat(xToBeat(mouse.x)) - cs;
                        } else {
                            dragTrackIdx   = lane;
                            dragClipIdx    = ci;
                            dragMode       = DRAG_MOVE;
                            dragBeatAnchor = snapBeat(xToBeat(mouse.x)) - cs;
                        }
                        handled = true;
                    }
                }
                if(!handled) { dragTrackIdx = -1; dragClipIdx = 0; dragMode = DRAG_NONE; }
            }
        }

        // Right-click: remove extra clip (not the primary, ci > 0)
        bool rClick = hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if(rClick && dragMode == DRAG_NONE) {
            int lane = (int)((mouse.y - p.y) / laneH);
            if(lane >= 0 && lane < (int)clipTracks.size()) {
                transportTrack* t = clipTracks[lane];
                int nClips = t->getClipCount();
                for(int ci = nClips - 1; ci >= 1; ci--) {
                    double cs = t->getClipStartAt(ci);
                    double ce = t->getClipEndAt(ci);
                    if(mouse.x >= beatToX(cs) && mouse.x <= beatToX(ce)) {
                        t->removeClip(ci);
                        break;
                    }
                }
            }
        }

        if(drag && dragTrackIdx >= 0 && dragTrackIdx < (int)clipTracks.size()) {
            transportTrack* t = clipTracks[dragTrackIdx];
            double curBeat = snapBeat(xToBeat(mouse.x));
            double cs  = t->getClipStartAt(dragClipIdx);
            double ce  = t->getClipEndAt(dragClipIdx);
            double dur = ce - cs;

            if(dragMode == DRAG_START) {
                double newCs = ofClamp(curBeat, 0.0, ce - 0.25);
                t->setClipStartAt(dragClipIdx, newCs);
            } else if(dragMode == DRAG_END) {
                t->setClipEndAt(dragClipIdx, ofClamp(curBeat, cs + 0.25, 9998.0));
            } else if(dragMode == DRAG_MOVE || dragMode == DRAG_COPY) {
                double nb = ofClamp(curBeat - dragBeatAnchor, 0.0, 9998.0 - dur);
                t->setClipStartAt(dragClipIdx, nb);
                t->setClipEndAt(dragClipIdx, nb + dur);
            } else if(dragMode == DRAG_STRETCH) {
                double newCe  = ofClamp(curBeat, dragStartCs + 0.25, 9998.0);
                double origLen = dragStartCe - dragStartCs;
                if(origLen > 0.001) {
                    double stretch = (newCe - dragStartCs) / origLen;
                    t->setClipEndAt(dragClipIdx, newCe);
                    t->setClipDuration(std::max(0.25, dragStartCd * stretch));
                    t->applyContentStretch(stretch);
                }
            }

            ImGui::SetMouseCursor(dragMode == DRAG_MOVE || dragMode == DRAG_COPY
                                  ? ImGuiMouseCursor_ResizeAll
                                  : ImGuiMouseCursor_ResizeEW);
        }

        if(rel) { dragTrackIdx = -1; dragClipIdx = 0; dragMode = DRAG_NONE; }

        // Hover cursors
        if(hover && dragMode == DRAG_NONE) {
            int lane = (int)((mouse.y - p.y) / laneH);
            if(lane >= 0 && lane < (int)clipTracks.size()) {
                transportTrack* t = clipTracks[lane];
                int nClips = t->getClipCount();
                for(int ci = 0; ci < nClips; ci++) {
                    double cs = t->getClipStartAt(ci);
                    double ce = t->getClipEndAt(ci);
                    float csX = beatToX(cs);
                    float ceX = beatToX(ce);
                    if(std::abs(mouse.x - csX) <= HANDLE_ZONE ||
                       (ce < 9998.0 && std::abs(mouse.x - ceX) <= HANDLE_ZONE)) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        break;
                    } else if(mouse.x >= csX && mouse.x <= ceX) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                        break;
                    }
                }
            }
        }
    }

private:
    ofParameter<int>         timelineSelect;
    ofParameter<std::string> trackName;
    ppqTimeline*             currentTimeline = nullptr;
    std::vector<std::string> timelineOptions;
    ofEventListeners         listeners;

    // Drag state
    enum DragMode { DRAG_NONE, DRAG_START, DRAG_END, DRAG_MOVE, DRAG_STRETCH, DRAG_COPY };
    DragMode dragMode     = DRAG_NONE;
    int      dragTrackIdx = -1;
    int      dragClipIdx  = 0;   // which clip index within the track is being dragged
    double   dragBeatAnchor  = 0.0;
    // Used for DRAG_STRETCH
    double   dragStartCs  = 0.0;
    double   dragStartCe  = 0.0;
    double   dragStartCd  = 0.0;

    float trackHeight = 120.f;
    bool  collapsed   = false;
    static constexpr float MIN_H       = 40.f;
    static constexpr float MAX_H       = 600.f;
    static constexpr float LANE_H      = 28.f;
    static constexpr float HANDLE_ZONE = 8.f;

    double snapBeat(double beat) const {
        if(!currentTimeline) return beat;
        int gTicks = currentTimeline->getGridTicks();
        if(gTicks <= 0) return beat;
        double gridBeats = gTicks / 24.0;
        return std::round(beat / gridBeats) * gridBeats;
    }

    static ImU32 trackColor(int idx, bool bright) {
        static const ImU32 cols[] = {
            IM_COL32( 80,150,255,255), IM_COL32(100,210,120,255),
            IM_COL32(255,175, 60,255), IM_COL32(200,100,220,255),
            IM_COL32(255,100,100,255), IM_COL32( 60,210,200,255),
        };
        ImU32 c = cols[idx % 6];
        if(!bright) {
            int r = int((c >> 0  & 0xFF) * 0.22f);
            int g = int((c >> 8  & 0xFF) * 0.22f);
            int b = int((c >> 16 & 0xFF) * 0.22f);
            return IM_COL32(r, g, b, 220);
        }
        return c;
    }

    void drawPlayhead(ImDrawList* dl, ImVec2 p, ImVec2 ep, float phX) {
        if(phX >= p.x && phX <= ep.x)
            dl->AddLine(ImVec2(phX, p.y), ImVec2(phX, ep.y),
                        IM_COL32(255, 80, 80, 255), 2.5f);
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

#endif /* trackScheduler_h */
