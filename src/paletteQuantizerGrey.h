#ifndef paletteQuantizerGrey_h
#define paletteQuantizerGrey_h

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

#include "ofxOceanodeNodeModel.h"

class paletteQuantizerGrey : public ofxOceanodeNodeModel {
public:
    paletteQuantizerGrey() : ofxOceanodeNodeModel("paletteQuantizerGrey") {}

    void setup() override {
        addParameter(input.set("Texture In", nullptr));
        addParameter(numColors.set("numColors", 4, 1, 16));
        addParameter(stability.set("stability", 0.85f, 0.0f, 0.99f));
        addParameter(analyzeEvery.set("analyzeEvery", 1, 1, 30));
        addOutputParameter(mixOutput.set("mix", nullptr));
        addOutputParameter(greyOutput.set("grey", nullptr));

        setupShader();

        numColorsListener = numColors.newListener([this](int &){
            resizeCachedPalette();
        });
        resizeCachedPalette();
    }

    void loadBeforeConnections(ofJson &json) {
        deserializeParameter(json, numColors);
        resizeCachedPalette();
    }

    void draw(ofEventArgs &a) override {
        ofTexture *source = input.get();
        if(source == nullptr || !source->isAllocated() || source->getWidth() <= 0 || source->getHeight() <= 0) {
            clearOutputs();
            return;
        }

        const int width = source->getWidth();
        const int height = source->getHeight();
        allocateRenderTargets(width, height);
        updateAnalysisBuffer(*source);

        if(analysisPixels.size() == 0) {
            clearOutputs();
            return;
        }

        if(frameCounter % std::max(1, analyzeEvery.get()) == 0 || !paletteInitialized || cachedPalette.size() != numColors.get()) {
            std::vector<glm::vec3> palette = extractPaletteFromAnalysis();
            if(palette.empty()) {
                clearRenderTargets();
                assignOutputs();
                frameCounter++;
                return;
            }
            updateCachedPalette(palette);
        }

        if(cachedPalette.empty()) {
            clearRenderTargets();
            assignOutputs();
            frameCounter++;
            return;
        }

        renderOutputs(*source, cachedPalette);
        assignOutputs();
        frameCounter++;
    }

    void deactivate() override {
        analysisFbo.clear();
        outputFbo.clear();
        analysisPixels.clear();
        cachedPalette.clear();
        paletteInitialized = false;
        frameCounter = 0;
        mixOutput = nullptr;
        greyOutput = nullptr;
    }

private:
    static constexpr int MAX_PALETTE_COLORS = 16;
    static constexpr int MAX_ANALYSIS_SIDE = 64;

    ofParameter<ofTexture*> input;
    ofParameter<int> numColors;
    ofParameter<float> stability;
    ofParameter<int> analyzeEvery;
    ofParameter<ofTexture*> mixOutput;
    ofParameter<ofTexture*> greyOutput;

    ofFbo analysisFbo;
    ofFbo outputFbo;
    ofFloatPixels analysisPixels;
    ofShader quantizeShader;
    std::array<float, MAX_PALETTE_COLORS * 3> paletteUniformData = {};
    std::vector<glm::vec3> cachedPalette;
    bool paletteInitialized = false;
    int frameCounter = 0;

    ofEventListener numColorsListener;

    void setupShader() {
        const std::string vertSource = R"(
            #version 410
            uniform mat4 modelViewProjectionMatrix;
            layout(location = 0) in vec4 position;
            void main() {
                gl_Position = modelViewProjectionMatrix * position;
            }
        )";

        const std::string fragSource = R"(
            #version 410

            uniform sampler2D tSource;
            uniform int numPaletteColors;
            uniform vec3 palette[16];

            layout(location = 0) out vec4 mixOut;
            layout(location = 1) out vec4 greyOut;

            float colorDistanceSq(vec3 a, vec3 b) {
                vec3 diff = a - b;
                return dot(diff, diff);
            }

            void main() {
                ivec2 uv = ivec2(gl_FragCoord.xy);
                vec4 src = texelFetch(tSource, uv, 0);

                if(src.a <= 0.0) {
                    mixOut = vec4(0.0);
                    greyOut = vec4(0.0);
                    return;
                }

                int bestIndex = 0;
                float bestDistance = colorDistanceSq(src.rgb, palette[0]);
                for(int i = 1; i < numPaletteColors; i++) {
                    float dist = colorDistanceSq(src.rgb, palette[i]);
                    if(dist < bestDistance) {
                        bestDistance = dist;
                        bestIndex = i;
                    }
                }

                float greyValue = 0.0;
                if(numPaletteColors > 1) {
                    greyValue = float(bestIndex) / float(numPaletteColors - 1);
                }

                mixOut = vec4(palette[bestIndex], src.a);
                greyOut = vec4(greyValue, greyValue, greyValue, src.a);
            }
        )";

        quantizeShader.setupShaderFromSource(GL_VERTEX_SHADER, vertSource);
        quantizeShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragSource);
        quantizeShader.bindDefaults();
        quantizeShader.linkProgram();
    }

    void resizeCachedPalette() {
        cachedPalette.resize(numColors.get(), glm::vec3(0.0f));
        paletteInitialized = false;
    }

    void allocateRenderTargets(int width, int height) {
        if(!outputFbo.isAllocated() || outputFbo.getWidth() != width || outputFbo.getHeight() != height) {
            ofFbo::Settings settings;
            settings.width = width;
            settings.height = height;
            settings.internalformat = GL_RGBA32F;
            settings.maxFilter = GL_NEAREST;
            settings.minFilter = GL_NEAREST;
            settings.numColorbuffers = 2;
            settings.useDepth = false;
            settings.useStencil = false;
            settings.textureTarget = GL_TEXTURE_2D;
            outputFbo.allocate(settings);
            outputFbo.begin();
            ofClear(0, 0, 0, 0);
            outputFbo.end();
        }

        int analysisWidth = MAX_ANALYSIS_SIDE;
        int analysisHeight = MAX_ANALYSIS_SIDE;
        if(width >= height) {
            analysisHeight = std::max(1, static_cast<int>(std::round((static_cast<float>(height) / width) * MAX_ANALYSIS_SIDE)));
        } else {
            analysisWidth = std::max(1, static_cast<int>(std::round((static_cast<float>(width) / height) * MAX_ANALYSIS_SIDE)));
        }

        if(!analysisFbo.isAllocated() || analysisFbo.getWidth() != analysisWidth || analysisFbo.getHeight() != analysisHeight) {
            ofFbo::Settings settings;
            settings.width = analysisWidth;
            settings.height = analysisHeight;
            settings.internalformat = GL_RGBA32F;
            settings.maxFilter = GL_NEAREST;
            settings.minFilter = GL_NEAREST;
            settings.numColorbuffers = 1;
            settings.useDepth = false;
            settings.useStencil = false;
            settings.textureTarget = GL_TEXTURE_2D;
            analysisFbo.allocate(settings);
            analysisFbo.begin();
            ofClear(0, 0, 0, 0);
            analysisFbo.end();
        }
    }

    void updateAnalysisBuffer(ofTexture &source) {
        analysisFbo.begin();
        ofClear(0, 0, 0, 0);
        ofSetColor(255, 255, 255, 255);
        source.draw(0, 0, analysisFbo.getWidth(), analysisFbo.getHeight());
        analysisFbo.end();

        analysisFbo.getTexture().readToPixels(analysisPixels);
    }

    std::vector<glm::vec3> extractPaletteFromAnalysis() const {
        std::vector<glm::vec3> sampleColors;
        const int channels = analysisPixels.getNumChannels();
        const int totalPixels = analysisPixels.getWidth() * analysisPixels.getHeight();
        sampleColors.reserve(totalPixels);

        for(int i = 0; i < totalPixels; i++) {
            const int index = i * channels;
            const float alpha = getChannel(analysisPixels, index, 3, channels);
            if(alpha <= 0.0f) continue;

            sampleColors.emplace_back(
                getChannel(analysisPixels, index, 0, channels),
                getChannel(analysisPixels, index, 1, channels),
                getChannel(analysisPixels, index, 2, channels)
            );
        }

        if(sampleColors.empty()) return {};

        const int paletteSize = std::min(numColors.get(), static_cast<int>(sampleColors.size()));
        return buildPalette(sampleColors, paletteSize);
    }

    void updateCachedPalette(const std::vector<glm::vec3> &newPalette) {
        if(newPalette.empty()) return;

        if(!paletteInitialized || cachedPalette.size() != newPalette.size()) {
            cachedPalette = newPalette;
            paletteInitialized = true;
            return;
        }

        std::vector<glm::vec3> reordered = matchPaletteOrder(newPalette, cachedPalette);
        const float blend = ofClamp(1.0f - stability.get(), 0.01f, 1.0f);
        for(size_t i = 0; i < reordered.size(); i++) {
            cachedPalette[i] = glm::mix(cachedPalette[i], reordered[i], blend);
        }
    }

    static std::vector<glm::vec3> matchPaletteOrder(const std::vector<glm::vec3> &current, const std::vector<glm::vec3> &previous) {
        std::vector<glm::vec3> ordered(previous.size(), glm::vec3(0.0f));
        std::vector<bool> used(current.size(), false);

        for(size_t i = 0; i < previous.size(); i++) {
            int bestIndex = -1;
            float bestDistance = std::numeric_limits<float>::max();
            for(size_t j = 0; j < current.size(); j++) {
                if(used[j]) continue;
                const glm::vec3 diff = previous[i] - current[j];
                const float distance = glm::dot(diff, diff);
                if(distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = static_cast<int>(j);
                }
            }

            if(bestIndex >= 0) {
                ordered[i] = current[bestIndex];
                used[bestIndex] = true;
            }
        }

        return ordered;
    }

    static float getChannel(const ofFloatPixels &pixels, int baseIndex, int channel, int numChannels) {
        if(channel == 3 && numChannels < 4) return 1.0f;
        const int safeChannel = std::min(channel, numChannels - 1);
        return pixels[baseIndex + safeChannel];
    }

    void renderOutputs(ofTexture &source, const std::vector<glm::vec3> &palette) {
        std::fill(paletteUniformData.begin(), paletteUniformData.end(), 0.0f);
        for(size_t i = 0; i < palette.size() && i < MAX_PALETTE_COLORS; i++) {
            paletteUniformData[i * 3 + 0] = palette[i].r;
            paletteUniformData[i * 3 + 1] = palette[i].g;
            paletteUniformData[i * 3 + 2] = palette[i].b;
        }

        outputFbo.begin();
        ofClear(0, 0, 0, 0);
        GLenum targetBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, targetBuffers);
        ofSetColor(255, 255, 255, 255);
        quantizeShader.begin();
        quantizeShader.setUniformTexture("tSource", source, 0);
        quantizeShader.setUniform1i("numPaletteColors", static_cast<int>(palette.size()));
        quantizeShader.setUniform3fv("palette[0]", paletteUniformData.data(), MAX_PALETTE_COLORS);
        ofDrawRectangle(0, 0, outputFbo.getWidth(), outputFbo.getHeight());
        quantizeShader.end();
        outputFbo.end();
    }

    void clearRenderTargets() {
        if(!outputFbo.isAllocated()) return;
        outputFbo.begin();
        ofClear(0, 0, 0, 0);
        outputFbo.end();
    }

    void clearOutputs() {
        mixOutput = nullptr;
        greyOutput = nullptr;
    }

    void assignOutputs() {
        mixOutput = outputFbo.isAllocated() ? &outputFbo.getTexture(0) : nullptr;
        greyOutput = outputFbo.isAllocated() ? &outputFbo.getTexture(1) : nullptr;
    }

    std::vector<glm::vec3> buildPalette(const std::vector<glm::vec3> &samples, int paletteSize) const {
        std::vector<glm::vec3> centers;
        centers.reserve(paletteSize);

        const float denominator = std::max(1, paletteSize - 1);
        for(int i = 0; i < paletteSize; i++) {
            const int sampleIndex = static_cast<int>(std::round((samples.size() - 1) * (i / denominator)));
            centers.push_back(samples[sampleIndex]);
        }

        for(int iteration = 0; iteration < 8; iteration++) {
            std::vector<glm::vec3> sums(paletteSize, glm::vec3(0.0f));
            std::vector<int> counts(paletteSize, 0);

            for(const glm::vec3 &sample : samples) {
                const int closestIndex = findClosestPaletteColor(sample, centers);
                sums[closestIndex] += sample;
                counts[closestIndex]++;
            }

            for(int i = 0; i < paletteSize; i++) {
                if(counts[i] > 0) {
                    centers[i] = sums[i] / static_cast<float>(counts[i]);
                }
            }
        }

        std::vector<int> order(paletteSize);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&centers](int a, int b) {
            return luminance(centers[a]) < luminance(centers[b]);
        });

        std::vector<glm::vec3> sortedCenters;
        sortedCenters.reserve(paletteSize);
        for(int index : order) {
            sortedCenters.push_back(centers[index]);
        }

        return sortedCenters;
    }

    static int findClosestPaletteColor(const glm::vec3 &color, const std::vector<glm::vec3> &palette) {
        int bestIndex = 0;
        float bestDistance = std::numeric_limits<float>::max();
        for(size_t i = 0; i < palette.size(); i++) {
            const glm::vec3 diff = color - palette[i];
            const float distance = glm::dot(diff, diff);
            if(distance < bestDistance) {
                bestDistance = distance;
                bestIndex = static_cast<int>(i);
            }
        }
        return bestIndex;
    }

    static float luminance(const glm::vec3 &color) {
        return color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
    }
};

#endif /* paletteQuantizerGrey_h */
