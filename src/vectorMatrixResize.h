#ifndef vectorMatrixResize_h
#define vectorMatrixResize_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>
#include <cmath>

class vectorMatrixResize : public ofxOceanodeNodeModel {
public:
	vectorMatrixResize() : ofxOceanodeNodeModel("Vector Matrix Resize") {}
	
	void setup() override {
		description = "Resamples a 2D matrix (stored as a 1D vector) to a new size. Input is treated as a row-major matrix with dimensions inWidth x inHeight, and output is resized to outWidth x outHeight.";
		
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(inWidth.set("In Width", 1, 1, INT_MAX));
		addParameter(inHeight.set("In Height", 1, 1, INT_MAX));
		addParameter(outWidth.set("Out Width", 1, 1, INT_MAX));
		addParameter(outHeight.set("Out Height", 1, 1, INT_MAX));
		addParameterDropdown(interp, "Interp", 0, {"Nearest", "Bilinear", "Min", "Max", "Average"});
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		listeners.push(input.newListener([this](vector<float> &v) {
			process();
		}));
		
		listeners.push(inWidth.newListener([this](int &i) {
			process();
		}));
		
		listeners.push(inHeight.newListener([this](int &i) {
			process();
		}));
		
		listeners.push(outWidth.newListener([this](int &i) {
			process();
		}));
		
		listeners.push(outHeight.newListener([this](int &i) {
			process();
		}));
		
		listeners.push(interp.newListener([this](int &i) {
			process();
		}));
	}
	
	void loadBeforeConnections(ofJson &json) override {
		deserializeParameter(json, inWidth);
		deserializeParameter(json, inHeight);
		deserializeParameter(json, outWidth);
		deserializeParameter(json, outHeight);
	}
	
private:
	void process() {
		const vector<float>& in = input.get();
		int srcW = inWidth.get();
		int srcH = inHeight.get();
		int dstW = outWidth.get();
		int dstH = outHeight.get();
		
		if (in.empty() || srcW < 1 || srcH < 1 || dstW < 1 || dstH < 1) {
			output = vector<float>(dstW * dstH, 0);
			return;
		}
		
		// Ensure input size matches expected matrix size (pad with zeros or truncate)
		vector<float> srcData(srcW * srcH, 0);
		size_t copySize = std::min(in.size(), srcData.size());
		std::copy(in.begin(), in.begin() + copySize, srcData.begin());
		
		vector<float> result(dstW * dstH, 0);
		
		int interpMode = interp.get();
		
		for (int dstY = 0; dstY < dstH; dstY++) {
			for (int dstX = 0; dstX < dstW; dstX++) {
				// Map destination coordinates to source coordinates
				float srcX = (dstW > 1) ? (float)dstX * (srcW - 1) / (dstW - 1) : 0;
				float srcY = (dstH > 1) ? (float)dstY * (srcH - 1) / (dstH - 1) : 0;
				
				// Handle edge case when destination is 1 pixel
				if (dstW == 1) srcX = (srcW - 1) * 0.5f;
				if (dstH == 1) srcY = (srcH - 1) * 0.5f;
				
				float value = 0;
				
				switch (interpMode) {
					case 0: // Nearest neighbor
						value = sampleNearest(srcData, srcW, srcH, srcX, srcY);
						break;
					case 1: // Bilinear
						value = sampleBilinear(srcData, srcW, srcH, srcX, srcY);
						break;
					case 2: // Min
						value = sampleMin(srcData, srcW, srcH, srcX, srcY, dstW, dstH);
						break;
					case 3: // Max
						value = sampleMax(srcData, srcW, srcH, srcX, srcY, dstW, dstH);
						break;
					case 4: // Average
						value = sampleAverage(srcData, srcW, srcH, srcX, srcY, dstW, dstH);
						break;
				}
				
				result[dstY * dstW + dstX] = value;
			}
		}
		
		output = result;
	}
	
	float getPixel(const vector<float>& data, int w, int h, int x, int y) {
		x = std::max(0, std::min(w - 1, x));
		y = std::max(0, std::min(h - 1, y));
		return data[y * w + x];
	}
	
	float sampleNearest(const vector<float>& data, int w, int h, float x, float y) {
		int ix = (int)round(x);
		int iy = (int)round(y);
		return getPixel(data, w, h, ix, iy);
	}
	
	float sampleBilinear(const vector<float>& data, int w, int h, float x, float y) {
		int x0 = (int)floor(x);
		int y0 = (int)floor(y);
		int x1 = x0 + 1;
		int y1 = y0 + 1;
		
		float fx = x - x0;
		float fy = y - y0;
		
		float v00 = getPixel(data, w, h, x0, y0);
		float v10 = getPixel(data, w, h, x1, y0);
		float v01 = getPixel(data, w, h, x0, y1);
		float v11 = getPixel(data, w, h, x1, y1);
		
		float v0 = v00 * (1 - fx) + v10 * fx;
		float v1 = v01 * (1 - fx) + v11 * fx;
		
		return v0 * (1 - fy) + v1 * fy;
	}
	
	// Get the region of source pixels that contribute to a destination pixel
	void getSourceRegion(int srcW, int srcH, int dstW, int dstH, int dstX, int dstY,
						 int& x0, int& y0, int& x1, int& y1) {
		float scaleX = (float)srcW / dstW;
		float scaleY = (float)srcH / dstH;
		
		x0 = (int)floor(dstX * scaleX);
		y0 = (int)floor(dstY * scaleY);
		x1 = (int)ceil((dstX + 1) * scaleX);
		y1 = (int)ceil((dstY + 1) * scaleY);
		
		x0 = std::max(0, std::min(srcW - 1, x0));
		y0 = std::max(0, std::min(srcH - 1, y0));
		x1 = std::max(0, std::min(srcW, x1));
		y1 = std::max(0, std::min(srcH, y1));
		
		// Ensure at least one pixel
		if (x1 <= x0) x1 = x0 + 1;
		if (y1 <= y0) y1 = y0 + 1;
	}
	
	float sampleMin(const vector<float>& data, int srcW, int srcH, float x, float y, int dstW, int dstH) {
		int dstX = (dstW > 1) ? (int)round(x * (dstW - 1) / (srcW - 1)) : 0;
		int dstY = (dstH > 1) ? (int)round(y * (dstH - 1) / (srcH - 1)) : 0;
		
		int x0, y0, x1, y1;
		getSourceRegion(srcW, srcH, dstW, dstH, dstX, dstY, x0, y0, x1, y1);
		
		float minVal = FLT_MAX;
		for (int py = y0; py < y1; py++) {
			for (int px = x0; px < x1; px++) {
				float val = getPixel(data, srcW, srcH, px, py);
				if (val < minVal) minVal = val;
			}
		}
		return minVal;
	}
	
	float sampleMax(const vector<float>& data, int srcW, int srcH, float x, float y, int dstW, int dstH) {
		int dstX = (dstW > 1) ? (int)round(x * (dstW - 1) / (srcW - 1)) : 0;
		int dstY = (dstH > 1) ? (int)round(y * (dstH - 1) / (srcH - 1)) : 0;
		
		int x0, y0, x1, y1;
		getSourceRegion(srcW, srcH, dstW, dstH, dstX, dstY, x0, y0, x1, y1);
		
		float maxVal = -FLT_MAX;
		for (int py = y0; py < y1; py++) {
			for (int px = x0; px < x1; px++) {
				float val = getPixel(data, srcW, srcH, px, py);
				if (val > maxVal) maxVal = val;
			}
		}
		return maxVal;
	}
	
	float sampleAverage(const vector<float>& data, int srcW, int srcH, float x, float y, int dstW, int dstH) {
		int dstX = (dstW > 1) ? (int)round(x * (dstW - 1) / (srcW - 1)) : 0;
		int dstY = (dstH > 1) ? (int)round(y * (dstH - 1) / (srcH - 1)) : 0;
		
		int x0, y0, x1, y1;
		getSourceRegion(srcW, srcH, dstW, dstH, dstX, dstY, x0, y0, x1, y1);
		
		float sum = 0;
		int count = 0;
		for (int py = y0; py < y1; py++) {
			for (int px = x0; px < x1; px++) {
				sum += getPixel(data, srcW, srcH, px, py);
				count++;
			}
		}
		return (count > 0) ? sum / count : 0;
	}
	
	ofParameter<vector<float>> input;
	ofParameter<int> inWidth;
	ofParameter<int> inHeight;
	ofParameter<int> outWidth;
	ofParameter<int> outHeight;
	ofParameter<int> interp;
	ofParameter<vector<float>> output;
	
	ofEventListeners listeners;
};

#endif /* vectorMatrixResize_h */
