#ifndef pixelStretch_h
#define pixelStretch_h

#include "ofxOceanodeNodeModel.h"

class pixelStretch : public ofxOceanodeNodeModel {
public:
    pixelStretch() : ofxOceanodeNodeModel("Pixel Stretch") {}

    void setup() {
        addParameter(input.set("Input", nullptr));
        addParameter(x.set("X", 0, 0, 2600));
        addOutputParameter(output.set("Output", nullptr));

        color = ofColor::orangeRed;

        loadShader();
    }

    void update(ofEventArgs &e) {
        if(input.get() == nullptr) return;

        if(!fbo.isAllocated() || fbo.getWidth() != input.get()->getWidth() || fbo.getHeight() != input.get()->getHeight()) {
            fbo.allocate(input.get()->getWidth(), input.get()->getHeight(), GL_RGBA);
        }

        fbo.begin();
        shader.begin();
        shader.setUniformTexture("tSource", *input.get(), 0);
        shader.setUniform1f("x", x);
        input.get()->draw(0, 0);
        shader.end();
        fbo.end();

        output = &fbo.getTexture();
    }

private:
    ofParameter<ofTexture*> input;
    ofParameter<float> x;
    ofParameter<ofTexture*> output;
    ofFbo fbo;
    ofShader shader;

    void loadShader() {
        string shaderPath = "Effects/PixelStretch.glsl";
        ofBuffer buffer = ofBufferFromFile(shaderPath);
        
        if(buffer.size()) {
            string shaderSource = buffer.getText();
            shader.setupShaderFromSource(GL_FRAGMENT_SHADER, shaderSource);
            shader.linkProgram();
        } else {
            ofLogError("pixelStretch") << "Unable to load shader file: " << shaderPath;
        }
    }
};

#endif /* pixelStretch_h */
