#pragma once
#include "ofxOceanodeNodeModel.h"
#include <cmath>

class vectorDeglitch : public ofxOceanodeNodeModel {
public:
	vectorDeglitch() : ofxOceanodeNodeModel("vectorDeglitch") {}

	void setup() override {
		// 0..1 input
		addParameter(input.set("Input", {0.0f}, {0.0f}, {1.0f}));

		// per-point change threshold: above this, the point is "suspicious"
		addParameter(pointThresh.set("PointThresh", 0.10f, 0.0f, 1.0f));

		// how many bad points (fraction 0..1) make the whole frame bad
		// e.g. 0.35 → if >35% of indices jump, we reject the frame
		addParameter(badRatio.set("BadRatio", 0.35f, 0.0f, 1.0f));

		// when a point is suspicious but frame is OK, blend to previous
		addParameter(blend.set("Blend", 1.0f, 0.0f, 1.0f)); // 1 = keep old

		// flat detector (all zeros or very small range)
		addParameter(flatEps.set("FlatEps", 0.0001f, 0.0f, 0.01f));

		// how many frames to keep outputting last good after a bad frame
		addParameter(glitchHold.set("Hold", 2, 0, 6));

		addOutputParameter(clean.set("Clean", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(glitched.set("Glitched", 0, 0, 1));

		listener = input.newListener([this](std::vector<float> &v){
			process(v);
		});

		firstFrame = true;
		waitingForResync = false;
		holdCounter = 0;
	}

private:
	// params
	ofParameter<std::vector<float>> input;
	ofParameter<float> pointThresh;
	ofParameter<float> badRatio;
	ofParameter<float> blend;
	ofParameter<float> flatEps;
	ofParameter<int>   glitchHold;

	ofParameter<std::vector<float>> clean;
	ofParameter<int> glitched;

	// state
	std::vector<float> lastGood;    // last accepted frame
	std::vector<float> prevRaw;     // last raw frame
	bool firstFrame;
	bool waitingForResync;
	int  holdCounter;
	ofEventListener listener;

	bool isFlat(const std::vector<float> &v, float eps){
		if(v.empty()) return true;
		float minv = v[0], maxv = v[0];
		for(float f : v){
			if(f < minv) minv = f;
			if(f > maxv) maxv = f;
		}
		return (maxv - minv) < eps;
	}

	void process(const std::vector<float> &inVec){
		// 1) first frame
		if(firstFrame){
			clean = inVec;
			lastGood = inVec;
			prevRaw  = inVec;
			glitched = 0;
			waitingForResync = isFlat(inVec, flatEps.get());
			firstFrame = false;
			return;
		}

		// 2) size changed → accept and reset
		if(inVec.size() != lastGood.size()){
			clean = inVec;
			lastGood = inVec;
			prevRaw  = inVec;
			glitched = 0;
			waitingForResync = isFlat(inVec, flatEps.get());
			holdCounter = 0;
			return;
		}

		// now sizes match
		const std::vector<float> &cur = inVec;
		size_t n = cur.size();

		bool flatNow = isFlat(cur, flatEps.get());

		// 3) flat → accept but enter resync
		if(flatNow){
			clean = cur;
			lastGood = cur;
			prevRaw  = cur;
			glitched = 0;
			waitingForResync = true;
			holdCounter = 0;
			return;
		}

		// 4) resync: first non-flat after flat is accepted
		if(waitingForResync){
			clean = cur;
			lastGood = cur;
			prevRaw  = cur;
			glitched = 0;
			waitingForResync = false;
			holdCounter = 0;
			return;
		}

		// 5) if we are in hold, just keep last good
		if(holdCounter > 0){
			clean = lastGood;
			glitched = 1;
			holdCounter--;
			prevRaw = cur;
			return;
		}

		// 6) per-index deviation counting
		float ptTh = pointThresh.get();
		int badCount = 0;
		for(size_t i = 0; i < n; ++i){
			if(std::fabs(cur[i] - lastGood[i]) > ptTh){
				badCount++;
			}
		}

		float ratio = (n > 0) ? (float)badCount / (float)n : 0.0f;

		// 7) if too many indices changed → frame is bad
		if(ratio > badRatio.get()){
			clean = lastGood;               // keep old
			glitched = 1;
			holdCounter = glitchHold.get(); // swallow next 1–3 frames
			prevRaw = cur;
			return;
		}

		// 8) otherwise frame is OK, but we still deglitch per-point
		std::vector<float> out(n);
		float b = blend.get();
		for(size_t i = 0; i < n; ++i){
			float c = cur[i];
			float o = lastGood[i];
			float d = std::fabs(c - o);
			if(d > ptTh){
				// suspicious point → blend
				c = o * b + c * (1.0f - b);
			}
			out[i] = c;
		}

		clean = out;
		glitched = 0;
		lastGood = out;   // update accepted
		prevRaw  = cur;
	}
};
