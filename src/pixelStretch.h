#ifndef PixelStretch_h
#define PixelStretch_h

#include "ofxOceanodeNodeModel.h"

class pixelStretch : public ofxOceanodeNodeModel {
public:
    pixelStretch() : ofxOceanodeNodeModel("Pixel Stretch"){}

    void setup() override {
        description = "Stretches the column of pixels at a given x position to all columns to the left of x.";

        addParameter(input.set("Input", nullptr));
        addParameter(x.set("X", 0, 0, ofGetWidth()));
        addOutputParameter(output.set("Output", nullptr));

        listener = x.newListener([this](int &xValue){
            stretchPixels();
        });
    }

    void update(ofEventArgs &a) {
        stretchPixels();
    }

    void activate() override {
        ofAddListener(ofEvents().update, this, &pixelStretch::update);
    }

    void deactivate() override {
        ofRemoveListener(ofEvents().update, this, &pixelStretch::update);
    }

    void stretchPixels() {
        if(input != nullptr) {
            ofPixels pixels;
            input.get()->readToPixels(pixels);

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

            fbo.allocate(pixels.getWidth(), pixels.getHeight(), GL_RGBA32F);
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
    ofEventListener listener;
};

#endif /* PixelStretch_h */
