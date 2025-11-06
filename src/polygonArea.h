#pragma once
#include "ofxOceanodeNodeModel.h"

class polygonArea : public ofxOceanodeNodeModel {
public:
	polygonArea() : ofxOceanodeNodeModel("Polygon Area") {}

	void setup() override {
		addParameter(xs.set("X", {0.0f}, {0.0f}, {1.0f}));
		addParameter(ys.set("Y", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(area.set("Area", 0.0f, 0.0f, FLT_MAX));
		
		listenerX = xs.newListener([this](vector<float> &v){ compute(); });
		listenerY = ys.newListener([this](vector<float> &v){ compute(); });
	}

private:
	ofParameter<vector<float>> xs;
	ofParameter<vector<float>> ys;
	ofParameter<float>         area;
	ofEventListener            listenerX, listenerY;

	void compute(){
		const auto &vx = xs.get();
		const auto &vy = ys.get();
		size_t n = std::min(vx.size(), vy.size());
		if(n < 3){
			area = 0.0f;
			return;
		}

		double acc = 0.0;
		for(size_t i = 0; i < n; ++i){
			size_t j = (i + 1) % n; // següent punt, i l’últim connecta amb el primer
			acc += (double)vx[i] * (double)vy[j] - (double)vx[j] * (double)vy[i];
		}
		area = fabs(acc) * 0.5;
	}
};
