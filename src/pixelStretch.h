#ifndef PixelStretch_h
#define PixelStretch_h

#include "ofxOceanodeNodeModel.h"

class pixelStretch : public ofxOceanodeNodeModel {
public:
    pixelStretch() : ofxOceanodeNodeModel("Pixel Stretch") {}

    void setup() override {
        description = "Stretches the column of pixels at a given x position to all columns to the left of x.";

        addParameter(input.set("Input", nullptr));
        addParameter(x.set("X", 0, 0, ofGetWidth()));
        addOutputParameter(output.set("Output", nullptr));

        input.addListener(this, &pixelStretch::onInputChanged);
        fboAllocated = false;
    }

    void update(ofEventArgs &a) {
        if (inputChanged) {
            updateTexture();
            inputChanged = false;
        }
    }

    void activate() override {
        ofAddListener(ofEvents().update, this, &pixelStretch::update);
    }

    void deactivate() override {
        ofRemoveListener(ofEvents().update, this, &pixelStretch::update);
        fbo.clear();
        output = nullptr;
    }

    void onInputChanged(ofTexture* &newInput) {
        inputChanged = true;
    }

    void updateTexture() {
        if (input.get() != nullptr) {
            ofPixels pixels;
            input.get()->readToPixels(pixels);

            if (!fboAllocated || fbo.getWidth() != pixels.getWidth() || fbo.getHeight() != pixels.getHeight()) {
                fbo.allocate(pixels.getWidth(), pixels.getHeight(), GL_RGBA32F);
                fbo.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
                fboAllocated = true;
            }

            int height = pixels.getHeight();
            std::vector<ofColor> columnPixels(height);
            for (int row = 0; row < height; row++) {
                columnPixels[row] = pixels.getColor(x, row);
            }

            for (int col = x - 1; col >= 0; col--) {
                for (int row = 0; row < height; row++) {
                    pixels.setColor(col, row, columnPixels[row]);
                }
            }

            fbo.begin();
            ofClear(0, 0, 0, 255);
            ofTexture tempTexture;
            tempTexture.loadData(pixels);
            tempTexture.draw(0, 0);
            fbo.end();

            output = &fbo.getTexture();
        }
    }

private:
    ofParameter<ofTexture*> input;
    ofParameter<int> x;
    ofParameter<ofTexture*> output;
    ofFbo fbo;
    bool fboAllocated;
    bool inputChanged = false;
};

#endif /* PixelStretch_h */
