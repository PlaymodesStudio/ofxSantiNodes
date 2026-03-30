#ifndef pianoRollTrack_h
#define pianoRollTrack_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

struct MidiNote {
	double startBeat;
	float  length;
	int    pitch;       // MIDI note number 0-127
	float  velocity;    // 0.0 – 1.0
	float  probability; // 0.0 – 1.0  (gate fire chance)

	MidiNote() : startBeat(0), length(1), pitch(60), velocity(0.8f), probability(1.0f) {}

	double endBeat() const { return startBeat + length; }

	bool operator<(const MidiNote& o) const {
		if(startBeat != o.startBeat) return startBeat < o.startBeat;
		return pitch < o.pitch;
	}
};

class pianoRollTrack : public ofxOceanodeNodeModel, public transportTrack {
public:
	pianoRollTrack() : ofxOceanodeNodeModel("Piano Roll Track") {
		color = ofColor(180, 255, 180);
	}

	~pianoRollTrack() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
	}

	void setup() override {
		refreshTimelineList();
		addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
		addParameter(trackName.set("Track Name", "Piano " + ofToString(getNumIdentifier())));
		addParameter(scrollOffset.set("Scroll", 48, 0, 127));
		addParameter(visibleRange.set("Zoom", 36, 12, 88));

		// ── Clip window ─────────────────────────────────────────────────────
		// Notes are stored in local beat space [0, clipDuration).
		// clipStart/clipEnd define the active window in global timeline beats.
		// Default: clipStart=0, clipEnd=999 → same behaviour as before.
		addParameter(clipStart   .set("Clip Start",   0.f,   0.f, 999.f));
		addParameter(clipEnd     .set("Clip End",    999.f,   0.f, 999.f));
		addParameter(clipDuration.set("Clip Dur",      4.f, 0.25f,  64.f));
		addParameter(clipLoop    .set("Loop",         false));

		addOutputParameter(pitchesOutput.set("Pitch[]",    {0}, {0}, {127}));
		addOutputParameter(velocitiesOutput.set("Velocity[]", {0}, {0}, {1}));
		addOutputParameter(gatesOutput.set("Gate[]",      {0}, {0}, {1}));
		addOutputParameter(numActiveOutput.set("Num Active", 0, 0, 128));

		listeners.push(timelineSelect.newListener([this](int &){ updateSubscription(); }));

		pitchesOutput.setSerializable(false);
		velocitiesOutput.setSerializable(false);
		gatesOutput.setSerializable(false);
		numActiveOutput.setSerializable(false);

		updateSubscription();
	}

	void update(ofEventArgs &) override {
		if(!currentTimeline) {
			pitchesOutput    = vector<float>();
			velocitiesOutput = vector<float>();
			gatesOutput      = vector<float>();
			numActiveOutput  = 0;
			return;
		}

		double beat = currentTimeline->getBeatPosition();

		// ── Clip window — check all clips ──────────────────────────────────
		double dur = (double)clipDuration.get();
		bool   lp  = clipLoop.get();
		double localBeat = -1.0;

		int nClips = getClipCount();
		for(int ci = 0; ci < nClips; ci++) {
			double cs = getClipStartAt(ci);
			double ce = getClipEndAt(ci);
			if(beat >= cs && beat < ce) {
				localBeat = beat - cs;
				if(lp && dur > 0.001) localBeat = std::fmod(localBeat, dur);
				break;
			}
		}

		if(localBeat < 0.0) {
			pitchesOutput = {}; velocitiesOutput = {}; gatesOutput = {};
			numActiveOutput = 0;
			prevActiveNotes.clear(); gateRollResults.clear();
			return;
		}

		// ── Probability gate: roll once per note onset, hold for note duration ──
		std::set<NoteKey> currentActive;
		for(const auto& n : notes)
			if(localBeat >= n.startBeat && localBeat < n.endBeat())
				currentActive.insert({n.startBeat, n.pitch});

		for(const auto& key : currentActive) {
			if(!prevActiveNotes.count(key)) {
				// New onset — find the note and roll
				float prob = 1.0f;
				for(const auto& n : notes)
					if(n.startBeat == key.first && n.pitch == key.second)
						{ prob = n.probability; break; }
				gateRollResults[key] = (ofRandom(1.0f) < prob);
			}
		}
		for(auto it = gateRollResults.begin(); it != gateRollResults.end(); )
			it = currentActive.count(it->first) ? std::next(it) : gateRollResults.erase(it);
		prevActiveNotes = currentActive;

		vector<float> ap, av, ag;
		for(const auto& n : notes) {
			if(localBeat >= n.startBeat && localBeat < n.endBeat()) {
				NoteKey key{n.startBeat, n.pitch};
				if(gateRollResults.count(key) && gateRollResults.at(key)) {
					ap.push_back(n.pitch);
					av.push_back(n.velocity);
					ag.push_back(1.0f);
				}
			}
		}
		pitchesOutput    = ap;
		velocitiesOutput = av;
		gatesOutput      = ag;
		numActiveOutput  = (int)ap.size();
	}

	// ── TransportTrack ────────────────────────────────────────────────────────
	std::string getTrackName() override { return trackName.get(); }
	float  getHeight()  const override  { return trackHeight; }
	void   setHeight(float h) override  { trackHeight = ofClamp(h, MIN_H, MAX_H); }
	bool   isCollapsed() const override { return collapsed; }
	void   setCollapsed(bool c) override { collapsed = c; }

	// Clip window accessors (used by trackScheduler)
	bool   hasClipWindow()        const override { return true; }
	double getClipStart()         const override { return clipStart.get(); }
	double getClipEnd()           const override { return clipEnd.get(); }
	double getClipDuration()      const override { return clipDuration.get(); }
	bool   getClipLoop()          const override { return clipLoop.get(); }
	void   setClipStart(double v)    override { clipStart.set((float)v); }
	void   setClipEnd(double v)      override { clipEnd.set((float)v); }
	void   setClipDuration(double v) override { clipDuration.set((float)ofClamp(v, 0.25, 64.0)); }
	void   setClipLoop(bool v)       override { clipLoop.set(v); }

	// Content stretching — scales all note positions/lengths by factor relative to snapshot
	void beginContentStretch() override {
		stretchSnapshot.clear();
		for(auto& n : notes) stretchSnapshot.push_back({n.startBeat, n.length});
	}
	void applyContentStretch(double factor) override {
		if(stretchSnapshot.size() != notes.size()) return;
		factor = std::max(0.01, factor);
		for(int i = 0; i < (int)notes.size(); i++) {
			notes[i].startBeat = stretchSnapshot[i].startBeat * factor;
			notes[i].length    = stretchSnapshot[i].length    * factor;
		}
	}

	// Mini content rendering for trackScheduler
	void drawMiniContent(ImDrawList* dl, ImVec2 p1, ImVec2 p2,
	                     double viewBeat0, double viewBeat1,
	                     double clipOrigin = -1.0) override
	{
		if(notes.empty()) return;
		double dur   = std::max(0.25, (double)clipDuration.get());
		double cs    = (clipOrigin >= 0.0) ? clipOrigin : (double)clipStart.get();
		bool   lp    = clipLoop.get();
		double barLen = viewBeat1 - viewBeat0;
		if(barLen <= 0.001) return;
		float W = p2.x - p1.x;
		float H = p2.y - p1.y;
		if(W <= 1.f || H <= 1.f) return;

		// Only consider notes within [0, dur) — notes beyond clipDuration are inactive
		int minP = 127, maxP = 0;
		for(auto& n : notes) {
			if(n.startBeat >= dur) continue;
			minP = std::min(minP, n.pitch); maxP = std::max(maxP, n.pitch);
		}
		if(minP > maxP) return; // no visible notes
		int pitchRange = std::max(maxP - minP, 1);

		auto localToX = [&](double localBeat, int rep) -> float {
			return p1.x + float(((cs + rep * dur + localBeat) - viewBeat0) / barLen) * W;
		};
		auto pitchToY = [&](int pitch) -> float {
			return p1.y + (1.f - float(pitch - minP) / pitchRange) * H;
		};

		int repStart = 0, repEnd = 0;
		if(lp) {
			repStart = std::max(0, (int)std::floor((viewBeat0 - cs) / dur));
			repEnd   = (int)std::ceil ((viewBeat1 - cs) / dur);
		}

		dl->PushClipRect(p1, p2, true);
		for(int rep = repStart; rep <= repEnd; rep++) {
			for(auto& n : notes) {
				if(n.startBeat >= dur) continue;               // outside clip duration
				double endBeat = std::min(n.endBeat(), dur);   // cap at clip duration
				float nx1 = localToX(n.startBeat, rep);
				float nx2 = localToX(endBeat,     rep);
				if(nx2 < p1.x || nx1 > p2.x) continue;
				float ny1   = pitchToY(n.pitch);
				float noteH = std::max(1.5f, H / (float)(pitchRange + 1));
				uint8_t a = (uint8_t)(n.velocity * 180 + 55);
				dl->AddRectFilled(ImVec2(nx1, ny1), ImVec2(nx2, ny1 + noteH),
				                  IM_COL32(170, 255, 160, a));
			}
		}
		// Loop dividers
		if(lp && W > 20) {
			for(int rep = repStart + 1; rep <= repEnd; rep++) {
				float dx = p1.x + float((cs + rep * dur - viewBeat0) / barLen) * W;
				dl->AddLine(ImVec2(dx, p1.y), ImVec2(dx, p2.y), IM_COL32(0, 200, 200, 80), 1.f);
			}
		}
		dl->PopClipRect();
	}

	// ── Main draw ─────────────────────────────────────────────────────────────
	void drawInTimeline(ImDrawList* dl, ImVec2 pos, ImVec2 sz,
	                    double viewStart, double viewEnd) override
	{
		// ── Layout ───────────────────────────────────────────────────────────
		std::string btnId = "##prBtn" + ofToString(getNumIdentifier());
		ImGui::InvisibleButton(btnId.c_str(), sz);

		ImVec2 p   = ImGui::GetItemRectMin();
		ImVec2 s   = ImGui::GetItemRectSize();
		ImVec2 ep  = ImGui::GetItemRectMax();

		const float pianoW   = s.x * 0.05f;
		const float velH     = 50.0f;
		const float probH    = 40.0f;
		const float laneGap  =  5.0f;
		const float lanesH   = velH + laneGap + probH;

		ImVec2 pianoStart(p.x,          p.y);
		ImVec2 pianoEnd  (p.x + pianoW, ep.y - lanesH);
		ImVec2 rollStart (p.x + pianoW, p.y);
		ImVec2 rollEnd   (ep.x,         ep.y - lanesH);
		ImVec2 velStart  (p.x + pianoW, ep.y - lanesH);
		ImVec2 velEnd    (ep.x,         ep.y - probH - laneGap);
		ImVec2 velLblS   (p.x,          ep.y - lanesH);
		ImVec2 velLblE   (p.x + pianoW, ep.y - probH - laneGap);
		ImVec2 probStart (p.x + pianoW, ep.y - probH);
		ImVec2 probEnd   (ep.x,         ep.y);
		ImVec2 probLblS  (p.x,          ep.y - probH);
		ImVec2 probLblE  (p.x + pianoW, ep.y);

		const float rollW = rollEnd.x - rollStart.x;
		const float rollH = rollEnd.y - rollStart.y;

		double visLen = viewEnd - viewStart;
		if(visLen <= 0.001) return;

		// ── Timeline info ─────────────────────────────────────────────────────
		int    gridTicks = 0;
		double beatsPerBar = 4.0, playhead = 0.0;
		if(currentTimeline) {
			gridTicks    = currentTimeline->getGridTicks();
			beatsPerBar  = double(currentTimeline->getNumerator()) *
			               (4.0 / double(currentTimeline->getDenominator()));
			playhead     = currentTimeline->getBeatPosition();
		}

		// ── Clip window values ────────────────────────────────────────────────
		double cs  = (double)clipStart.get();
		double ce  = (double)clipEnd.get();
		double dur = (double)clipDuration.get();
		bool   lp  = clipLoop.get();

		// ── Coordinate helpers (global beat space) ────────────────────────────
		auto beatToX = [&](double b) {
			return rollStart.x + float((b - viewStart) / visLen) * rollW;
		};
		auto xToBeat = [&](float x) {
			return viewStart + double(x - rollStart.x) / rollW * visLen;
		};
		// Local↔global helpers (notes live in local space [0, clipDuration))
		auto localToX = [&](double lb) { return beatToX(lb + cs); };
		auto xToLocal = [&](float x)   { return xToBeat(x) - cs; };

		auto pitchToY = [&](int pitch) -> float {
			if(pitch > scrollOffset.get() + visibleRange.get() - 1) return rollStart.y - 100.f;
			if(pitch < scrollOffset.get()) return rollEnd.y + 100.f;
			int loNote = scrollOffset.get();
			int hiNote = std::min(127, loNote + visibleRange.get() - 1);
			return rollStart.y + (hiNote - pitch) * (rollH / (hiNote - loNote + 1));
		};
		auto yToPitch = [&](float y) -> int {
			int loNote = scrollOffset.get();
			int hiNote = std::min(127, loNote + visibleRange.get() - 1);
			float noteH = rollH / (hiNote - loNote + 1);
			return ofClamp(hiNote - (int)((y - rollStart.y) / noteH), 0, 127);
		};
		auto snap = [&](double b) -> double {
			if(gridTicks <= 0) return b;
			double g = gridTicks / 24.0;
			return std::round(b / g) * g;
		};

		int loNote = scrollOffset.get();
		int hiNote = std::min(127, loNote + visibleRange.get() - 1);
		float noteH = rollH / (hiNote - loNote + 1);

		// ── Backgrounds ───────────────────────────────────────────────────────
		dl->AddRectFilled(pianoStart, pianoEnd,  IM_COL32(30,30,30,255));
		dl->AddRectFilled(rollStart,  rollEnd,   IM_COL32(40,40,40,255));
		dl->AddRectFilled(velLblS,    velLblE,   IM_COL32(30,30,30,255));
		dl->AddRectFilled(velStart,   velEnd,    IM_COL32(35,35,35,255));
		dl->AddRectFilled(ImVec2(p.x,         velEnd.y),
		                  ImVec2(ep.x,        probStart.y), IM_COL32(18,18,18,255));
		dl->AddRectFilled(probLblS,   probLblE,  IM_COL32(28,28,28,255));
		dl->AddRectFilled(probStart,  probEnd,   IM_COL32(33,32,28,255));
		dl->AddLine(ImVec2(p.x, probStart.y), ImVec2(ep.x, probStart.y), IM_COL32(60,60,60,255));
		dl->AddRect(p, ep, IM_COL32(60,60,60,255));
		dl->AddText(ImVec2(velLblS.x+2,  velLblS.y+16),  IM_COL32(150,150,150,255), "Vel");
		dl->AddText(ImVec2(probLblS.x+2, probLblS.y+12), IM_COL32(200,170,80,255),  "Prob");

		// ── Piano keys + note rows ────────────────────────────────────────────
		for(int pitch = loNote; pitch <= hiNote; pitch++) {
			float y1 = pitchToY(pitch);
			float y2 = std::min(pitchToY(pitch - 1), rollEnd.y);
			int   pc = pitch % 12;
			bool black = (pc==1||pc==3||pc==6||pc==8||pc==10);

			dl->AddRectFilled(ImVec2(pianoStart.x, y1),
			                  ImVec2(pianoEnd.x-1, y2),
			                  black ? IM_COL32(20,20,20,255) : IM_COL32(200,200,200,255));

			if(!black)
				dl->AddRectFilled(ImVec2(rollStart.x, y1), ImVec2(rollEnd.x, y2),
				                  IM_COL32(45,45,45,255));

			dl->AddLine(ImVec2(rollStart.x,y1), ImVec2(rollEnd.x,y1),
			            IM_COL32(60,60,60,255), 0.5f);

			if(pc == 0) {
				char buf[8]; snprintf(buf,8,"C%d",(pitch/12)-1);
				dl->AddText(ImVec2(pianoStart.x+2,y1+2), IM_COL32(100,100,100,255), buf);
				dl->AddLine(ImVec2(rollStart.x,y1), ImVec2(rollEnd.x,y1),
				            IM_COL32(80,80,80,255), 1.0f);
			}
		}

		// ── Grid lines ────────────────────────────────────────────────────────
		int vSBar = (int)(viewStart / beatsPerBar);
		int vEBar = (int)(viewEnd   / beatsPerBar) + 1;
		for(int bar = vSBar; bar <= vEBar; bar++) {
			double bb = bar * beatsPerBar;
			float  bx = beatToX(bb);
			if(bx < rollStart.x-5 || bx > rollEnd.x+5) continue;
			dl->AddLine(ImVec2(bx,rollStart.y),ImVec2(bx,rollEnd.y),
			            IM_COL32(120,120,120,255),2.f);
			dl->AddLine(ImVec2(bx,velStart.y),ImVec2(bx,velEnd.y),
			            IM_COL32(80,80,80,255),1.f);
			dl->AddLine(ImVec2(bx,probStart.y),ImVec2(bx,probEnd.y),
			            IM_COL32(70,70,60,255),1.f);
			if(gridTicks > 0 && bar < vEBar) {
				double g = gridTicks / 24.0;
				double nb = (bar+1)*beatsPerBar;
				for(double b = bb+g; b < nb; b += g) {
					if(b < viewStart || b > viewEnd) continue;
					float gx = beatToX(b);
					dl->AddLine(ImVec2(gx,rollStart.y),ImVec2(gx,rollEnd.y),
					            IM_COL32(70,70,70,100),0.5f);
					dl->AddLine(ImVec2(gx,velStart.y),ImVec2(gx,velEnd.y),
					            IM_COL32(60,60,60,100),0.5f);
					dl->AddLine(ImVec2(gx,probStart.y),ImVec2(gx,probEnd.y),
					            IM_COL32(55,55,45,100),0.5f);
				}
			}
		}

		// ── Loop region ───────────────────────────────────────────────────────
		if(currentTimeline) {
			double ls, le; bool len;
			if(getLoopRegion(ls, le, len) && len) {
				float lx1 = std::max(beatToX(ls), rollStart.x);
				float lx2 = std::min(beatToX(le), rollEnd.x);
				dl->AddRectFilled(ImVec2(lx1,rollStart.y),ImVec2(lx2,rollEnd.y),
				                  IM_COL32(80,80,160,50));
				dl->AddRectFilled(ImVec2(lx1,velStart.y), ImVec2(lx2,velEnd.y),
				                  IM_COL32(80,80,160,50));
				dl->AddRectFilled(ImVec2(lx1,probStart.y),ImVec2(lx2,probEnd.y),
				                  IM_COL32(80,80,160,50));
				dl->AddLine(ImVec2(lx1,rollStart.y),ImVec2(lx1,probEnd.y),
				            IM_COL32(160,160,255,180),2.f);
				dl->AddLine(ImVec2(lx2,rollStart.y),ImVec2(lx2,probEnd.y),
				            IM_COL32(160,160,255,180),2.f);
			}
		}

		// ── Draw notes ────────────────────────────────────────────────────────
		// Notes are in local beat space; drawn at local+clipStart (+ n*clipDuration for loops).
		// rep=0: full colours + selection; rep>0: ghost copies of loops.
		{
			int maxReps = (lp && dur > 0.001)
			              ? std::max(1, (int)((ce - cs) / dur) + 2)
			              : 1;

			for(int rep = 0; rep < maxReps; rep++) {
				double offset = cs + rep * (lp ? dur : 0.0);
				if(lp && offset >= ce) break;

				// Clip-rect to this repetition's time slot
				float slotX1 = std::max(beatToX(offset),          rollStart.x);
				float slotX2 = std::min(beatToX(std::min(offset + dur, ce)), rollEnd.x);
				if(!lp) slotX2 = rollEnd.x;   // no slot clipping when not looping
				if(slotX1 >= slotX2) continue;

				dl->PushClipRect(ImVec2(slotX1, rollStart.y),
				                 ImVec2(slotX2, rollEnd.y), true);

				for(size_t i = 0; i < notes.size(); i++) {
					const MidiNote& n = notes[i];
					if(n.pitch < loNote || n.pitch > hiNote) continue;

					float x1 = beatToX(n.startBeat + offset);
					float x2 = beatToX(n.endBeat()  + offset);
					if(x2 < rollStart.x || x1 > rollEnd.x) continue;

					float dx1 = std::max(x1, rollStart.x);
					float dx2 = std::min(x2, rollEnd.x);
					float dy1 = std::max(pitchToY(n.pitch),   rollStart.y);
					float dy2 = std::min(pitchToY(n.pitch-1), rollEnd.y);

					bool  sel = (rep == 0) && selectedNotes.count((int)i) > 0;
					ImU32 col = sel       ? IM_COL32(180,255,180,255)
					          : (rep == 0) ? IM_COL32(120,200,120,220)
					                       : IM_COL32(100,170,100,110);

					dl->AddRectFilled(ImVec2(dx1+1,dy1+1), ImVec2(dx2-1,dy2-1), col, 2.f);
					if(rep == 0) {
						dl->AddRect(ImVec2(dx1+1,dy1+1), ImVec2(dx2-1,dy2-1),
						            IM_COL32(80,80,80,255), 2.f, 0, 1.5f);
						if(x2 >= rollStart.x && x2 <= rollEnd.x)
							dl->AddLine(ImVec2(x2-2,dy1+2), ImVec2(x2-2,dy2-2),
							            IM_COL32(255,255,255,60), 2.f);
					}
				}

				dl->PopClipRect();
			}
		}

		// ── Ghost copies during alt+drag ──────────────────────────────────────
		if(isDraggingNote && altHeldAtDragStart) {
			for(const MidiNote& cn : copyBuffer) {
				if(cn.pitch < loNote || cn.pitch > hiNote) continue;
				float x1 = localToX(cn.startBeat);
				float x2 = localToX(cn.endBeat());
				if(x2 < rollStart.x || x1 > rollEnd.x) continue;
				float dx1 = std::max(x1, rollStart.x);
				float dx2 = std::min(x2, rollEnd.x);
				float dy1 = std::max(pitchToY(cn.pitch),   rollStart.y);
				float dy2 = std::min(pitchToY(cn.pitch-1), rollEnd.y);
				dl->AddRectFilled(ImVec2(dx1+1,dy1+1), ImVec2(dx2-1,dy2-1),
				                  IM_COL32(180,255,180,100), 2.f);
			}
		}

		// ── Preview note while creating ───────────────────────────────────────
		if(ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isCreatingNote) {
			double cb  = snap(xToLocal(ImGui::GetMousePos().x));
			double st  = std::min(dragStartBeat, cb);
			double en  = std::max(dragStartBeat, cb);
			float  px1 = localToX(st), px2 = localToX(en);
			float  py1 = pitchToY(dragStartPitch), py2 = pitchToY(dragStartPitch-1);
			if(dragStartPitch >= loNote && dragStartPitch <= hiNote)
				dl->AddRectFilled(ImVec2(px1+1,py1+1), ImVec2(px2-1,py2-1),
				                  IM_COL32(120,200,120,120), 2.f);
			float vbh = (velEnd.y-velStart.y)*0.8f;
			dl->AddLine(ImVec2(px1,velEnd.y-1), ImVec2(px1,velEnd.y-vbh),
			            IM_COL32(100,200,100,120), 2.f);
			dl->AddCircleFilled(ImVec2(px1, velEnd.y-vbh), 3.5f,
			                    IM_COL32(100,200,100,120));
			dl->AddLine(ImVec2(px1,probEnd.y-1), ImVec2(px1,probStart.y),
			            IM_COL32(220,170,50,120), 2.f);
			dl->AddCircleFilled(ImVec2(px1, probStart.y), 3.5f,
			                    IM_COL32(220,170,50,120));
		}

		// ── Velocity lines ────────────────────────────────────────────────────
		for(size_t i = 0; i < notes.size(); i++) {
			float velX = localToX(notes[i].startBeat);
			if(velX < velStart.x-2 || velX > velEnd.x+2) continue;

			float vbh  = (velEnd.y - velStart.y) * notes[i].velocity;
			float topY = velEnd.y - vbh;

			bool sel = selectedNotes.count((int)i) > 0;
			uint8_t br = (uint8_t)(100 + notes[i].velocity * 155);
			ImU32 vc = sel ? IM_COL32(180,255,180,255) : IM_COL32(100,br,100,220);

			dl->AddLine(ImVec2(velX, velEnd.y-1), ImVec2(velX, topY), vc, 2.f);
			dl->AddCircleFilled(ImVec2(velX, topY), 3.5f, vc);
		}

		// ── Probability lines ─────────────────────────────────────────────────
		for(size_t i = 0; i < notes.size(); i++) {
			float probX = localToX(notes[i].startBeat);
			if(probX < probStart.x-2 || probX > probEnd.x+2) continue;

			float pbh  = (probEnd.y - probStart.y) * notes[i].probability;
			float topY = probEnd.y - pbh;

			bool    sel = selectedNotes.count((int)i) > 0;
			uint8_t br  = (uint8_t)(80 + notes[i].probability * 175);
			ImU32   pc  = sel ? IM_COL32(255,230,120,255) : IM_COL32(br,br/2,30,220);

			dl->AddLine(ImVec2(probX, probEnd.y-1), ImVec2(probX, topY), pc, 2.f);
			dl->AddCircleFilled(ImVec2(probX, topY), 3.5f, pc);
		}

		// ── Rubber-band selection rect ────────────────────────────────────────
		if(isRubberBanding && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ImVec2 m = ImGui::GetMousePos();
			float rx1 = std::min(rubberBandX1, m.x);
			float rx2 = std::max(rubberBandX1, m.x);
			float ry1 = std::min(rubberBandY1, m.y);
			float ry2 = std::max(rubberBandY1, m.y);
			dl->AddRectFilled(ImVec2(rx1,ry1), ImVec2(rx2,ry2), IM_COL32(100,150,255,30));
			dl->AddRect      (ImVec2(rx1,ry1), ImVec2(rx2,ry2), IM_COL32(100,150,255,200), 0.f, 0, 1.f);
		}

		// ── Playhead ──────────────────────────────────────────────────────────
		float phX = beatToX(playhead);
		if(phX >= rollStart.x && phX <= rollEnd.x) {
			dl->AddLine(ImVec2(phX,rollStart.y), ImVec2(phX,rollEnd.y),
			            IM_COL32(255,80,80,255), 2.5f);
			dl->AddLine(ImVec2(phX,velStart.y),  ImVec2(phX,velEnd.y),
			            IM_COL32(255,80,80,255), 2.5f);
		}

		// ── Clip inactive region overlays (drawn over notes to dim them) ───────
		{
			float csX = beatToX(cs);
			float ceX = beatToX(ce);
			// Before clipStart
			if(csX > rollStart.x) {
				float ox2 = std::min(csX, rollEnd.x);
				dl->AddRectFilled(ImVec2(rollStart.x,rollStart.y), ImVec2(ox2,rollEnd.y),
				                  IM_COL32(0,0,0,115));
				dl->AddRectFilled(ImVec2(velStart.x,velStart.y),
				                  ImVec2(std::min(csX,velEnd.x),velEnd.y), IM_COL32(0,0,0,115));
				dl->AddRectFilled(ImVec2(probStart.x,probStart.y),
				                  ImVec2(std::min(csX,probEnd.x),probEnd.y), IM_COL32(0,0,0,115));
			}
			// After clipEnd
			if(ceX < rollEnd.x) {
				float ox1 = std::max(ceX, rollStart.x);
				dl->AddRectFilled(ImVec2(ox1,rollStart.y), ImVec2(rollEnd.x,rollEnd.y),
				                  IM_COL32(0,0,0,115));
				dl->AddRectFilled(ImVec2(std::max(ceX,velStart.x),velStart.y),
				                  ImVec2(velEnd.x,velEnd.y), IM_COL32(0,0,0,115));
				dl->AddRectFilled(ImVec2(std::max(ceX,probStart.x),probStart.y),
				                  ImVec2(probEnd.x,probEnd.y), IM_COL32(0,0,0,115));
			}
		}

		// ── Loop repeat dividers ──────────────────────────────────────────────
		if(lp && dur > 0.001) {
			for(double lb = cs + dur; lb < ce; lb += dur) {
				float lx = beatToX(lb);
				if(lx < rollStart.x || lx > rollEnd.x) continue;
				dl->AddLine(ImVec2(lx,rollStart.y), ImVec2(lx,rollEnd.y),
				            IM_COL32(100,200,255,150), 1.5f);
				dl->AddLine(ImVec2(lx,velStart.y), ImVec2(lx,velEnd.y),
				            IM_COL32(100,200,255,100), 1.f);
			}
		}

		// ── Clip handles ──────────────────────────────────────────────────────
		{
			float csX = beatToX(cs);
			float ceX = beatToX(ce);

			// clipStart handle (green, arrow pointing right)
			if(csX >= rollStart.x - CLIP_HANDLE_ZONE && csX <= rollEnd.x + CLIP_HANDLE_ZONE) {
				ImU32 col = draggingClipStart ? IM_COL32(150,255,150,255) : IM_COL32(100,240,130,220);
				dl->AddLine(ImVec2(csX, p.y), ImVec2(csX, probEnd.y), col, 2.f);
				dl->AddTriangleFilled(ImVec2(csX, p.y+2),
				                      ImVec2(csX+9.f, p.y+9.f),
				                      ImVec2(csX, p.y+16.f), col);
			}
			// clipEnd handle (orange, arrow pointing left)
			if(ceX >= rollStart.x - CLIP_HANDLE_ZONE && ceX <= rollEnd.x + CLIP_HANDLE_ZONE) {
				ImU32 col = draggingClipEnd ? IM_COL32(255,200,100,255) : IM_COL32(255,165,60,220);
				dl->AddLine(ImVec2(ceX, p.y), ImVec2(ceX, probEnd.y), col, 2.f);
				dl->AddTriangleFilled(ImVec2(ceX, p.y+2),
				                      ImVec2(ceX-9.f, p.y+9.f),
				                      ImVec2(ceX, p.y+16.f), col);
			}
		}

		// ── Interactions ──────────────────────────────────────────────────────
		ImVec2 mouse = ImGui::GetMousePos();
		bool   hover = ImGui::IsItemHovered();
		bool   lClick   = hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool   rClick   = hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
		bool   dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
		bool   released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

		bool cmdHeld = ImGui::GetIO().KeySuper;
		bool altHeld = ImGui::GetIO().KeyAlt;

		bool inRoll = (mouse.x >= rollStart.x && mouse.x <= rollEnd.x &&
		               mouse.y >= rollStart.y && mouse.y <= rollEnd.y);
		bool inVel  = (mouse.x >= velStart.x  && mouse.x <= velEnd.x  &&
		               mouse.y >= velStart.y  && mouse.y <= velEnd.y);
		bool inProb = (mouse.x >= probStart.x && mouse.x <= probEnd.x &&
		               mouse.y >= probStart.y && mouse.y <= probEnd.y);

		// ── Clip handle interaction (takes priority) ──────────────────────────
		{
			float csX = beatToX(cs);
			float ceX = beatToX(ce);
			bool nearStart = std::abs(mouse.x - csX) <= CLIP_HANDLE_ZONE;
			bool nearEnd   = std::abs(mouse.x - ceX) <= CLIP_HANDLE_ZONE;

			if(hover && (nearStart || nearEnd) &&
			   !isDraggingNote && !isResizingNote && !isCreatingNote)
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

			if(lClick && (nearStart || nearEnd)) {
				if(nearStart) draggingClipStart = true;
				else          draggingClipEnd   = true;
			}
			if(dragging) {
				if(draggingClipStart) {
					float nv = ofClamp((float)snap(xToBeat(mouse.x)), 0.f, (float)ce - 0.25f);
					clipStart.set(nv);
				}
				if(draggingClipEnd) {
					float nv = ofClamp((float)snap(xToBeat(mouse.x)), (float)cs + 0.25f, 999.f);
					clipEnd.set(nv);
				}
			}
			if(released) { draggingClipStart = false; draggingClipEnd = false; }

			// Skip note interactions while dragging a handle
			if(draggingClipStart || draggingClipEnd) return;
		}

		// Hover cursor for resize handle
		if(hover && !isDraggingNote && !isResizingNote && !isDraggingVelocity && inRoll) {
			for(int i = (int)notes.size()-1; i >= 0; i--) {
				if(notes[i].pitch < loNote || notes[i].pitch > hiNote) continue;
				float x2 = localToX(notes[i].endBeat());
				float y1 = pitchToY(notes[i].pitch);
				float y2 = pitchToY(notes[i].pitch-1);
				if(mouse.x >= x2-RESIZE_ZONE && mouse.x <= x2+2.f &&
				   mouse.y >= y1 && mouse.y < y2) {
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
					break;
				}
			}
		}

		if(lClick) {
			if(inRoll) {
				int hitResize = -1, hitBody = -1;

				for(int i = (int)notes.size()-1; i >= 0; i--) {
					if(notes[i].pitch < loNote || notes[i].pitch > hiNote) continue;
					float x1 = localToX(notes[i].startBeat);
					float x2 = localToX(notes[i].endBeat());
					float y1 = pitchToY(notes[i].pitch);
					float y2 = pitchToY(notes[i].pitch-1);
					if(mouse.y < y1 || mouse.y >= y2) continue;

					if(mouse.x >= x2-RESIZE_ZONE && mouse.x <= x2+2.f) {
						hitResize = i; break;
					}
					if(mouse.x >= x1 && mouse.x < x2 && hitBody == -1)
						hitBody = i;
				}

				if(hitResize >= 0) {
					isResizingNote = true;
					resizeNoteIdx  = hitResize;
				} else if(hitBody >= 0) {
					if(cmdHeld) {
						if(selectedNotes.count(hitBody)) selectedNotes.erase(hitBody);
						else selectedNotes.insert(hitBody);
					} else {
						if(!selectedNotes.count(hitBody)) {
							selectedNotes.clear();
							selectedNotes.insert(hitBody);
						}
					}
					isDraggingNote    = true;
					altHeldAtDragStart = altHeld;
					dragRefBeat       = xToBeat(mouse.x);   // global, delta-only
					dragRefPitch      = yToPitch(mouse.y);
					noteDragStates.clear();
					for(int idx : selectedNotes)
						noteDragStates.push_back({notes[idx].startBeat, notes[idx].pitch});

					if(altHeld) {
						copyBuffer.clear();
						for(int idx : selectedNotes) copyBuffer.push_back(notes[idx]);
					}
				} else {
					if(cmdHeld) {
						isRubberBanding = true;
						rubberBandX1    = mouse.x;
						rubberBandY1    = mouse.y;
					} else {
						selectedNotes.clear();
						isCreatingNote = true;
						dragStartBeat  = snap(xToLocal(mouse.x));  // LOCAL beat
						dragStartPitch = yToPitch(mouse.y);
					}
				}
			} else if(inVel) {
				int   nearest = -1;
				float minDist = FLT_MAX;
				for(int i = 0; i < (int)notes.size(); i++) {
					float d = std::abs(mouse.x - localToX(notes[i].startBeat));
					if(d < minDist) { minDist = d; nearest = i; }
				}
				if(nearest >= 0) {
					velDragNote        = nearest;
					isDraggingVelocity = true;
				}
			} else if(inProb) {
				int   nearest = -1;
				float minDist = FLT_MAX;
				for(int i = 0; i < (int)notes.size(); i++) {
					float d = std::abs(mouse.x - localToX(notes[i].startBeat));
					if(d < minDist) { minDist = d; nearest = i; }
				}
				if(nearest >= 0) {
					probDragNote          = nearest;
					isDraggingProbability = true;
				}
			}
		}

		if(dragging) {
			if(isResizingNote && resizeNoteIdx >= 0 &&
			   resizeNoteIdx < (int)notes.size())
			{
				double newEnd = snap(xToLocal(mouse.x));   // LOCAL beat
				double minLen = gridTicks > 0 ? gridTicks/24.0 : 0.0625;
				double newLen = std::max(newEnd - notes[resizeNoteIdx].startBeat, minLen);
				notes[resizeNoteIdx].length = (float)newLen;
			}
			else if(isDraggingNote && !selectedNotes.empty()) {
				// Delta in global beats == delta in local beats (cs cancels)
				double db = snap(xToBeat(mouse.x)) - snap(dragRefBeat);
				int    dp = yToPitch(mouse.y) - dragRefPitch;

				if(altHeldAtDragStart) {
					int j = 0;
					for(int idx : selectedNotes) {
						copyBuffer[j].startBeat = noteDragStates[j].origBeat + db;
						copyBuffer[j].pitch     = ofClamp(noteDragStates[j].origPitch + dp, 0, 127);
						j++;
					}
				} else {
					int j = 0;
					for(int idx : selectedNotes) {
						notes[idx].startBeat = noteDragStates[j].origBeat + db;
						notes[idx].pitch     = ofClamp(noteDragStates[j].origPitch + dp, 0, 127);
						j++;
					}
				}
			}
			else if(isDraggingVelocity && velDragNote >= 0 &&
			        velDragNote < (int)notes.size())
			{
				float pct = 1.f - (mouse.y - velStart.y) / (velEnd.y - velStart.y);
				notes[velDragNote].velocity = ofClamp(pct, 0.f, 1.f);
			}
			else if(isDraggingProbability && probDragNote >= 0 &&
			        probDragNote < (int)notes.size())
			{
				float pct = 1.f - (mouse.y - probStart.y) / (probEnd.y - probStart.y);
				notes[probDragNote].probability = ofClamp(pct, 0.f, 1.f);
			}
		}

		if(released) {
			if(isRubberBanding) {
				ImVec2 m = ImGui::GetMousePos();
				float rx1 = std::min(rubberBandX1, m.x);
				float rx2 = std::max(rubberBandX1, m.x);
				float ry1 = std::min(rubberBandY1, m.y);
				float ry2 = std::max(rubberBandY1, m.y);
				for(int i = 0; i < (int)notes.size(); i++) {
					if(notes[i].pitch < loNote || notes[i].pitch > hiNote) continue;
					float nx1 = localToX(notes[i].startBeat);
					float nx2 = localToX(notes[i].endBeat());
					float ny1 = pitchToY(notes[i].pitch);
					float ny2 = pitchToY(notes[i].pitch - 1);
					if(nx2 >= rx1 && nx1 <= rx2 && ny2 >= ry1 && ny1 <= ry2)
						selectedNotes.insert(i);
				}
				isRubberBanding = false;
			}

			if(isResizingNote) {
				isResizingNote = false;
				resizeNoteIdx  = -1;
			}

			if(isDraggingNote) {
				if(altHeldAtDragStart) {
					for(auto& cn : copyBuffer) notes.push_back(cn);
					copyBuffer.clear();
				}
				std::sort(notes.begin(), notes.end());
				isDraggingNote = false;
				noteDragStates.clear();
			}

			if(isCreatingNote) {
				double eb  = snap(xToLocal(mouse.x));    // LOCAL beat
				double st  = std::min(dragStartBeat, eb);
				double en  = std::max(dragStartBeat, eb);
				if(en - st > 0.001) {
					MidiNote nw;
					nw.startBeat = st;
					nw.length    = (float)(en - st);
					nw.pitch     = dragStartPitch;
					nw.velocity  = 0.8f;
					notes.push_back(nw);
					std::sort(notes.begin(), notes.end());
				}
				isCreatingNote = false;
			}

			isDraggingVelocity    = false;
			velDragNote           = -1;
			isDraggingProbability = false;
			probDragNote          = -1;
		}

		// Delete/Backspace key removes all selected notes
		if(!selectedNotes.empty() &&
		   (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)))
		{
			std::vector<int> toDelete(selectedNotes.rbegin(), selectedNotes.rend());
			for(int idx : toDelete)
				notes.erase(notes.begin() + idx);
			selectedNotes.clear();
		}

		// Right-click delete
		if(rClick) {
			if(inRoll) {
				for(int i = (int)notes.size()-1; i >= 0; i--) {
					if(notes[i].pitch < loNote || notes[i].pitch > hiNote) continue;
					float x1 = localToX(notes[i].startBeat);
					float x2 = localToX(notes[i].endBeat());
					float y1 = pitchToY(notes[i].pitch);
					float y2 = pitchToY(notes[i].pitch-1);
					if(mouse.x >= x1 && mouse.x <= x2 &&
					   mouse.y >= y1 && mouse.y < y2) {
						selectedNotes.erase(i);
						notes.erase(notes.begin() + i);
						std::set<int> fixed;
						for(int s : selectedNotes) fixed.insert(s > i ? s-1 : s);
						selectedNotes = fixed;
						break;
					}
				}
			} else if(inVel) {
				for(int i = (int)notes.size()-1; i >= 0; i--) {
					float vx = localToX(notes[i].startBeat);
					if(std::abs(mouse.x - vx) <= VEL_HIT_ZONE) {
						selectedNotes.erase(i);
						notes.erase(notes.begin() + i);
						std::set<int> fixed;
						for(int s : selectedNotes) fixed.insert(s > i ? s-1 : s);
						selectedNotes = fixed;
						break;
					}
				}
			}
		}
	}

	// ── Preset ────────────────────────────────────────────────────────────────
	void presetSave(ofJson &json) override {
		std::vector<std::vector<float>> nl;
		for(auto &n : notes)
			nl.push_back({(float)n.startBeat, n.length, (float)n.pitch, n.velocity, n.probability});
		json["notes"]       = nl;
		json["trackHeight"] = trackHeight;
		json["collapsed"]   = collapsed;
		saveExtraClips(json);
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("notes")) {
			notes.clear();
			for(auto &d : json["notes"]) {
				if(d.size() >= 4) {
					MidiNote n;
					n.startBeat = d[0]; n.length = d[1];
					n.pitch = (int)d[2]; n.velocity = d[3];
					n.probability = d.size() >= 5 ? (float)d[4] : 1.0f;
					notes.push_back(n);
				}
			}
		}
		if(json.count("trackHeight")) trackHeight = ofClamp((float)json["trackHeight"], MIN_H, MAX_H);
		if(json.count("collapsed"))   collapsed = json["collapsed"];
		loadExtraClips(json);
	}

private:
	// ── Parameters ────────────────────────────────────────────────────────────
	ofParameter<int>           timelineSelect;
	ofParameter<std::string>   trackName;
	ofParameter<int>           scrollOffset;
	ofParameter<int>           visibleRange;
	ofParameter<float>         clipStart;
	ofParameter<float>         clipEnd;
	ofParameter<float>         clipDuration;
	ofParameter<bool>          clipLoop;
	ofParameter<vector<float>> pitchesOutput, velocitiesOutput, gatesOutput;
	ofParameter<int>           numActiveOutput;

	ppqTimeline*              currentTimeline = nullptr;
	std::vector<MidiNote>     notes;
	struct NoteSnapshot { double startBeat; float length; };
	std::vector<NoteSnapshot> stretchSnapshot;
	std::vector<std::string>  timelineOptions;
	ofEventListeners          listeners;

	// ── Interaction state ─────────────────────────────────────────────────────
	std::set<int> selectedNotes;

	// Note creation
	bool   isCreatingNote  = false;
	double dragStartBeat   = 0.0;   // LOCAL beat
	int    dragStartPitch  = 60;

	// Note move
	bool   isDraggingNote      = false;
	bool   altHeldAtDragStart  = false;
	double dragRefBeat         = 0.0;   // GLOBAL beat (delta-only)
	int    dragRefPitch        = 0;
	struct DragState { double origBeat; int origPitch; };
	std::vector<DragState> noteDragStates;

	// Alt+drag copy
	std::vector<MidiNote> copyBuffer;

	// Right-edge resize
	bool isResizingNote = false;
	int  resizeNoteIdx  = -1;
	static constexpr float RESIZE_ZONE = 6.0f;

	// Velocity drag
	bool isDraggingVelocity = false;
	int  velDragNote        = -1;
	static constexpr float VEL_HIT_ZONE = 8.0f;

	// Probability drag
	bool isDraggingProbability = false;
	int  probDragNote          = -1;

	// Gate probability roll state
	using NoteKey = std::pair<double, int>;
	std::map<NoteKey, bool> gateRollResults;
	std::set<NoteKey>       prevActiveNotes;

	// Rubber-band selection
	bool  isRubberBanding = false;
	float rubberBandX1    = 0.f;
	float rubberBandY1    = 0.f;

	// Clip handle drag state
	bool  draggingClipStart = false;
	bool  draggingClipEnd   = false;
	static constexpr float CLIP_HANDLE_ZONE = 8.0f;

	// Track geometry
	float trackHeight = 260.0f;
	bool  collapsed   = false;
	static constexpr float MIN_H = 160.0f;
	static constexpr float MAX_H = 600.0f;

	// ── Helpers ───────────────────────────────────────────────────────────────
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

	bool getLoopRegion(double& ls, double& le, bool& en) {
		if(!currentTimeline) return false;
		en = currentTimeline->isLoopEnabled();
		ls = currentTimeline->getLoopStart();
		le = currentTimeline->getLoopEnd();
		return true;
	}
};

#endif
