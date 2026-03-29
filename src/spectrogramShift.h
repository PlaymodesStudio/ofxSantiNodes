#ifndef spectrogramShift_h
#define spectrogramShift_h

#include "ofxOceanodeNodeModel.h"

class spectrogramShift : public ofxOceanodeNodeModel {
public:
	spectrogramShift() : ofxOceanodeNodeModel("Spectrogram Shift") {}

	void setup() {
		addParameter(input.set("Input", nullptr));
		addParameter(width.set("Width", 400, 10, 4096));
		addParameter(scrollSpeed.set("Speed", 1.0f, 0.0f, 16.0f));
		addParameter(clear.set("Clear", false));
		addOutputParameter(output.set("Output", nullptr));

		color = ofColor::cyan;

		string defaultVert = R"(
			#version 410
			uniform mat4 modelViewProjectionMatrix;
			layout(location = 0) in vec4 position;
			void main(){
				gl_Position = modelViewProjectionMatrix * position;
			}
		)";

		shader.setupShaderFromSource(GL_VERTEX_SHADER, defaultVert);
		shader.setupShaderFromFile(GL_FRAGMENT_SHADER, "Effects/SpectrogramShift.glsl");
		shader.bindDefaults();
		shader.linkProgram();

		clearListener = clear.newListener([this](bool &val){
			if(val) {
				clearHistory();
				clear = false;
			}
		});
	}

	void draw(ofEventArgs &e) {
		if(input.get() == nullptr || !input.get()->isAllocated()) return;

		const int inW = input.get()->getWidth();
		const int inH = input.get()->getHeight();
		if(inW <= 0 || inH <= 0) return;

		if(!histA.isAllocated() || histA.getWidth() != width || histA.getHeight() != inH)
			allocateHistory(width, inH);

		// Accumulate fractional speed; only shift when we have >= 1 pixel
		accumulator += scrollSpeed;
		int shift = (int)accumulator;
		if(shift < 1) return;
		accumulator -= shift;
		shift = min(shift, (int)width);

		ofFbo* readFbo  = ping ? &histA : &histB;
		ofFbo* writeFbo = ping ? &histB : &histA;

		writeFbo->begin();
		shader.begin();
		shader.setUniformTexture("tSource",  readFbo->getTexture(), 0);
		shader.setUniformTexture("inputTex", *input.get(),          1);
		shader.setUniform1i("shift", shift);
		ofSetColor(255);
		ofDrawRectangle(0, 0, writeFbo->getWidth(), writeFbo->getHeight());
		shader.end();
		writeFbo->end();

		ping = !ping;
		output = &writeFbo->getTexture();
	}

	void deactivate() {
		histA.clear();
		histB.clear();
		accumulator = 0.0f;
		output = nullptr;
	}

private:
	ofParameter<ofTexture*> input;
	ofParameter<int>        width;
	ofParameter<float>      scrollSpeed;
	ofParameter<bool>       clear;
	ofParameter<ofTexture*> output;

	ofFbo histA, histB;
	bool  ping = false;
	float accumulator = 0.0f;

	ofShader shader;
	ofEventListener clearListener;

	void allocateHistory(int w, int h) {
		ofFbo::Settings s;
		s.width          = w;
		s.height         = h;
		s.internalformat = GL_RGBA32F;
		s.maxFilter      = GL_NEAREST;
		s.minFilter      = GL_NEAREST;
		s.numColorbuffers = 1;
		s.useDepth       = false;
		s.useStencil     = false;
		s.textureTarget  = GL_TEXTURE_2D;
		histA.allocate(s);
		histB.allocate(s);
		clearHistory();
	}

	void clearHistory() {
		if(histA.isAllocated()) { histA.begin(); ofClear(0, 0, 0, 255); histA.end(); }
		if(histB.isAllocated()) { histB.begin(); ofClear(0, 0, 0, 255); histB.end(); }
	}
};

#endif /* spectrogramShift_h */
