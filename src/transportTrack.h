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
};

#endif
