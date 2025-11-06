#pragma once
#include "ofxOceanodeNodeModel.h"

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
				// tanca chunk
				flushCurrent();
			}else{
				current.push_back(val);
			}
		}
		// últim chunk
		flushCurrent();

		// posem-ho als outputs, buidant els que sobren
		setOut(out1, chunks, 0);
		setOut(out2, chunks, 1);
		setOut(out3, chunks, 2);
		setOut(out4, chunks, 3);
		setOut(out5, chunks, 4);
		setOut(out6, chunks, 5);
		setOut(out7, chunks, 6);
		setOut(out8, chunks, 7);

		// nombre de grups trobats
		numSets = static_cast<int>(chunks.size());

		// calcular el més gran
		if(!chunks.empty()){
			// busquem el de més mida; si hi ha empat, es queda el primer
			size_t bestIdx = 0;
			size_t bestSize = chunks[0].size();
			for(size_t i = 1; i < chunks.size(); ++i){
				if(chunks[i].size() > bestSize){
					bestSize = chunks[i].size();
					bestIdx = i;
				}
			}
			largest = chunks[bestIdx];
		}else{
			// si no hi ha chunks, posem un vector curt perquè el GUI no peti
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
