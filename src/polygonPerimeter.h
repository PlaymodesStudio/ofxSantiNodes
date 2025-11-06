#pragma once
#include "ofxOceanodeNodeModel.h"

class polygonPerimeter : public ofxOceanodeNodeModel {
public:
	polygonPerimeter() : ofxOceanodeNodeModel("Polygon Perimeter") {}

	void setup() override {
		addParameter(xs.set("X", {0.0f}, {0.0f}, {1.0f}));
		addParameter(ys.set("Y", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(perimeter.set("Perimeter", 0.0f, 0.0f, FLT_MAX));

		listenerX = xs.newListener([this](vector<float> &v){ compute(); });
		listenerY = ys.newListener([this](vector<float> &v){ compute(); });
	}

private:
	ofParameter<vector<float>> xs;
	ofParameter<vector<float>> ys;
	ofParameter<float>         perimeter;
	ofEventListener            listenerX, listenerY;

	void compute(){
		const auto &vx = xs.get();
		const auto &vy = ys.get();
		size_t n = std::min(vx.size(), vy.size());
		if(n < 2){
			perimeter = 0.0f;
			return;
		}

		double acc = 0.0;
		for(size_t i = 0; i < n; ++i){
			size_t j = (i + 1) % n;   // següent punt, l’últim connecta al primer
			double dx = (double)vx[j] - (double)vx[i];
			double dy = (double)vy[j] - (double)vy[i];
			acc += sqrt(dx*dx + dy*dy);
		}
		perimeter = acc;
	}
};
