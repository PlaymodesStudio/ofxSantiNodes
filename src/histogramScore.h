#ifndef histogramScore_h
#define histogramScore_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeInspectorController.h"
#include <algorithm>

class histogramScore : public ofxOceanodeNodeModel {
public:
	histogramScore() : ofxOceanodeNodeModel("Histogram Score") {
		description =
			"Scrolling waveform rendered into a texture with transparent background.\n"
			"Each vector element draws an independent lane.\n"
			"In Pitch Mode, input = MIDI pitch; Min/Max define the pitch range.\n"
			"PitxText draws note names (C4, Eb3…) at note onsets (thickness 0→nonzero).";

		maxBufferSamples = 60 * 240;
		writeIndex       = 0;
		numLanes         = 0;
		totalWriteCount  = 0;
	}

	void setup() override {
		// ── scan fonts in data folder ─────────────────────────────────────
		scanFonts();

		// ── per-sample vector inputs ──────────────────────────────────────
		addParameter(input.set("Input",
			std::vector<float>(1, 0.5f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addParameter(thickness.set("Thickness",
			std::vector<float>(1, 0.05f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addParameter(opacity.set("Opacity",
			std::vector<float>(1, 1.0f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addParameter(hue.set("H",
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addParameter(sat.set("S",
			std::vector<float>(1, 0.8f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addParameter(lum.set("L",
			std::vector<float>(1, 0.5f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));

		// ── scalar controls ───────────────────────────────────────────────
		addParameter(minVal.set("Min",        0.0f, -FLT_MAX, FLT_MAX));
		addParameter(maxVal.set("Max",        1.0f, -FLT_MAX, FLT_MAX));
		addParameter(timeWindow.set("Time Window", 2.0f, 0.1f, 240.0f));
		addParameter(freeze.set("Freeze",     false));
		addParameter(fboWidth.set("Width",  600, 64, 4096));
		addParameter(fboHeight.set("Height", 200, 64, 4096));

		// ── time mode ─────────────────────────────────────────────────────
		// Time Mode replaces Time Window with three parameters:
		//   TimeToHeader  — how many ms after generation data arrives at HeaderPos
		//   ZoomMs        — how many ms of history fill the full texture width
		//   HeaderPos     — pixel column that acts as the sync point
		//
		// All nodes sharing the same TimeToHeader will have data generated at
		// the same moment arrive at their HeaderPos at the same wall-clock time,
		// regardless of ZoomMs (zoom level) or texture width.
		addParameter(timeMode.set("Time Mode",       false));
		addParameter(timeToHeader.set("TimeToHeader", 3000.0f, 0.0f, 60000.0f));
		addParameter(zoomMs.set("ZoomMs",             {3000.0f}, {1.0f}, {60000.0f}));
		addParameter(headerPos.set("HeaderPos",       0, 0, 4096));

		// ── dropdowns (node GUI) ──────────────────────────────────────────
		addParameterDropdown(blendMode, "Blend Mode", 0,
			{"Alpha", "Add", "Multiply", "Screen", "Subtract"});

		// ── pitch mode ────────────────────────────────────────────────────
		addParameter(pitchMode.set("Pitch Mode", false));
		addParameter(pitxText.set("PitxText",   false));

		// ── inspector: font & size ────────────────────────────────────────
		if(!fontPaths.empty()){
			ofxOceanodeInspectorController::registerInspectorDropdown(
				"Histogram Score", "Font", fontNames);
			fontIndex.set("Font", 0, 0, (int)fontNames.size() - 1);
			addInspectorParameter(fontIndex);
		}
		addInspectorParameter(fontSize.set("Font Size", 18, 6, 96));
		addInspectorParameter(pitchGrid.set("Pitch Grid",        true));
		addInspectorParameter(gridThickness.set("Grid Thickness", 1.0f, 0.1f, 10.0f));
		addParameter(fontLightness.set("FontL",
			std::vector<float>(1, 0.5f),
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, 1.0f)));
		addInspectorParameter(showBg.set("Background",  false));
		addInspectorParameter(bgColor.set("BG Color",   ofColor(0, 0, 0, 255)));

		// ── output ────────────────────────────────────────────────────────
		addOutputParameter(output.set("Output",     nullptr));
		addOutputParameter(outputGrid.set("Grid Out", nullptr));
		addOutputParameter(outputText.set("Text Out", nullptr));

		// ── font reload listeners ─────────────────────────────────────────
		fontIndexListener = fontIndex.newListener([this](int &){ loadFont(); });
		fontSizeListener  = fontSize.newListener([this](int &){ loadFont(); });

		// ── FBO size listeners: force reallocation on next draw ───────────
		fboWidthListener  = fboWidth.newListener([this](int &){ fbo.clear(); fboGrid.clear(); fboText.clear(); histMesh.clear(); });
		fboHeightListener = fboHeight.newListener([this](int &){ fbo.clear(); fboGrid.clear(); fboText.clear(); });

		resizeBuffers(1);
		loadFont();
	}

	// ── update: write ring buffers + detect note onsets ──────────────────
	void update(ofEventArgs &) override {
		if(freeze.get()) return;

		const auto& inVec  = input.get();
		int lanes = (int)inVec.size();
		if(lanes < 1) return;
		if(lanes != numLanes) resizeBuffers(lanes);

		const auto& thkVec = thickness.get();
		const auto& opaVec = opacity.get();
		const auto& hVec   = hue.get();
		const auto& sVec   = sat.get();
		const auto& lVec   = lum.get();

		const bool doPitx = pitxText.get() && pitchMode.get();

		for(int i = 0; i < numLanes; i++){
			float curThk = elem(thkVec, i, 0.0f);

			valBufs[i][writeIndex] = elem(inVec,  i, 0.5f);
			thkBufs[i][writeIndex] = curThk;
			opaBufs[i][writeIndex] = elem(opaVec, i, 1.0f);
			hueBufs[i][writeIndex] = elem(hVec,   i, 0.0f);
			satBufs[i][writeIndex] = elem(sVec,   i, 0.8f);
			lumBufs[i][writeIndex] = elem(lVec,   i, 0.5f);

			// label whenever the rounded MIDI pitch changes (and note is active)
			if(doPitx && curThk > 0.0f){
				int curMidi = (int)std::round(elem(inVec, i, 60.0f));
				if(curMidi != lastLabelPitch[i]){
					NoteLabel nl;
					nl.writeCount = totalWriteCount;
					nl.midiPitch  = curMidi;
					nl.h = elem(hVec, i, 0.0f);
					nl.s = elem(sVec, i, 0.8f);
					nl.l = elem(lVec, i, 0.5f);
					notationBufs[i].push_back(nl);
					lastLabelPitch[i] = curMidi;
				}
			}

			// reset pitch tracking when note goes silent
			if(curThk <= 0.0f) lastLabelPitch[i] = -1;
		}

		writeIndex = (writeIndex + 1) % maxBufferSamples;
		totalWriteCount++;
	}

	// ── draw: render into FBO ─────────────────────────────────────────────
	void draw(ofEventArgs &) override {
		const int w = fboWidth.get();
		const int h = fboHeight.get();

		auto allocFbo = [](ofFbo& f, int w, int h){
			if(f.isAllocated() && (int)f.getWidth() == w && (int)f.getHeight() == h) return;
			ofFbo::Settings s;
			s.width          = w;
			s.height         = h;
			s.internalformat = GL_RGBA;
			s.useDepth       = false;
			s.numSamples     = 0;
			s.textureTarget  = GL_TEXTURE_2D;
			f.allocate(s);
		};
		allocFbo(fbo,     w, h);
		allocFbo(fboText, w, h);

		const float minV  = minVal.get();
		const float maxV  = maxVal.get();
		const float range = maxV - minV;
		if(range <= 0.0f || numLanes == 0){ output = nullptr; return; }

		// ── per-lane time computation ──────────────────────────────────────
		//
		// ZoomMs is now a vector so each lane can have a different zoom level.
		// All other time-mode parameters (TimeToHeader, HeaderPos) are shared.
		//
		// The formula (derived in setup comments) is:
		//   samplesVis   = ZoomMs_i × fps / 1000
		//   delaySamples = (TimeToHeader − ZoomMs_i × (1 − HeaderPos/W)) × fps / 1000
		//
		// Because delaySamples absorbs the difference, all lanes with the same
		// TimeToHeader will have data generated simultaneously arrive at
		// HeaderPos at the same wall-clock time.
		//
		const float fps        = 60.0f;
		const bool  inTimeMode = timeMode.get();
		const float ttH        = inTimeMode ? ofClamp(timeToHeader.get(), 0.0f, 60000.0f) : 0.0f;
		const int   hpos       = inTimeMode ? ofClamp(headerPos.get(), 0, w - 1) : 0;
		const float hFrac      = (float)hpos / (float)w;
		const float twSecs     = inTimeMode ? 0.0f : ofClamp(timeWindow.get(), 0.1f, 240.0f);
		const bool  isPitch    = pitchMode.get();
		const int   wm1        = std::max(w - 1, 1);

		// Mutable per-lane slots — updated by computeLaneTime() and captured
		// by reference inside lerpBuf, so lerpBuf always sees the latest values.
		int samplesVis   = 1;
		int delaySamples = 0;
		int recentIdx    = 0;
		int oldestIdx    = 0;

		auto computeLaneTime = [&](int lane){
			if(inTimeMode){
				float zMs_i  = ofClamp(elem(zoomMs.get(), lane, 3000.0f), 1.0f, 60000.0f);
				samplesVis   = ofClamp((int)(zMs_i / 1000.0f * fps), 1, maxBufferSamples);
				float rawDly = (ttH - zMs_i * (1.0f - hFrac)) / 1000.0f * fps;
				delaySamples = ofClamp((int)rawDly, 0, maxBufferSamples - samplesVis);
			} else {
				samplesVis   = ofClamp((int)(twSecs * fps), 10, maxBufferSamples);
				delaySamples = 0;
			}
			recentIdx = (writeIndex - 1 - delaySamples + maxBufferSamples) % maxBufferSamples;
			oldestIdx = (recentIdx - samplesVis + 1 + maxBufferSamples) % maxBufferSamples;
		};

		// ── build main mesh (persistent — no per-frame allocation) ──────
		// Indices never change once the mesh is the right size; only
		// vertices and colours are updated in-place each frame.
		const int totalQuads = numLanes * w;
		if((int)histMesh.getNumVertices() != totalQuads * 4){
			histMesh.clear();
			histMesh.setMode(OF_PRIMITIVE_TRIANGLES);
			histMesh.getVertices().assign(totalQuads * 4, {0, 0, 0});
			histMesh.getColors()  .assign(totalQuads * 4, ofFloatColor(0, 0, 0, 0));
			// Indices are static — set once and reused every frame
			auto& idx = histMesh.getIndices();
			idx.resize(totalQuads * 6);
			for(int q = 0; q < totalQuads; ++q){
				int vi = q * 4, ii = q * 6;
				idx[ii+0]=vi+0; idx[ii+1]=vi+2; idx[ii+2]=vi+1;
				idx[ii+3]=vi+1; idx[ii+4]=vi+2; idx[ii+5]=vi+3;
			}
		}

		auto& verts  = histMesh.getVertices();
		auto& colors = histMesh.getColors();

		for(int lane = 0; lane < numLanes; ++lane){
			computeLaneTime(lane); // sets samplesVis/delaySamples/recentIdx/oldestIdx
			const int baseQ  = lane * w;
			const int sVis1  = std::max(samplesVis - 1, 1);
			const float fwm1 = (float)(wm1 > 0 ? wm1 : 1);

			for(int px = 0; px < w; ++px){
				// Compute ring-buffer sample position ONCE per pixel.
				// All 6 buffer reads share s0/s1/frac — 6× fewer modulo ops
				// compared to calling a lerpBuf lambda for each channel.
				float rawF = (float)px / fwm1 * (float)sVis1;
				int   i0   = (int)rawF;
				int   i1   = std::min(i0 + 1, sVis1);
				int   s0   = (oldestIdx + i0) % maxBufferSamples;
				int   s1   = (oldestIdx + i1) % maxBufferSamples;
				float frac = rawF - (float)i0;

				// Guard: don't interpolate thickness across a zero crossing —
				// preserves hard note-on/off edges (matches Processing sketch).
				float t0 = thkBufs[lane][s0], t1 = thkBufs[lane][s1];
				bool  jump = (t0 <= 0.0f) != (t1 <= 0.0f);
				float ef   = jump ? 0.0f : frac; // effective frac

				float thk  = ofLerp(t0, t1, ef);
				float v    = ofClamp(ofLerp(valBufs[lane][s0], valBufs[lane][s1], ef), minV, maxV);
				float opa  = ofLerp(opaBufs[lane][s0], opaBufs[lane][s1], ef);
				float hVal = ofLerp(hueBufs[lane][s0], hueBufs[lane][s1], ef);
				float sVal = ofLerp(satBufs[lane][s0], satBufs[lane][s1], ef);
				float lVal = ofLerp(lumBufs[lane][s0], lumBufs[lane][s1], ef);

				float norm  = (v - minV) / range;
				float cy    = isPitch
				              ? h * (1.0f - norm)
				              : h * (1.0f - (norm * 0.9f + 0.05f));
				float halfH = thk * (float)h * 0.5f;
				ofFloatColor fc = hslToColor(hVal, sVal, lVal, opa);

				float x0 = (float)px,     x1 = (float)(px + 1);
				float y0 = cy - halfH,    y1 = cy + halfH;
				int   qi = (baseQ + px) * 4;

				verts [qi+0] = {x0, y0, 0}; verts [qi+1] = {x1, y0, 0};
				verts [qi+2] = {x0, y1, 0}; verts [qi+3] = {x1, y1, 0};
				colors[qi+0] = colors[qi+1] = colors[qi+2] = colors[qi+3] = fc;
			}
		}

		// ── grid FBO (separate texture — always behind histogram and text) ──
		allocFbo(fboGrid, w, h);
		fboGrid.begin();
		ofClear(0, 0, 0, 0);
		if(isPitch && pitchGrid.get()){
			ofEnableBlendMode(OF_BLENDMODE_ALPHA);
			const float gt = gridThickness.get();
			int miMin = (int)std::floor(minV);
			int miMax = (int)std::ceil(maxV);
			for(int midi = miMin; midi <= miMax; midi++){
				float norm2 = (float)(midi - minV) / range;
				float y = h * (1.0f - norm2);
				if(midi % 12 == 0){                            // C = octave boundary
					ofSetLineWidth(gt * 2.0f);
					ofSetColor(255, 255, 255, 90);
				} else if(midi % 12 == 5 || midi % 12 == 7){  // F and G
					ofSetLineWidth(gt * 1.0f);
					ofSetColor(255, 255, 255, 40);
				} else {                                       // remaining semitones
					ofSetLineWidth(gt * 0.5f);
					ofSetColor(255, 255, 255, 20);
				}
				ofDrawLine(0, y, (float)w, y);
			}
			ofSetLineWidth(1.0f);
			ofDisableBlendMode();
		}
		fboGrid.end();
		outputGrid = &fboGrid.getTexture();

		// ── render ────────────────────────────────────────────────────────
		fbo.begin();
		if(showBg.get()){
			ofColor c = bgColor.get();
			ofClear(c.r, c.g, c.b, c.a);
		} else {
			ofClear(0, 0, 0, 0);
		}

		// histogram
		// Reset global colour so stale ofSetColor state (e.g. from grid lines)
		// doesn't multiply-tint the vertex colours.
		ofSetColor(255, 255, 255, 255);
		applyBlendMode(blendMode.get());
		histMesh.draw();

		ofDisableBlendMode();
		fbo.end();
		output = &fbo.getTexture();

		// ── text layer (separate FBO) ─────────────────────────────────────
		fboText.begin();
		ofClear(0, 0, 0, 0);

		if(isPitch && pitxText.get() && notationFont.isLoaded()){
			ofEnableBlendMode(OF_BLENDMODE_ALPHA);

			for(int lane = 0; lane < numLanes; lane++){
				// recompute per-lane time so labels align with each lane's zoom
				computeLaneTime(lane);
				long long oldestCount = (totalWriteCount - 1LL - delaySamples) - (long long)samplesVis + 1LL;

				auto& nots = notationBufs[lane];

				// prune off-screen labels
				nots.erase(
					std::remove_if(nots.begin(), nots.end(),
						[oldestCount](const NoteLabel& nl){ return nl.writeCount < oldestCount; }),
					nots.end());

				for(auto& nl : nots){
					float t = (float)(nl.writeCount - oldestCount) / (float)samplesVis;
					float x = t * (float)w;
					if(x < 0.0f || x > (float)w) continue;

					float norm2 = (float)(nl.midiPitch - minV) / range;
					float y = h * (1.0f - norm2);

					// draw to the LEFT of the onset so it doesn't overlap histogram data.
					// bbox.x is the left bearing, bbox.x+bbox.width is the rightmost pixel
					// extent when drawing at origin, so we solve:
					//   tx + bbox.x + bbox.width = x  →  tx = x - bbox.x - bbox.width
					std::string label = midiToPitchName(nl.midiPitch);
					ofRectangle bbox  = notationFont.getStringBoundingBox(label, 0, 0);
					float tx = x - bbox.x - bbox.width - 10.0f; // 10px gap before onset
					// vertically centre on note position (baseline offset)
					float ty = y + bbox.height * 0.5f;

					// Font lightness: 0=black, 0.5=same as note colour, 1=white
					// fl ∈ [0, 0.5] → lerp black → noteL
					// fl ∈ [0.5, 1] → lerp noteL → white
					float fl   = elem(fontLightness.get(), lane, 0.5f);
					float adjL = (fl <= 0.5f)
					             ? fl * 2.0f * nl.l
					             : nl.l + (fl - 0.5f) * 2.0f * (1.0f - nl.l);
					ofFloatColor fc = hslToColor(nl.h, nl.s, adjL, 230.0f / 255.0f);
					ofSetColor(fc);
					notationFont.drawString(label, tx, ty);
				}
			}

			// ── HeaderPos mask: zero out alpha for x ∈ [0, headerPos) ────
			// Text that has scrolled past the header position becomes fully
			// transparent, as if an inverted mask covers the left region.
			// glBlendFuncSeparate(0,1, 0,0) keeps RGB intact, forces A→0.
			if(hpos > 0){
				glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_ZERO, GL_ZERO);
				ofSetColor(255, 255, 255, 255);
				ofFill();
				ofDrawRectangle(0, 0, (float)hpos, (float)h);
				ofEnableBlendMode(OF_BLENDMODE_ALPHA); // restore
			}
		}

		ofDisableBlendMode();
		fboText.end();
		outputText = &fboText.getTexture();
	}

private:
	// ── helpers ───────────────────────────────────────────────────────────
	static float elem(const std::vector<float>& v, int i, float fallback){
		if(v.empty()) return fallback;
		return v[std::min(i, (int)v.size() - 1)];
	}

	static ofFloatColor hslToColor(float h, float s, float l, float a){
		float r, g, b;
		if(s <= 0.0f){
			r = g = b = l;
		} else {
			auto hue2rgb = [](float p, float q, float t) -> float {
				if(t < 0.0f) t += 1.0f;
				if(t > 1.0f) t -= 1.0f;
				if(t < 1.0f/6) return p + (q - p) * 6.0f * t;
				if(t < 1.0f/2) return q;
				if(t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6.0f;
				return p;
			};
			float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
			float p = 2.0f * l - q;
			r = hue2rgb(p, q, h + 1.0f/3);
			g = hue2rgb(p, q, h);
			b = hue2rgb(p, q, h - 1.0f/3);
		}
		return ofFloatColor(r, g, b, a);
	}

	static void applyBlendMode(int mode){
		switch(mode){
			case 1:  ofEnableBlendMode(OF_BLENDMODE_ADD);      break;
			case 2:  ofEnableBlendMode(OF_BLENDMODE_MULTIPLY); break;
			case 3:  ofEnableBlendMode(OF_BLENDMODE_SCREEN);   break;
			case 4:  ofEnableBlendMode(OF_BLENDMODE_SUBTRACT); break;
			default:
				// OF_BLENDMODE_ALPHA uses the same src/dst factors for every channel,
				// giving A_out = A_src² + A_dst·(1−A_src) — wrong for FBO compositing.
				// glBlendFuncSeparate fixes the alpha channel to the correct over-operator:
				//   A_out = A_src + A_dst·(1−A_src)
				// so the FBO's alpha correctly represents coverage and downstream
				// compositing works without the background bleeding through.
				glEnable(GL_BLEND);
				glBlendFuncSeparate(GL_SRC_ALPHA,         GL_ONE_MINUS_SRC_ALPHA,  // RGB
				                    GL_ONE,               GL_ONE_MINUS_SRC_ALPHA); // Alpha
				break;
		}
	}

	static std::string midiToPitchName(int midi){
		static const char* names[] =
			{"C","C#","D","Eb","E","F","F#","G","G#","A","Bb","B"};
		int octave = (midi / 12) - 1;
		int note   = midi % 12;
		return std::string(names[note]) + ofToString(octave);
	}

	// ── font management ───────────────────────────────────────────────────
	void scanFonts(){
		fontPaths.clear();
		fontNames.clear();
		std::vector<std::string> dirs = {
			ofToDataPath("fonts", true),
			ofToDataPath("", true)
		};
		for(auto& d : dirs){
			ofDirectory dir(d);
			dir.allowExt("ttf"); dir.allowExt("TTF");
			dir.allowExt("otf"); dir.allowExt("OTF");
			dir.listDir();
			for(auto& f : dir.getFiles()){
				fontPaths.push_back(f.getAbsolutePath());
				fontNames.push_back(f.getBaseName());
			}
		}
	}

	void loadFont(){
		if(fontPaths.empty()) return;
		int idx = ofClamp(fontIndex.get(), 0, (int)fontPaths.size() - 1);
		int sz  = ofClamp(fontSize.get(), 6, 96);
		notationFont.load(fontPaths[idx], sz, true, true);
	}

	// ── buffer management ─────────────────────────────────────────────────
	void resizeBuffers(int lanes){
		numLanes = lanes;
		histMesh.clear(); // force index rebuild for new lane count
		valBufs.assign(lanes, std::vector<float>(maxBufferSamples, 0.5f));
		thkBufs.assign(lanes, std::vector<float>(maxBufferSamples, 0.0f));
		opaBufs.assign(lanes, std::vector<float>(maxBufferSamples, 1.0f));
		hueBufs.assign(lanes, std::vector<float>(maxBufferSamples, 0.0f));
		satBufs.assign(lanes, std::vector<float>(maxBufferSamples, 0.8f));
		lumBufs.assign(lanes, std::vector<float>(maxBufferSamples, 0.5f));
		notationBufs.assign(lanes, {});
		lastLabelPitch.assign(lanes, -1);
		writeIndex      = 0;
		totalWriteCount = 0;
	}

	// ── note label struct ─────────────────────────────────────────────────
	struct NoteLabel {
		long long writeCount;
		int       midiPitch;
		float     h, s, l;   // HSL of the lane at onset — used for font coloring
	};

	// ── parameters ────────────────────────────────────────────────────────
	ofParameter<std::vector<float>> input, thickness, opacity, hue, sat, lum;
	ofParameter<float>              minVal, maxVal, timeWindow;
	ofParameter<bool>               freeze;
	ofParameter<int>                fboWidth, fboHeight;
	ofParameter<int>                blendMode;
	ofParameter<bool>               pitchMode, pitxText;
	ofParameter<bool>                    timeMode;
	ofParameter<float>                   timeToHeader;
	ofParameter<std::vector<float>>      zoomMs;
	ofParameter<int>                     headerPos;
	ofParameter<bool>               pitchGrid;
	ofParameter<float>              gridThickness;
	ofParameter<vector<float>>      fontLightness;
	ofParameter<bool>               showBg;
	ofParameter<ofColor>            bgColor;
	ofParameter<int>                fontIndex, fontSize;
	ofParameter<ofTexture*>         output;
	ofParameter<ofTexture*>         outputGrid;
	ofParameter<ofTexture*>         outputText;

	// ── ring buffers [lane][time] ─────────────────────────────────────────
	std::vector<std::vector<float>> valBufs, thkBufs, opaBufs;
	std::vector<std::vector<float>> hueBufs, satBufs, lumBufs;
	int       maxBufferSamples;
	int       writeIndex;
	int       numLanes;
	long long totalWriteCount;

	// ── notation data ─────────────────────────────────────────────────────
	std::vector<std::vector<NoteLabel>> notationBufs;
	std::vector<int>                    lastLabelPitch;   // -1 = no active pitch

	// ── font ──────────────────────────────────────────────────────────────
	ofTrueTypeFont          notationFont;
	std::vector<std::string> fontPaths;
	std::vector<std::string> fontNames;
	ofEventListener          fontIndexListener, fontSizeListener;
	ofEventListener          fboWidthListener,  fboHeightListener;

	ofFbo  fbo;
	ofFbo  fboGrid;
	ofFbo  fboText;
	ofMesh histMesh; // persistent — indices built once, vertices updated in-place
};

#endif /* histogramScore_h */
