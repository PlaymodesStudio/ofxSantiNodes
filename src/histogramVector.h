#ifndef histogramVector_h
#define histogramVector_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include "ofFbo.h"
#include "ofPixels.h"
#include "ofShader.h"
#include "ofTexture.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

class histogramVector : public ofxOceanodeNodeModel {
public:
	histogramVector() : ofxOceanodeNodeModel("Histogram Vector") {
		description =
			"Scrolling spectrogram-style histogram for a float vector input.\n"
			"Every incoming vector becomes a new right-side column and the history scrolls left.\n"
			"Element 0 is rendered at the bottom, like the low band of a classic spectrogram.";
	}

	void setup() override {
		addParameter(input.set("Input",
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addParameter(showWindow.set("Show", false));
		addParameter(width.set("Width", 512, 16, 4096));
		addParameter(speed.set("Speed", 1.0f, 0.0f, 16.0f));
		addParameter(minValue.set("Min", 0.0f, -FLT_MAX, FLT_MAX));
		addParameter(maxValue.set("Max", 1.0f, -FLT_MAX, FLT_MAX));
		addParameter(colormapIdx.set("Colormap", 0, 0, 3));
		addParameter(clear.set("Clear", false));

		addOutputParameter(textureOut.set("Output", nullptr, nullptr, nullptr));
		textureOut.setSerializable(false);

		addInspectorParameter(widgetWidth.set("Widget Width", 480.0f, 100.0f, 1920.0f));
		addInspectorParameter(widgetHeight.set("Widget Height", 200.0f, 40.0f, 600.0f));

		addCustomRegion(
			ofParameter<std::function<void()>>().set("Histogram Vector", [this](){ drawWidget(); }),
			ofParameter<std::function<void()>>().set("Histogram Vector", [this](){ drawWidget(); })
		);

		clearListener = clear.newListener([this](bool &value){
			if(value) {
				clearHistory();
				clear = false;
			}
		});

		initShader();
	}

	void update(ofEventArgs &) override {
		const auto &values = input.get();
		if(values.empty()) return;

		ensureResources((int)values.size(), width.get());
		if(bandCount <= 0 || !fbos[ping].isAllocated()) return;

		const float minV = minValue.get();
		const float maxV = maxValue.get();
		if(maxV <= minV) {
			textureOut = &fbos[ping].getTexture();
			return;
		}

		const float invRange = 1.0f / (maxV - minV);
		if((int)normalizedColumn.size() != bandCount) normalizedColumn.resize(bandCount, 0.0f);

		for(int i = 0; i < bandCount; i++) {
			normalizedColumn[i] = ofClamp((values[i] - minV) * invRange, 0.0f, 1.0f);
		}

		scrollAccum += speed.get();
		int pixels = std::min((int)scrollAccum, historyWidth);
		if(pixels >= 1) {
			scrollAccum -= (float)pixels;
			doScrollPass(pixels);
		}

		textureOut = &fbos[ping].getTexture();
	}

	void draw(ofEventArgs &) override {
		if(!showWindow.get()) return;

		std::string title =
			(canvasID == "Canvas" ? "" : canvasID + "/") +
			"Histogram Vector " + ofToString(getNumIdentifier());

		bool open = showWindow.get();
		if(ImGui::Begin(title.c_str(), &open)) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			drawHistogramImage(
				ImGui::GetCursorScreenPos().x,
				ImGui::GetCursorScreenPos().y,
				std::max(avail.x, 100.0f),
				std::max(avail.y, 40.0f)
			);
			ImGui::Dummy(avail);
		}
		ImGui::End();
		if(!open) showWindow = false;
	}

	void deactivate() override {
		scrollAccum = 0.0f;
		clearHistory();
	}

private:
	ofParameter<std::vector<float>> input;
	ofParameter<bool> showWindow;
	ofParameter<int> width;
	ofParameter<float> speed;
	ofParameter<float> minValue;
	ofParameter<float> maxValue;
	ofParameter<int> colormapIdx;
	ofParameter<bool> clear;
	ofParameter<float> widgetWidth;
	ofParameter<float> widgetHeight;
	ofParameter<ofTexture*> textureOut;

	ofEventListener clearListener;

	ofFbo fbos[2];
	int ping = 0;
	ofShader scrollShader;
	ofTexture columnTex;
	ofFloatPixels columnPix;
	std::vector<float> normalizedColumn;
	std::vector<float> previousColumn;
	int bandCount = 0;
	int historyWidth = 0;
	int columnTextureWidth = 0;
	float scrollAccum = 0.0f;
	bool hasPreviousColumn = false;

	void ensureResources(int newBandCount, int newWidth) {
		newWidth = std::max(newWidth, 1);
		if(newBandCount <= 0) return;

		const bool needsFbos =
			!fbos[0].isAllocated() ||
			!fbos[1].isAllocated() ||
			bandCount != newBandCount ||
			historyWidth != newWidth;

		const bool needsColumn =
			!columnTex.isAllocated() ||
			bandCount != newBandCount;

		bandCount = newBandCount;
		historyWidth = newWidth;

		if(needsFbos) {
			ofFbo::Settings s;
			s.width = historyWidth;
			s.height = bandCount;
			s.internalformat = GL_RGBA8;
			s.useDepth = false;
			s.useStencil = false;
			s.numSamples = 0;
			s.minFilter = GL_LINEAR;
			s.maxFilter = GL_LINEAR;
			s.textureTarget = GL_TEXTURE_2D;
			for(int i = 0; i < 2; i++) {
				fbos[i].allocate(s);
			}
			ping = 0;
			clearHistory();
		}

		if(needsColumn) {
			normalizedColumn.assign(bandCount, 0.0f);
			previousColumn.assign(bandCount, 0.0f);
			hasPreviousColumn = false;
			ensureColumnTextureWidth(1);
		}
	}

	void clearHistory() {
		for(auto &fbo : fbos) {
			if(fbo.isAllocated()) {
				fbo.begin();
				ofClear(0, 0, 0, 255);
				fbo.end();
			}
		}
		if(columnPix.isAllocated()) {
			columnPix.set(0.0f);
			if(columnTex.isAllocated()) columnTex.loadData(columnPix);
		}
		hasPreviousColumn = false;
		if(fbos[ping].isAllocated()) textureOut = &fbos[ping].getTexture();
		else textureOut = nullptr;
	}

	void ensureColumnTextureWidth(int newWidth) {
		newWidth = std::max(newWidth, 1);
		if(bandCount <= 0) return;
		if(columnTex.isAllocated() && columnTextureWidth == newWidth && columnPix.isAllocated()) return;

		columnTextureWidth = newWidth;
		columnPix.allocate(columnTextureWidth, bandCount, OF_PIXELS_R);
		columnPix.set(0.0f);
		columnTex.allocate(columnTextureWidth, bandCount, GL_R32F, false);
		columnTex.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		columnTex.setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
		columnTex.loadData(columnPix);
	}

	void initShader() {
		const std::string vert = R"(
#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
void main() {
	gl_Position = modelViewProjectionMatrix * position;
}
)";

		const std::string frag = R"(
#version 150
uniform sampler2D tHistory;
uniform sampler2D tColumn;
uniform vec2 size;
uniform float normShift;
uniform int colormapIdx;

out vec4 out_color;

vec3 colorHot(float t) {
	return vec3(
		clamp(t * 3.0, 0.0, 1.0),
		clamp(t * 3.0 - 1.0, 0.0, 1.0),
		clamp(t * 3.0 - 2.0, 0.0, 1.0)
	);
}

vec3 colorViridis(float t) {
	vec3 c0 = vec3(0.267, 0.005, 0.329);
	vec3 c1 = vec3(0.128, 0.567, 0.551);
	vec3 c2 = vec3(0.993, 0.906, 0.144);
	return t < 0.5 ? mix(c0, c1, t * 2.0) : mix(c1, c2, (t - 0.5) * 2.0);
}

vec3 colorPhosphor(float t) {
	return vec3(t * 0.1, t, t * 0.3);
}

vec3 applyColormap(float t) {
	if(colormapIdx == 1) return colorViridis(t);
	if(colormapIdx == 2) return colorPhosphor(t);
	if(colormapIdx == 3) return vec3(t);
	return colorHot(t);
}

void main() {
	vec2 uv = gl_FragCoord.xy / size;

	if(uv.x >= 1.0 - normShift) {
		float localX = clamp((uv.x - (1.0 - normShift)) / max(normShift, 1e-6), 0.0, 1.0);
		float value = texture(tColumn, vec2(localX, 1.0 - uv.y)).r;
		out_color = vec4(applyColormap(clamp(value, 0.0, 1.0)), 1.0);
	} else {
		out_color = texture(tHistory, vec2(uv.x + normShift, uv.y));
	}
}
)";

		scrollShader.setupShaderFromSource(GL_VERTEX_SHADER, vert);
		scrollShader.setupShaderFromSource(GL_FRAGMENT_SHADER, frag);
		scrollShader.linkProgram();
	}

	void doScrollPass(int pixels) {
		if(bandCount <= 0 || historyWidth <= 0) return;

		ensureColumnTextureWidth(pixels);
		float *data = columnPix.getData();
		for(int band = 0; band < bandCount; band++) {
			const float startValue = hasPreviousColumn ? previousColumn[band] : normalizedColumn[band];
			const float endValue = normalizedColumn[band];
			for(int x = 0; x < columnTextureWidth; x++) {
				const float t = hasPreviousColumn ? (float)(x + 1) / (float)columnTextureWidth : 1.0f;
				data[band * columnTextureWidth + x] = ofLerp(startValue, endValue, t);
			}
		}
		columnTex.loadData(columnPix);

		const float normShift = (float)pixels / (float)historyWidth;
		ofFbo &readFbo = fbos[ping];
		ofFbo &writeFbo = fbos[1 - ping];

		writeFbo.begin();
		scrollShader.begin();
		scrollShader.setUniformTexture("tHistory", readFbo.getTexture(), 0);
		scrollShader.setUniformTexture("tColumn", columnTex, 1);
		scrollShader.setUniform2f("size", (float)historyWidth, (float)bandCount);
		scrollShader.setUniform1f("normShift", normShift);
		scrollShader.setUniform1i("colormapIdx", colormapIdx.get());
		ofSetColor(255);
		ofDrawRectangle(0, 0, historyWidth, bandCount);
		scrollShader.end();
		writeFbo.end();

		ping = 1 - ping;
		previousColumn = normalizedColumn;
		hasPreviousColumn = true;
	}

	void drawHistogramImage(float x, float y, float width, float height) {
		if(!fbos[ping].isAllocated()) {
			ImGui::Text("No data");
			return;
		}

		GLuint texId = fbos[ping].getTexture().getTextureData().textureID;
		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)(uintptr_t)texId,
			ImVec2(x, y),
			ImVec2(x + width, y + height),
			ImVec2(0, 0),
			ImVec2(1, 1)
		);
		ImGui::SetCursorScreenPos(ImVec2(x, y + height + 2.0f));
	}

	void drawWidget() {
		float zoom = ofxOceanodeShared::getZoomLevel();
		static constexpr float PAD = 2.0f;
		ImVec2 cursor = ImGui::GetCursorScreenPos();
		const auto &customRegionContext = ofxOceanodeShared::getCustomRegionRenderContext();
		const float pad = PAD * zoom;
		const float availableWidth = std::max(1.0f, customRegionContext.width - pad * 2.0f);
		const float availableHeight = std::max(1.0f, customRegionContext.height - pad * 2.0f);
		const float maxNodeWidth = std::max(
			1.0f,
			(float)(ofxOceanodeShared::getNodeWidthText() + ofxOceanodeShared::getNodeWidthWidget()) * zoom - pad * 2.0f
		);
		const float previewWidth = customRegionContext.active
			? availableWidth
			: std::min(widgetWidth.get() * zoom, maxNodeWidth);
		const float previewHeight = customRegionContext.active ? availableHeight : widgetHeight.get() * zoom;
		drawHistogramImage(cursor.x + pad, cursor.y + pad, previewWidth, previewHeight);
		ImGui::Dummy(ImVec2(previewWidth + pad * 2.0f,
			customRegionContext.active ? previewHeight + pad * 2.0f : 4.0f * zoom));
	}
};

#endif /* histogramVector_h */
