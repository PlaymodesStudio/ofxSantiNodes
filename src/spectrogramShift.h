#ifndef spectrogramShift_h
#define spectrogramShift_h

#include "ofxOceanodeNodeModel.h"

class spectrogramShift : public ofxOceanodeNodeModel {
public:
	spectrogramShift() : ofxOceanodeNodeModel("Spectrogram Shift") {}

	void setup() {
		// Use normalized UVs for anything we allocate from now on.
		ofDisableArbTex();

		addParameter(input.set("Input", nullptr));
		addParameter(width.set("Width", 400, 10, 4096));
		addParameter(clear.set("Clear", false));
		addOutputParameter(output.set("Output", nullptr));

		color = ofColor::cyan;

		loadShader();

		clearListener = clear.newListener([this](bool &val){
			if(val) {
				clearHistory();
				clear = false;
			}
		});
	}

	void update(ofEventArgs &e) {
		if(input.get() == nullptr) return;

		const int inW = input.get()->getWidth();
		const int inH = input.get()->getHeight();
		if(inW <= 0 || inH <= 0) return;

		// Allocate/resize FBOs as needed
		if(!histA.isAllocated() || histA.getWidth() != width || histA.getHeight() != inH){
			allocateHistory(width, inH);
		}
		if(!input2D.isAllocated() || input2D.getWidth() != inW || input2D.getHeight() != inH){
			allocateInput2D(inW, inH);
		}

		// 1) Normalize input to GL_TEXTURE_2D by drawing it into input2D (works whether source is 2D or RECT)
		input2D.begin();
		ofClear(0,0,0,255);
		ofSetColor(255);                // no tint
		// Stretch/fit exactly (column mapping depends only on normalized UVs later)
		input.get()->draw(0, 0, input2D.getWidth(), input2D.getHeight());
		input2D.end();

		// 2) Ping-pong: read from prev history, write into next
		ofFbo* readFbo  = ping ? &histA : &histB;
		ofFbo* writeFbo = ping ? &histB : &histA;

		writeFbo->begin();
		ofClear(0,0,0,255);
		shader.begin();

		// Uniforms
		shader.setUniformTexture("prevHistory", readFbo->getTexture(), 0);
		shader.setUniformTexture("inputTex",    input2D.getTexture(),   1);

		// Draw a full-screen quad into writeFbo
		ofSetColor(255);
		ofDrawRectangle(0, 0, writeFbo->getWidth(), writeFbo->getHeight());

		shader.end();
		writeFbo->end();

		ping = !ping;
		output = &writeFbo->getTexture();
	}

private:
	// Parameters
	ofParameter<ofTexture*> input;
	ofParameter<int>        width;
	ofParameter<bool>       clear;
	ofParameter<ofTexture*> output;

	// FBOs
	ofFbo histA, histB;  // ping-pong history buffer (GL_TEXTURE_2D)
	ofFbo input2D;       // normalized copy of input (GL_TEXTURE_2D)
	bool  ping = false;

	// Shader
	ofShader shader;

	// Clear button listener
	ofEventListener clearListener;

	void allocateHistory(int w, int h){
		ofFbo::Settings s;
		s.width = w;
		s.height = h;
		s.internalformat = GL_RGBA32F;      // high precision (use GL_RGB8 if you prefer)
		s.useDepth = false;
		s.numSamples = 0;
		s.textureTarget = GL_TEXTURE_2D;
		s.minFilter = GL_NEAREST;
		s.maxFilter = GL_NEAREST;
		histA.allocate(s);
		histB.allocate(s);

		histA.getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
		histB.getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		clearHistory();
	}

	void allocateInput2D(int w, int h){
		ofFbo::Settings s;
		s.width = w;
		s.height = h;
		s.internalformat = GL_RGBA32F;
		s.useDepth = false;
		s.numSamples = 0;
		s.textureTarget = GL_TEXTURE_2D;
		s.minFilter = GL_NEAREST;
		s.maxFilter = GL_NEAREST;
		input2D.allocate(s);
		input2D.getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	}

	void loadShader() {
		const std::string vert = R"(
			#version 410
			uniform mat4 modelViewProjectionMatrix;
			in vec4 position;
			in vec2 texcoord;
			out vec2 vUV;
			void main(){
				vUV = texcoord;                         // normalized 0..1 (because we allocate 2D tex)
				gl_Position = modelViewProjectionMatrix * position;
			}
		)";

		// Logic:
		// - Shift history left by 1 pixel: sample prevHistory at x+1
		// - Fill rightmost column with the RIGHTMOST column from the input texture
		const std::string frag = R"(
			#version 410
			uniform sampler2D prevHistory;
			uniform sampler2D inputTex;
			in vec2 vUV;
			out vec4 outColor;

			void main(){
				vec2 hSize = vec2(textureSize(prevHistory, 0));  // (width, height)
				vec2 iSize = vec2(textureSize(inputTex,    0));

				// Defensive: if upstream ever sent pixel coords, normalize them.
				vec2 uv = vUV;
				if (max(uv.x, uv.y) > 1.0) uv /= hSize;

				float x = uv.x * hSize.x;
				float y = uv.y;

				// If not the rightmost pixel, shift left by one.
				if (x < hSize.x - 1.0) {
					float nx = (x + 1.0) / hSize.x;
					outColor = texture(prevHistory, vec2(nx, y));
				} else {
					// Rightmost pixel: sample rightmost column from input
					float ix = (iSize.x - 1.0) / iSize.x;   // rightmost normalized column
					outColor = texture(inputTex, vec2(ix, y));
				}
			}
		)";

		shader.setupShaderFromSource(GL_VERTEX_SHADER,   vert);
		shader.setupShaderFromSource(GL_FRAGMENT_SHADER, frag);
		shader.bindDefaults();
		shader.linkProgram();
	}

	void clearHistory(){
		histA.begin(); ofClear(0,0,0,255); histA.end();
		histB.begin(); ofClear(0,0,0,255); histB.end();
	}
};

#endif /* spectrogramShift_h */
