#ifndef vectorColorGradient_h
#define vectorColorGradient_h

#include "ofxOceanodeNodeModel.h"

class vectorColorGradient : public ofxOceanodeNodeModel {
public:
	vectorColorGradient() : ofxOceanodeNodeModel("Vector Color Gradient") {}

	void setup() override {
		description = "Generates R, G, B vector outputs. Features Curve control for easing and multiple interpolation modes (RGB, HSB Short, HSB Long).";

		// Inputs
		addParameter(colorA.set("Color A", ofFloatColor(0.0, 0.0, 0.0, 1.0), ofFloatColor(0.0, 0.0, 0.0, 0.0), ofFloatColor(1.0, 1.0, 1.0, 1.0)));
		addParameter(colorB.set("Color B", ofFloatColor(1.0, 1.0, 1.0, 1.0), ofFloatColor(0.0, 0.0, 0.0, 0.0), ofFloatColor(1.0, 1.0, 1.0, 1.0)));
		addParameter(size.set("Size", 100, 2, 10000));
		addParameter(curve.set("Curve", 1.0, 0.01, 5.0));
		
		// Mode: 0 = RGB, 1 = HSB Short, 2 = HSB Long
		addInspectorParameter(mode.set("Mode", 0, 0, 2));

		// Outputs
		addOutputParameter(outR.set("R", {0.0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(outG.set("G", {0.0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(outB.set("B", {0.0}, {-FLT_MAX}, {FLT_MAX}));

		// Listeners
		listeners.push(colorA.newListener([this](ofFloatColor &c){ calculate(); }));
		listeners.push(colorB.newListener([this](ofFloatColor &c){ calculate(); }));
		listeners.push(size.newListener([this](int &s){ calculate(); }));
		listeners.push(curve.newListener([this](float &f){ calculate(); }));
		listeners.push(mode.newListener([this](int &i){ calculate(); }));

		calculate();
	}

	void draw(ofEventArgs &a) override {
		// Draw a small preview of the gradient on the node itself
		float w = 150; // Standard node width approx
		float h = 20;
		float x = 0; // Relative to node transform
		float y = 30; // Below title
		
		// Safety check if vectors are empty
		if(outR.get().empty()) return;
		
		int n = outR.get().size();
		ofMesh m;
		m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
		
		for(int i = 0; i < n; i++){
			float px = ofMap(i, 0, n-1, -w/2 + 10, w/2 - 10); // Centered with padding
			float r = outR.get()[i];
			float g = outG.get()[i];
			float b = outB.get()[i];
			
			m.addVertex(glm::vec3(px, -10, 0));
			m.addColor(ofFloatColor(r, g, b, 1.0));
			
			m.addVertex(glm::vec3(px, 10, 0));
			m.addColor(ofFloatColor(r, g, b, 1.0));
		}
		
		// Draw preview box in center of node
		ofPushMatrix();
		ofTranslate(0, 40); // Move down a bit
		ofPushStyle();
		m.draw();
		
		// Draw Border
		ofNoFill();
		ofSetColor(150);
		ofDrawRectangle(-w/2 + 10, -10, w - 20, 20);
		ofPopStyle();
		ofPopMatrix();
	}

	void calculate() {
		int count = size.get();
		if (count < 1) count = 1;

		vector<float> tempR; tempR.reserve(count);
		vector<float> tempG; tempG.reserve(count);
		vector<float> tempB; tempB.reserve(count);

		ofFloatColor cA = colorA.get();
		ofFloatColor cB = colorB.get();
		int currentMode = mode.get();
		float curveExp = curve.get();

		if (count == 1) {
			tempR.push_back(cA.r);
			tempG.push_back(cA.g);
			tempB.push_back(cA.b);
		} else {
			// Pre-calculate HSB values if needed to avoid doing it per-pixel
			float h1, s1, b1, h2, s2, b2;
			if(currentMode > 0) {
				cA.getHsb(h1, s1, b1);
				cB.getHsb(h2, s2, b2);
				
				// HSB Short Path Logic
				float diff = h2 - h1;
				if(currentMode == 1) { // Short
					if (diff > 0.5f) { h1 += 1.0f; }
					else if (diff < -0.5f) { h1 -= 1.0f; }
				}
				// HSB Long Path Logic
				else if(currentMode == 2) { // Long
					if (diff >= 0.0f && diff < 0.5f) { h1 += 1.0f; }
					else if (diff < 0.0f && diff > -0.5f) { h2 += 1.0f; }
				}
			}

			for (int i = 0; i < count; i++) {
				// Normalized linear progress 0.0 -> 1.0
				float pct = (float)i / (float)(count - 1);
				
				// Apply easing curve (Gamma correction style)
				float t = pow(pct, curveExp);
				
				float r, g, b;

				if (currentMode == 0) {
					// RGB Linear
					r = ofLerp(cA.r, cB.r, t);
					g = ofLerp(cA.g, cB.g, t);
					b = ofLerp(cA.b, cB.b, t);
				} else {
					// HSB Interpolation
					float h = ofLerp(h1, h2, t);
					float s = ofLerp(s1, s2, t);
					float v = ofLerp(b1, b2, t); // brightness
					
					// Wrap hue back to 0-1 range
					while(h > 1.0f) h -= 1.0f;
					while(h < 0.0f) h += 1.0f;
					
					ofFloatColor c;
					c.setHsb(h, s, v);
					r = c.r;
					g = c.g;
					b = c.b;
				}
				
				tempR.push_back(r);
				tempG.push_back(g);
				tempB.push_back(b);
			}
		}

		outR = tempR;
		outG = tempG;
		outB = tempB;
	}
	
	// Custom UI drawing for Mode to show text instead of just numbers
	void customDrawInspector() {
		// This is optional if your Oceanode version supports custom ImGui in inspector
		// Otherwise the integer slider works fine.
	}

private:
	ofParameter<ofFloatColor> colorA;
	ofParameter<ofFloatColor> colorB;
	ofParameter<int> size;
	ofParameter<float> curve;
	ofParameter<int> mode;

	ofParameter<vector<float>> outR;
	ofParameter<vector<float>> outG;
	ofParameter<vector<float>> outB;

	ofEventListeners listeners;
};

#endif /* vectorColorGradient_h */
