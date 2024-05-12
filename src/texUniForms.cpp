#include "texUniForms.h"

#define NUM_INPUTS 2  // Set for two inputs as required

texUniForms::texUniForms() : ofxOceanodeNodeModel("Texture Unifier Forms") {
    color = ofColor::lightGray;
}

void texUniForms::setup(){
    addParameter(triggerTextureIndex.set("Trig.Index", 0, 0, NUM_INPUTS));
    addInspectorParameter(spacing.set("Spacing", 1, 0, INT_MAX));
    addInspectorParameter(lastSpacing.set("Last Space", true));
    inputs.resize(NUM_INPUTS);
    customPositions.resize(NUM_INPUTS);
    opacities.resize(NUM_INPUTS);
    for(int i = 0; i < inputs.size(); i++){
        addParameter(inputs[i].set("Input." + ofToString(i), nullptr));
        addParameter(opacities[i].set("Opac." + ofToString(i), 1.0, 0.0, 1.0));
        addParameter(customPositions[i].set("Pos " + ofToString(i), {0.0f, 0.0f}, {-FLT_MAX, -FLT_MAX}, {FLT_MAX, FLT_MAX}));
        listeners.push(inputs[i].newListener(this, &texUniForms::computeOutput));
    }
    addOutputParameter(output.set("Output", nullptr));
}

void texUniForms::computeOutput(ofTexture* &in){
    if(in != nullptr && &in == &inputs[triggerTextureIndex].get()){
        ofRectangle fboRect(0, 0, 0, 0);
        for(int i = 0; i < inputs.size(); i++){
            auto &t = inputs[i];
            if(t != nullptr){
                const vector<float>& pos = customPositions[i].get(); // Use const reference
                fboRect.growToInclude(ofRectangle(pos[0], pos[1], t.get()->getWidth(), t.get()->getHeight()));
            }
        }
        if(!lastSpacing){
            fboRect.setHeight(fboRect.getHeight() - spacing);
        }
        if(outputFbo.getHeight() != fboRect.getHeight() || outputFbo.getWidth() != fboRect.getWidth() || !outputFbo.isAllocated()){
            if(fboRect.getHeight() != 0 && fboRect.getWidth() != 0){
                outputFbo.clear();
    
                ofFbo::Settings settings;
                settings.height = fboRect.getHeight();
                settings.width = fboRect.getWidth();
                settings.internalformat = GL_RGB32F;
                settings.maxFilter = GL_NEAREST;
                settings.minFilter = GL_NEAREST;
                settings.numColorbuffers = 1;
                settings.useDepth = false;
                settings.useStencil = false;
                
                outputFbo.allocate(settings);
                outputFbo.begin();
                ofClear(0, 0, 0, 255);
                outputFbo.end();
            } else {
                return;
            }
        }
        
        outputFbo.begin();
        ofClear(0, 0, 0);
        for(int i = 0; i < inputs.size(); i++){
            auto &t = inputs[i];
            if(t != nullptr){
                const vector<float>& pos = customPositions[i].get(); // Use const reference here as well
                ofPushStyle();
                ofSetColor(255.0 * opacities[i]);
                t.get()->draw(pos[0], pos[1]); // Use the coordinates directly
                ofPopStyle();
            }
        }
        outputFbo.end();
        
        output = &outputFbo.getTexture();
    }
}
