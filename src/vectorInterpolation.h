#ifndef vectorInterpolation_h
#define vectorInterpolation_h

#include "ofxOceanodeNodeModel.h"

class vectorInterpolation : public ofxOceanodeNodeModel {
public:
    vectorInterpolation() : ofxOceanodeNodeModel("Vector Interpolation") {
        addParameter(input.set("Input", {0}, {0}, {1}));
        addParameter(size.set("Size", 1, 1, INT_MAX));
        addParameterDropdown(interp, "Interp", 0, {"Linear", "Cosine", "Smoothstep", "Quadratic", "Circular", "Elastic", "Catmull-Rom", "Sigmoid", "Cubic"});
        addOutputParameter(output.set("Output", {0}, {0}, {1}));
        
        listener = input.newListener(this, &vectorInterpolation::inputListener);
        listener2 = size.newListener([this](int &i){
            vector<float> v = input.get();
            inputListener(v);
        });
    };
    
private:
    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }
    
    float cosineInterpolate(float a, float b, float t) {
        float t2 = (1.0f - cos(t * PI)) / 2.0f;
        return lerp(a, b, t2);
    }
    
    float smoothstepInterpolate(float a, float b, float t) {
        float t2 = t * t * (3 - 2 * t);
        return lerp(a, b, t2);
    }
    
    float quadraticInterpolate(float a, float b, float t) {
        float t2 = t * t;
        return lerp(a, b, t2);
    }
    
    float circularInterpolate(float a, float b, float t) {
        float t2;
        if(t <= 0.5f) {
            t2 = 2.0f * t * t; // Accelerate
        } else {
            t2 = -2.0f * t * t + 4.0f * t - 1.0f; // Decelerate
        }
        return lerp(a, b, t2);
    }
    
    float elasticInterpolate(float a, float b, float t) {
        float t2 = t * t;
        float bounceFactor = sin(t * PI * 3.0f) * pow(1.0f - t, 2.0f);
        return lerp(a, b, t2 + bounceFactor * 0.2f);
    }
    
    float catmullRomInterpolate(const vector<float>& v, float position) {
        int idx = floor(position);
        float t = position - idx;
        
        // Get the four points needed
        float p0 = idx > 0 ? v[idx-1] : v[0];
        float p1 = v[idx];
        float p2 = idx < v.size()-1 ? v[idx+1] : v[v.size()-1];
        float p3 = idx < v.size()-2 ? v[idx+2] : p2;
        
        float t2 = t * t;
        float t3 = t2 * t;
        
        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
    }
    
    float sigmoidInterpolate(float a, float b, float t) {
        // Adjusted sigmoid with better curve control
        const float steepness = 2.5f;
        float scaled_t = (t - 0.5f) * steepness;
        float sigmoid = 1.0f / (1.0f + exp(-scaled_t * 4.0f));
        return lerp(a, b, sigmoid);
    }
    
    float cubicInterpolate(float a, float b, float t) {
        // Improved cubic with better easing
        float t2 = t * t;
        float t3 = t2 * t;
        float ease = 3.0f * t2 - 2.0f * t3;
        return lerp(a, b, ease);
    }
    
    float getInterpolatedValue(const vector<float>& v, float position, int method) {
        if(position <= 0) return v[0];
        if(position >= v.size() - 1) return v[v.size() - 1];
        
        int idx1 = floor(position);
        int idx2 = idx1 + 1;
        float t = position - idx1;
        
        switch(method) {
            case 0: // Linear
                return lerp(v[idx1], v[idx2], t);
            case 1: // Cosine
                return cosineInterpolate(v[idx1], v[idx2], t);
            case 2: // Smoothstep
                return smoothstepInterpolate(v[idx1], v[idx2], t);
            case 3: // Quadratic
                return quadraticInterpolate(v[idx1], v[idx2], t);
            case 4: // Circular
                return circularInterpolate(v[idx1], v[idx2], t);
            case 5: // Elastic
                return elasticInterpolate(v[idx1], v[idx2], t);
            case 6: // Catmull-Rom
                return catmullRomInterpolate(v, position);
            case 7: // Sigmoid
                return sigmoidInterpolate(v[idx1], v[idx2], t);
            case 8: // Cubic
                return cubicInterpolate(v[idx1], v[idx2], t);
            default:
                return lerp(v[idx1], v[idx2], t);
        }
    }
    
    void inputListener(vector<float> &v){
        int _size = size;
        if(_size < 1) _size = 1;
        
        if(v.size() > 0){
            if(v.size() == _size){
                output = v;
            }else{
                vector<float> tempOut(_size, 0);
                for(int i = 0; i < _size; i++){
                    float position = ((float)i / (float)(_size - 1)) * (v.size() - 1);
                    tempOut[i] = getInterpolatedValue(v, position, interp);
                }
                output = tempOut;
            }
        }
    }
    
    ofEventListener listener;
    ofEventListener listener2;
    
    ofParameter<vector<float>> input;
    ofParameter<int> size;
    ofParameter<int> interp;
    ofParameter<vector<float>> output;
};

#endif /* vectorInterpolation_h */
