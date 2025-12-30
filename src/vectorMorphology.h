#ifndef vectorMorphology_h
#define vectorMorphology_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

class vectorMorphology : public ofxOceanodeNodeModel {
public:
	vectorMorphology() : ofxOceanodeNodeModel("Vector Morphology") {

		addParameter(input.set(
			"Input",
			std::vector<float>(1, 0.0f),
			std::vector<float>(1, -std::numeric_limits<float>::max()),
			std::vector<float>(1,  std::numeric_limits<float>::max())
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

		addOutputParameter(outInt.set("Value", -1, -1, INT_MAX));
		addOutputParameter(outVector.set(
			"Values",
			std::vector<int>(1, -1),
			std::vector<int>(1, -1),
			std::vector<int>(1, INT_MAX)
		));

		listeners.push(input.newListener([this](std::vector<float>&){ recompute(); }));
		listeners.push(epsilon.newListener([this](float&){ recompute(); }));
		listeners.push(circular.newListener([this](bool&){
			// Reset motion tracking when topology changes
			hasPrev = false;
			prevMotionDir = +1;
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

		// NOTE:
		// For circular merged blobs (last+first), this weightedSum is UNWRAPPED:
		// indices 0..end are treated as n..n+end by adding n*mass(firstSegment).
		double weightedSum = 0.0;

		int peakIndex = 0;
		float peakValue = -std::numeric_limits<float>::max();
	};

	// =====================================================
	// Parameters
	// =====================================================
	ofParameter<std::vector<float>> input;
	ofParameter<float> epsilon;
	ofParameter<bool> circular;
	ofParameter<int> operation;

	ofParameter<int> outInt;
	ofParameter<std::vector<int>> outVector;

	ofEventListeners listeners;

	// =====================================================
	// Motion tracking state (dominant blob)
	// =====================================================
	bool hasPrev = false;
	double prevCentroidUnwrapped = 0.0;
	int prevMotionDir = +1; // +1 increasing index, -1 decreasing

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

	// Unwrapped centroid position (for motion)
	static double blobCentroidUnwrapped(const Blob& b) {
		if(b.mass <= 0.0) return 0.0;
		return b.weightedSum / b.mass;
	}

	// In circular mode, shift centroid by +/- k*n to be nearest previous
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

				// Unwrap the first segment by +n
				merged.weightedSum =
					last.weightedSum +
					first.weightedSum +
					(double)n * first.mass;

				// Peak: pick strongest
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
	// Main compute
	// =====================================================
	void recompute() {
		const auto& v = input.get();
		const int n = (int)v.size();

		outInt = -1;
		setVectorSafe({ -1 });
		if(n == 0) return;

		const bool circ = circular.get();
		auto blobs = extractBlobs(v, epsilon.get(), circ);
		if(blobs.empty()) return;

		// Dominant blob = max mass
		auto domIt = std::max_element(
			blobs.begin(), blobs.end(),
			[](const Blob& a, const Blob& b){ return a.mass < b.mass; }
		);
		const Blob& dom = *domIt;

		// Centroid output index (what you patch with)
		const int centroidIdx = blobCentroidIndex(dom, v, circ);

		// Temporal motion direction (both modes)
		double cNow = blobCentroidUnwrapped(dom);
		if(circ && hasPrev) {
			cNow = unwrapNear(cNow, prevCentroidUnwrapped, n);
		}

		int motionDir = prevMotionDir;
		if(hasPrev) {
			// Deadband avoids jitter when centroid barely moves
			const double deadband = 1e-6;
			const double delta = cNow - prevCentroidUnwrapped;
			motionDir = signWithHold(delta, deadband, prevMotionDir);
		}

		// Head/bottom based on temporal motion direction (both modes)
		// +1 means moving towards increasing index (mod n if circular)
		// -1 means moving towards decreasing index
		const int bottom = (motionDir > 0) ? dom.start : dom.end;
		const int head   = (motionDir > 0) ? dom.end   : dom.start;

		// Update motion state
		if(!hasPrev) {
			prevCentroidUnwrapped = cNow;
			prevMotionDir = motionDir;
			hasPrev = true;
		} else {
			prevCentroidUnwrapped = cNow;
			prevMotionDir = motionDir;
		}

		// Emit selected operation
		switch(operation.get()) {

			case 0: outInt = centroidIdx; break;                 // centroid
			case 1: outInt = motionDir; break;                   // direction (temporal)
			case 2: outInt = clampIndex(bottom, n); break;       // bottom (temporal)
			case 3: outInt = clampIndex(head, n); break;         // head (temporal)
			case 4: outInt = blobWidth(dom, n, circ); break;     // width
			case 5: outInt = clampIndex(dom.peakIndex, n); break;// peak
			case 6: outInt = (int)blobs.size(); break;           // numBlobs

			case 7: { // multiCentroid
				std::vector<int> r;
				r.reserve(blobs.size());
				for(const auto& b : blobs)
					r.push_back(blobCentroidIndex(b, v, circ));
				setVectorSafe(r);
			} break;

			// NOTE:
			// True temporal head/bottom per blob needs per-blob tracking/IDs.
			// For now we define them structurally (+1 => start->end) to keep deterministic.
			case 8: { // multiBottom
				std::vector<int> r;
				r.reserve(blobs.size());
				for(const auto& b : blobs)
					r.push_back(clampIndex(b.start, n));
				setVectorSafe(r);
			} break;

			case 9: { // multiHead
				std::vector<int> r;
				r.reserve(blobs.size());
				for(const auto& b : blobs)
					r.push_back(clampIndex(b.end, n));
				setVectorSafe(r);
			} break;

			case 10: { // multiWidth
				std::vector<int> r;
				r.reserve(blobs.size());
				for(const auto& b : blobs)
					r.push_back(blobWidth(b, n, circ));
				setVectorSafe(r);
			} break;

			case 11: { // multiPeak
				std::vector<int> r;
				r.reserve(blobs.size());
				for(const auto& b : blobs)
					r.push_back(clampIndex(b.peakIndex, n));
				setVectorSafe(r);
			} break;

			default: break;
		}
	}
};

#endif /* vectorMorphology_h */
