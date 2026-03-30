#ifndef transportTrack_h
#define transportTrack_h

#include "imgui.h"
#include "ofMain.h"

class transportTrack {
public:
	virtual ~transportTrack() {}

	// The timeline calls this to ask the track to draw itself
	// pos: Top-left position (for reference, but track should use GetCursorScreenPos)
	// size: Width and height allocated for this track
	// startBeat, endBeat: The current timeline zoom view
	virtual void drawInTimeline(ImDrawList* dl, ImVec2 pos, ImVec2 size, double startBeat, double endBeat) = 0;
	
	// Identification
	virtual std::string getTrackName() = 0;
	
	// Height management
	virtual float getHeight() const = 0;
	virtual void setHeight(float h) = 0;
	
	// Collapse state
	virtual bool isCollapsed() const = 0;
	virtual void setCollapsed(bool collapsed) = 0;

	// Optional clip-window interface (override in tracks that support it)
	virtual bool   hasClipWindow()        const { return false; }
	virtual double getClipStart()         const { return 0.0; }
	virtual double getClipEnd()           const { return 9999.0; }
	virtual double getClipDuration()      const { return 4.0; }
	virtual bool   getClipLoop()          const { return false; }
	virtual void   setClipStart(double)   {}
	virtual void   setClipEnd(double)     {}
	virtual void   setClipDuration(double){}
	virtual void   setClipLoop(bool)      {}

	// Multi-clip interface — clip 0 maps to the scalar clipStart/clipEnd params;
	// extra clips are stored in the protected extraClips vector.
	virtual int    getClipCount() const {
		return hasClipWindow() ? 1 + (int)extraClips.size() : 0;
	}
	virtual double getClipStartAt(int idx) const {
		if(idx == 0) return getClipStart();
		int ei = idx - 1;
		return (ei < (int)extraClips.size()) ? extraClips[ei].first : 0.0;
	}
	virtual double getClipEndAt(int idx) const {
		if(idx == 0) return getClipEnd();
		int ei = idx - 1;
		return (ei < (int)extraClips.size()) ? extraClips[ei].second : 9999.0;
	}
	virtual void setClipStartAt(int idx, double v) {
		if(idx == 0) { setClipStart(v); return; }
		int ei = idx - 1;
		if(ei < (int)extraClips.size()) extraClips[ei].first = (float)v;
	}
	virtual void setClipEndAt(int idx, double v) {
		if(idx == 0) { setClipEnd(v); return; }
		int ei = idx - 1;
		if(ei < (int)extraClips.size()) extraClips[ei].second = (float)v;
	}
	// Add a new clip; returns the new clip's index.
	virtual int addClip(double start, double end) {
		extraClips.push_back({(float)start, (float)end});
		return (int)extraClips.size(); // idx = extraClips.size() because clip 0 is primary
	}
	// Remove clip at idx (idx 0 = primary clip, cannot be removed).
	virtual void removeClip(int idx) {
		int ei = idx - 1;
		if(ei >= 0 && ei < (int)extraClips.size())
			extraClips.erase(extraClips.begin() + ei);
	}

protected:
	// Extra clip windows [start, end] in global beat space.
	// Clip 0 is always the primary clipStart/clipEnd parameters.
	std::vector<std::pair<float,float>> extraClips;

	// Helpers for preset serialization of extra clips.
	void saveExtraClips(ofJson& json) const {
		if(extraClips.empty()) return;
		ofJson ec = ofJson::array();
		for(auto& p : extraClips) ec.push_back({p.first, p.second});
		json["extraClips"] = ec;
	}
	void loadExtraClips(const ofJson& json) {
		extraClips.clear();
		if(json.count("extraClips")) {
			for(auto& e : json["extraClips"])
				if(e.size() >= 2) extraClips.push_back({(float)e[0], (float)e[1]});
		}
	}

public:

	// Content stretching: call beginContentStretch() before drag, then
	// applyContentStretch(factor) each frame with factor = newDur/originalDur.
	virtual void   beginContentStretch()           {}
	virtual void   applyContentStretch(double /*factor*/) {}

	// Mini-content rendering — called by trackScheduler inside the clip bar rect.
	// p1/p2 = screen-space bounding box of the clip bar.
	// viewBeat0/viewBeat1 = global beat range that maps to p1.x … p2.x.
	// clipOrigin = global beat of this specific clip's start (-1 = use the track's primary clipStart).
	virtual void drawMiniContent(ImDrawList* /*dl*/, ImVec2 /*p1*/, ImVec2 /*p2*/,
	                             double /*viewBeat0*/, double /*viewBeat1*/,
	                             double /*clipOrigin*/ = -1.0) {}
};

#endif
