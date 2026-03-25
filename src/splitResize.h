#pragma once

#include "ofxOceanodeNodeModel.h"

class splitResize : public ofxOceanodeNodeModel {
public:
    splitResize() : ofxOceanodeNodeModel("Split Resize") {
        description = "Splits the input vector into N equal regions and resizes each region independently using interpolation.";
    }

    void setup() override {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(numRegions.set("Num Regions", 2, 1, 16));
        addParameterDropdown(interp, "Interp", 0, {"Nearest", "Max", "Linear", "Cosine", "Smoothstep", "Quadratic", "Circular", "Elastic", "Catmull-Rom", "Sigmoid", "Cubic"});

        int n = numRegions.get();
        regionSizes.set("Region Sizes", vector<int>(n, 1), vector<int>(1, 1), vector<int>(1, INT_MAX));
        addParameter(regionSizes);

        outputs.resize(n);
        for (int i = 0; i < n; i++) {
            outputs[i].set("Output " + ofToString(i + 1), {0.0f}, {-FLT_MAX}, {FLT_MAX});
            addOutputParameter(outputs[i]);
        }

        listeners.push(numRegions.newListener([this](int &) {
            pendingUpdate = true;
        }));
        listeners.push(input.newListener([this](vector<float> &) {
            process();
        }));
        listeners.push(regionSizes.newListener([this](vector<int> &) {
            process();
        }));
        listeners.push(interp.newListener([this](int &) {
            process();
        }));
    }

    void update(ofEventArgs &a) override {
        if (pendingUpdate) {
            pendingUpdate = false;
            updateNumRegions();
        }
    }

    void loadBeforeConnections(ofJson &json) override {
        deserializeParameter(json, numRegions);
        updateNumRegions();
    }

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
        return lerp(a, b, t * t);
    }

    float circularInterpolate(float a, float b, float t) {
        float t2 = (t <= 0.5f) ? 2.0f * t * t : -2.0f * t * t + 4.0f * t - 1.0f;
        return lerp(a, b, t2);
    }

    float elasticInterpolate(float a, float b, float t) {
        float t2 = t * t;
        float bounce = sin(t * PI * 3.0f) * pow(1.0f - t, 2.0f);
        return lerp(a, b, t2 + bounce * 0.2f);
    }

    float catmullRomInterpolate(const vector<float>& v, float position) {
        int idx = (int)floor(position);
        float t = position - idx;
        float p0 = idx > 0 ? v[idx - 1] : v[0];
        float p1 = v[idx];
        float p2 = idx < (int)v.size() - 1 ? v[idx + 1] : v[v.size() - 1];
        float p3 = idx < (int)v.size() - 2 ? v[idx + 2] : p2;
        float t2 = t * t, t3 = t2 * t;
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    float sigmoidInterpolate(float a, float b, float t) {
        float scaled_t = (t - 0.5f) * 2.5f;
        float sigmoid = 1.0f / (1.0f + exp(-scaled_t * 4.0f));
        return lerp(a, b, sigmoid);
    }

    float cubicInterpolate(float a, float b, float t) {
        float t2 = t * t, t3 = t2 * t;
        return lerp(a, b, 3.0f * t2 - 2.0f * t3);
    }

    float getInterpolatedValue(const vector<float>& v, float position, int method) {
        if (position <= 0) return v[0];
        if (position >= (float)(v.size() - 1)) return v[v.size() - 1];
        if (method == 0) return v[(int)round(position)]; // Nearest
        // method == 1 (Max) is handled in resizeRegion before this is called
        int idx1 = (int)floor(position);
        int idx2 = idx1 + 1;
        float t = position - idx1;
        switch (method) {
            case 2: return lerp(v[idx1], v[idx2], t);
            case 3: return cosineInterpolate(v[idx1], v[idx2], t);
            case 4: return smoothstepInterpolate(v[idx1], v[idx2], t);
            case 5: return quadraticInterpolate(v[idx1], v[idx2], t);
            case 6: return circularInterpolate(v[idx1], v[idx2], t);
            case 7: return elasticInterpolate(v[idx1], v[idx2], t);
            case 8: return catmullRomInterpolate(v, position);
            case 9: return sigmoidInterpolate(v[idx1], v[idx2], t);
            case 10: return cubicInterpolate(v[idx1], v[idx2], t);
            default: return lerp(v[idx1], v[idx2], t);
        }
    }

    vector<float> resizeRegion(const vector<float>& region, int targetSize, int method) {
        if (targetSize <= 0) targetSize = 1;
        if ((int)region.size() == targetSize) return region;
        if (region.size() == 1) return vector<float>(targetSize, region[0]);

        // Max pooling: each output sample = max of its input window
        if (method == 1) {
            int inSize = (int)region.size();
            vector<float> result(targetSize);
            for (int i = 0; i < targetSize; i++) {
                int iStart = (int)floor((float)i / targetSize * inSize);
                int iEnd   = (int)ceil((float)(i + 1) / targetSize * inSize);
                if (iEnd > inSize) iEnd = inSize;
                float maxVal = region[iStart];
                for (int j = iStart + 1; j < iEnd; j++) {
                    if (region[j] > maxVal) maxVal = region[j];
                }
                result[i] = maxVal;
            }
            return result;
        }

        vector<float> result(targetSize);
        for (int i = 0; i < targetSize; i++) {
            float position = (targetSize > 1) ? ((float)i / (float)(targetSize - 1)) * (region.size() - 1) : 0.0f;
            result[i] = getInterpolatedValue(region, position, method);
        }
        return result;
    }

    void process() {
        const vector<float>& in = input.get();
        int n = (int)outputs.size();
        if (in.empty() || n == 0) return;

        const vector<int>& sizes = regionSizes.get();
        int interpMethod = interp;
        int inSize = (int)in.size();

        // Collect active regions (size > 0)
        vector<int> activeIndices;
        for (int r = 0; r < n; r++) {
            int sz = (r < (int)sizes.size()) ? sizes[r] : 1;
            if (sz > 0) activeIndices.push_back(r);
        }
        int activeCount = (int)activeIndices.size();

        for (int r = 0; r < n; r++) {
            int sz = (r < (int)sizes.size()) ? sizes[r] : 1;

            if (sz == 0 || activeCount == 0) {
                outputs[r].set(vector<float>(1, 0.0f));
                continue;
            }

            // Find this region's position among active regions
            int activePos = 0;
            for (int a = 0; a < activeCount; a++) {
                if (activeIndices[a] == r) { activePos = a; break; }
            }

            int iStart = (int)floor((float)activePos / activeCount * inSize);
            int iEnd   = (int)floor((float)(activePos + 1) / activeCount * inSize);
            if (iEnd > inSize) iEnd = inSize;
            if (iEnd <= iStart) iEnd = iStart + 1;
            if (iEnd > inSize) { iStart = inSize - 1; iEnd = inSize; }

            vector<float> region(in.begin() + iStart, in.begin() + iEnd);
            outputs[r].set(resizeRegion(region, sz, interpMethod));
        }
    }

    void updateNumRegions() {
        int oldSize = (int)outputs.size();
        int newSize = numRegions.get();
        if (newSize == oldSize) return;

        if (newSize < oldSize) {
            for (int i = oldSize - 1; i >= newSize; i--) {
                removeParameter("Output " + ofToString(i + 1));
            }
            outputs.resize(newSize);
        } else if (newSize > oldSize) {
            outputs.resize(newSize);
            for (int i = oldSize; i < newSize; i++) {
                outputs[i].set("Output " + ofToString(i + 1), {0.0f}, {-FLT_MAX}, {FLT_MAX});
                addOutputParameter(outputs[i]);
            }
        }

        // Sync regionSizes length to numRegions
        vector<int> sizes = regionSizes.get();
        sizes.resize(newSize, 1);
        regionSizes.set(sizes);
    }

    ofParameter<int> numRegions;
    ofParameter<vector<float>> input;
    ofParameter<int> interp;
    ofParameter<vector<int>> regionSizes;
    vector<ofParameter<vector<float>>> outputs;

    bool pendingUpdate = false;
    ofEventListeners listeners;
};
