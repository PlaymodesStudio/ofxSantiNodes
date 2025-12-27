#ifndef vectorMorphologyVV_h
#define vectorMorphologyVV_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

class vectorMorphologyVV : public ofxOceanodeNodeModel {
public:
	vectorMorphologyVV() : ofxOceanodeNodeModel("Vector Morphology VV") {

		addParameter(input.set(
			"Input",
			std::vector<std::vector<float>>(1, std::vector<float>(1, 0.0f)),
			std::vector<std::vector<float>>(1, std::vector<float>(1, -std::numeric_limits<float>::max())),
			std::vector<std::vector<float>>(1, std::vector<float>(1,  std::numeric_limits<float>::max()))
		));
		addParameter(epsilon.set("Epsilon", 0.0001f, 0.0f, 1.0f));
		addParameter(circular.set("Circular", false));

		addParameterDropdown(operation, "Operation", 0, {
			"centroid",
			"direction",  // temporal motion direction (+1 / -1)
			"bottom",
			"head",
			"width",
			"peak",
			"numBlobs",
			"multiCentroid",
			"multiBottom",
			"multiHead",
			"multiWidth",
			"multiPeak"
		});

		addOutputParameter(outVector.set(
			"Values",
			std::vector<int>(1, -1),
			std::vector<int>(1, -1),
			std::vector<int>(1, INT_MAX)
		));
		addOutputParameter(outVectorVector.set(
			"Multi Values",
			std::vector<std::vector<int>>(1, std::vector<int>(1, -1)),
			std::vector<std::vector<int>>(1, std::vector<int>(1, -1)),
			std::vector<std::vector<int>>(1, std::vector<int>(1, INT_MAX))
		));

		listeners.push(input.newListener([this](std::vector<std::vector<float>>&){ recompute(); }));
		listeners.push(epsilon.newListener([this](float&){ recompute(); }));
		listeners.push(circular.newListener([this](bool&){
			// Reset motion tracking when topology changes
			prevStates.clear();
			recompute();
		}));
		listeners.push(operation.newListener([this](int&){ recompute(); }));

		recompute();
	}

private:
	// =====================================================
	// Blob
	// =====================================================
	struct Blob {
		int start = 0;
		int end = 0;
		double mass = 0.0;
		double weightedSum = 0.0;
		int peakIndex = 0;
		float peakValue = -std::numeric_limits<float>::max();
	};

	// =====================================================
	// Motion tracking state per vector
	// =====================================================
	struct MotionState {
		bool hasPrev = false;
		double prevCentroidUnwrapped = 0.0;
		int prevMotionDir = +1;
	};

	// =====================================================
	// Parameters
	// =====================================================
	ofParameter<std::vector<std::vector<float>>> input;
	ofParameter<float> epsilon;
	ofParameter<bool> circular;
	ofParameter<int> operation;

	ofParameter<std::vector<int>> outVector;
	ofParameter<std::vector<std::vector<int>>> outVectorVector;

	ofEventListeners listeners;

	// Motion tracking state for each input vector
	std::vector<MotionState> prevStates;

	// =====================================================
	// Utilities
	// =====================================================
	static int clampIndex(int i, int n) {
		if(n <= 0) return -1;
		return std::max(0, std::min(i, n - 1));
	}

	void setVectorSafe(const std::vector<int>& v) {
		if(v.empty()) outVector = std::vector<int>(1, -1);
		else outVector = v;
	}

	void setVectorVectorSafe(const std::vector<std::vector<int>>& v) {
		if(v.empty()) outVectorVector = std::vector<std::vector<int>>(1, std::vector<int>(1, -1));
		else outVectorVector = v;
	}

	static bool indexInBlob(const Blob& b, int i) {
		if(b.start <= b.end) return (i >= b.start && i <= b.end);
		return (i >= b.start || i <= b.end); // wrapped
	}

	static int blobWidth(const Blob& b, int n, bool circular) {
		if(circular && b.start > b.end) {
			return (n - b.start) + (b.end + 1);
		}
		return b.end - b.start + 1;
	}

	// =====================================================
	// Centroid output (index)
	// =====================================================
	static int blobCentroidIndex(const Blob& b,
								 const std::vector<float>& v,
								 bool circular) {
		const int n = (int)v.size();
		if(n == 0 || b.mass <= 0.0) return -1;

		if(!circular) {
			int idx = (int)std::lround(b.weightedSum / b.mass);
			return clampIndex(idx, n);
		}

		// Circular centroid via angular mean
		double X = 0.0;
		double Y = 0.0;
		const double twoPi = 2.0 * M_PI;

		for(int i = 0; i < n; ++i) {
			if(!indexInBlob(b, i)) continue;
			const double w = (double)v[i];
			const double theta = twoPi * (double)i / (double)n;
			X += w * std::cos(theta);
			Y += w * std::sin(theta);
		}

		double angle = std::atan2(Y, X);
		if(angle < 0) angle += twoPi;

		int idx = (int)std::lround((angle / twoPi) * n) % n;
		return idx;
	}

	static double blobCentroidUnwrapped(const Blob& b) {
		if(b.mass <= 0.0) return 0.0;
		return b.weightedSum / b.mass;
	}

	static double unwrapNear(double current, double previous, int n) {
		if(n <= 0) return current;
		double best = current;
		double bestDist = std::abs(current - previous);
		for(int k = -2; k <= 2; ++k) {
			double cand = current + (double)k * (double)n;
			double d = std::abs(cand - previous);
			if(d < bestDist) {
				bestDist = d;
				best = cand;
			}
		}
		return best;
	}

	static int signWithHold(double delta, double deadband, int holdDir) {
		if(delta > deadband) return +1;
		if(delta < -deadband) return -1;
		return holdDir;
	}

	// =====================================================
	// Blob extraction (with circular merge)
	// =====================================================
	static std::vector<Blob> extractBlobs(
		const std::vector<float>& v,
		float eps,
		bool circular
	) {
		const int n = (int)v.size();
		std::vector<Blob> blobs;
		if(n == 0) return blobs;

		bool inBlob = false;
		Blob cur;

		for(int i = 0; i < n; ++i) {
			const float val = v[i];
			if(val > eps) {
				if(!inBlob) {
					inBlob = true;
					cur = Blob();
					cur.start = i;
					cur.end = i;
				} else {
					cur.end = i;
				}

				cur.mass += (double)val;
				cur.weightedSum += (double)i * (double)val;

				if(val > cur.peakValue) {
					cur.peakValue = val;
					cur.peakIndex = i;
				}
			}
			else if(inBlob) {
				blobs.push_back(cur);
				inBlob = false;
			}
		}
		if(inBlob) blobs.push_back(cur);

		// Merge last + first if circular and touching boundary
		if(circular && blobs.size() > 1) {
			Blob& first = blobs.front();
			Blob& last  = blobs.back();

			if(first.start == 0 && last.end == n - 1) {
				Blob merged;
				merged.start = last.start;
				merged.end   = first.end;
				merged.mass  = first.mass + last.mass;

				merged.weightedSum =
					last.weightedSum +
					first.weightedSum +
					(double)n * first.mass;

				if(first.peakValue >= last.peakValue) {
					merged.peakValue = first.peakValue;
					merged.peakIndex = first.peakIndex;
				} else {
					merged.peakValue = last.peakValue;
					merged.peakIndex = last.peakIndex;
				}

				blobs.erase(blobs.begin());
				blobs.pop_back();
				blobs.insert(blobs.begin(), merged);
			}
		}

		return blobs;
	}

	// =====================================================
	// Process single vector
	// =====================================================
	struct SingleResult {
		int value = -1;
		std::vector<int> multiValues;
	};

	SingleResult processSingleVector(const std::vector<float>& v, MotionState& state) {
		SingleResult result;
		result.multiValues = { -1 };

		const int n = (int)v.size();
		if(n == 0) return result;

		const bool circ = circular.get();
		auto blobs = extractBlobs(v, epsilon.get(), circ);
		if(blobs.empty()) return result;

		// Dominant blob = max mass
		auto domIt = std::max_element(
			blobs.begin(), blobs.end(),
			[](const Blob& a, const Blob& b){ return a.mass < b.mass; }
		);
		const Blob& dom = *domIt;

		const int centroidIdx = blobCentroidIndex(dom, v, circ);

		// Temporal motion direction
		double cNow = blobCentroidUnwrapped(dom);
		if(circ && state.hasPrev) {
			cNow = unwrapNear(cNow, state.prevCentroidUnwrapped, n);
		}

		int motionDir = state.prevMotionDir;
		if(state.hasPrev) {
			const double deadband = 1e-6;
			const double delta = cNow - state.prevCentroidUnwrapped;
			motionDir = signWithHold(delta, deadband, state.prevMotionDir);
		}

		const int bottom = (motionDir > 0) ? dom.start : dom.end;
		const int head   = (motionDir > 0) ? dom.end   : dom.start;

		// Update motion state
		state.prevCentroidUnwrapped = cNow;
		state.prevMotionDir = motionDir;
		state.hasPrev = true;

		// Emit selected operation
		switch(operation.get()) {
			case 0: result.value = centroidIdx; break;
			case 1: result.value = motionDir; break;
			case 2: result.value = clampIndex(bottom, n); break;
			case 3: result.value = clampIndex(head, n); break;
			case 4: result.value = blobWidth(dom, n, circ); break;
			case 5: result.value = clampIndex(dom.peakIndex, n); break;
			case 6: result.value = (int)blobs.size(); break;

			case 7: { // multiCentroid
				result.multiValues.clear();
				result.multiValues.reserve(blobs.size());
				for(const auto& b : blobs)
					result.multiValues.push_back(blobCentroidIndex(b, v, circ));
			} break;

			case 8: { // multiBottom
				result.multiValues.clear();
				result.multiValues.reserve(blobs.size());
				for(const auto& b : blobs)
					result.multiValues.push_back(clampIndex(b.start, n));
			} break;

			case 9: { // multiHead
				result.multiValues.clear();
				result.multiValues.reserve(blobs.size());
				for(const auto& b : blobs)
					result.multiValues.push_back(clampIndex(b.end, n));
			} break;

			case 10: { // multiWidth
				result.multiValues.clear();
				result.multiValues.reserve(blobs.size());
				for(const auto& b : blobs)
					result.multiValues.push_back(blobWidth(b, n, circ));
			} break;

			case 11: { // multiPeak
				result.multiValues.clear();
				result.multiValues.reserve(blobs.size());
				for(const auto& b : blobs)
					result.multiValues.push_back(clampIndex(b.peakIndex, n));
			} break;

			default: break;
		}

		return result;
	}

	// =====================================================
	// Main compute
	// =====================================================
	void recompute() {
		const auto& vv = input.get();
		const int numVectors = (int)vv.size();

		// Resize motion state tracking
		if((int)prevStates.size() != numVectors) {
			prevStates.resize(numVectors);
		}

		std::vector<int> values;
		std::vector<std::vector<int>> multiValues;

		values.reserve(numVectors);
		multiValues.reserve(numVectors);

		// Process each vector
		for(int i = 0; i < numVectors; ++i) {
			SingleResult res = processSingleVector(vv[i], prevStates[i]);
			values.push_back(res.value);
			multiValues.push_back(res.multiValues);
		}

		setVectorSafe(values);
		setVectorVectorSafe(multiValues);
	}
};

#endif /* vectorMorphologyVV_h */
