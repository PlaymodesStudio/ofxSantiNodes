#pragma once
#include "ofxOceanodeNodeModel.h"
#include <algorithm> // per std::sort

class vectorSplitOnMinusOne : public ofxOceanodeNodeModel {
public:
	vectorSplitOnMinusOne() : ofxOceanodeNodeModel("vectorSplitOnMinusOne") {}

	void setup() override {
		// input amb un element per defecte
		addParameter(input.set("Input", {0.0f}, {0.0f}, {1.0f}));

		// outputs: els inicialitzem amb 1 element perquè el GUI d’Oceanode no peti
		addOutputParameter(out1.set("Out1", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out2.set("Out2", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out3.set("Out3", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out4.set("Out4", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out5.set("Out5", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out6.set("Out6", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out7.set("Out7", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(out8.set("Out8", {0.0f}, {0.0f}, {1.0f}));

		// nou output: el vector més gran trobat
		addOutputParameter(largest.set("Largest", {0.0f}, {0.0f}, {1.0f}));

		addOutputParameter(numSets.set("NumSets", 0, 0, 8));

		listener = input.newListener([this](vector<float> &v){
			split(v);
		});
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> out1, out2, out3, out4, out5, out6, out7, out8;
	ofParameter<vector<float>> largest;
	ofParameter<int> numSets;
	ofEventListener listener;

	void split(const vector<float> &v){
		// 1) separem pels -1
		vector<vector<float>> chunks;
		chunks.reserve(8);

		vector<float> current;
		current.reserve(v.size());

		auto flushCurrent = [&](){
			if(!current.empty() && chunks.size() < 8){
				chunks.push_back(current);
			}
			current.clear();
		};

		for(float val : v){
			if(val == -1.0f){
				flushCurrent();
			}else{
				current.push_back(val);
			}
		}
		flushCurrent();

		// nombre de grups trobats (abans d’ordenar ja està bé)
		numSets = static_cast<int>(chunks.size());

		// 2) ordenem de més llarg a més curt
		// si hi ha empats, es manté l’ordre d’aparició (stable sort)
		std::stable_sort(chunks.begin(), chunks.end(),
			[](const vector<float> &a, const vector<float> &b){
				return a.size() > b.size(); // desc
			}
		);

		// 3) assignem als outputs segons aquest ordre
		setOut(out1, chunks, 0);
		setOut(out2, chunks, 1);
		setOut(out3, chunks, 2);
		setOut(out4, chunks, 3);
		setOut(out5, chunks, 4);
		setOut(out6, chunks, 5);
		setOut(out7, chunks, 6);
		setOut(out8, chunks, 7);

		// 4) largest = primer (si n’hi ha)
		if(!chunks.empty()){
			largest = chunks[0];
		}else{
			largest = {0.0f};
		}
	}

	void setOut(ofParameter<vector<float>> &dest,
				const vector<vector<float>> &chunks,
				size_t idx)
	{
		if(idx < chunks.size()){
			dest = chunks[idx];
		}else{
			// posar un vector curt perquè el GUI no peti
			dest = {0.0f};
		}
	}
};
