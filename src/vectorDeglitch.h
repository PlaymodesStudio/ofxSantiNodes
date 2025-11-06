#pragma once
#include "ofxOceanodeNodeModel.h"
#include <cmath>

class vectorDeglitch : public ofxOceanodeNodeModel {
public:
	vectorDeglitch() : ofxOceanodeNodeModel("vectorDeglitch") {}

	void setup() override {
		// input 0..1
		addParameter(input.set("Input", {0.0f}, {0.0f}, {1.0f}));

		// normalized thresholds
		addParameter(frameThresh.set("FrameThresh", 0.15f, 0.0f, 1.0f));
		addParameter(pointThresh.set("PointThresh", 0.10f, 0.0f, 1.0f));
		addParameter(blend.set("Blend", 1.0f, 0.0f, 1.0f)); // 1 = 100% valor antic

		// quant considerem que un frame és "plà" (p.ex. tot zeros)
		addParameter(flatEps.set("FlatEps", 0.0001f, 0.0f, 0.01f));

		addOutputParameter(clean.set("Clean", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(glitched.set("Glitched", 0, 0, 1));

		listener = input.newListener([this](std::vector<float> &v){
			process(v);
		});

		firstFrame = true;
		waitingForResync = false;
	}

private:
	ofParameter<std::vector<float>> input;
	ofParameter<float> frameThresh, pointThresh, blend, flatEps;
	ofParameter<std::vector<float>> clean;
	ofParameter<int> glitched;

	std::vector<float> prev;
	bool firstFrame;
	bool waitingForResync;  // estem en "hem rebut zeros, espera al pròxim frame bo"
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
		// 1. primer frame
		if(firstFrame){
			clean = inVec;
			prev = inVec;
			glitched = 0;
			firstFrame = false;
			waitingForResync = isFlat(inVec, flatEps.get());
			return;
		}

		// 2. si la mida canvia, reseteja (per si de cas)
		if(inVec.size() != prev.size()){
			clean = inVec;
			prev = inVec;
			glitched = 0;
			waitingForResync = isFlat(inVec, flatEps.get());
			return;
		}

		// 3. detectem si aquest frame és "plà" (zeros)
		bool flatNow = isFlat(inVec, flatEps.get());

		// si ara és plà -> entrem en mode "espera el pròxim bo"
		if(flatNow){
			clean = inVec;
			prev = inVec;
			glitched = 0;
			waitingForResync = true;
			return;
		}

		// 4. si veníem d'un frame plà → acceptem aquest directament i sortim de resync
		if(waitingForResync){
			clean = inVec;
			prev = inVec;
			glitched = 0;
			waitingForResync = false;
			return;
		}

		// 5. aquí ja tenim: mateixa mida, frame no plà, no estem en resync
		size_t n = inVec.size();
		std::vector<float> out(n);

		// frame-level diff
		float avgDiff = 0.0f;
		for(size_t i = 0; i < n; ++i){
			avgDiff += std::fabs(inVec[i] - prev[i]);
		}
		avgDiff /= (float)n;

		if(avgDiff > frameThresh.get()){
			// frame glitxat: reuse prev
			out = prev;
			glitched = 1;
		}else{
			// per-point
			float ptTh = pointThresh.get();
			float b = blend.get();
			for(size_t i = 0; i < n; ++i){
				float cur = inVec[i];
				float old = prev[i];
				float d = std::fabs(cur - old);
				if(d > ptTh){
					cur = old * b + cur * (1.0f - b);
				}
				out[i] = cur;
			}
			glitched = 0;
		}

		clean = out;
		prev = out;
		// no canviem waitingForResync aquí perquè ja estem en mode normal
	}
};
