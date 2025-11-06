#pragma once
#include "ofxOceanodeNodeModel.h"
#include <numeric>
#include <cmath>
#include <limits>

class metaballAnalyzer : public ofxOceanodeNodeModel {
public:
	metaballAnalyzer() : ofxOceanodeNodeModel("metaballAnalyzer") {}

	void setup() override {
		addParameter(xs.set("X", {0.0f}, {0.0f}, {1.0f}));
		addParameter(ys.set("Y", {0.0f}, {0.0f}, {1.0f}));

		addParameter(protThresh.set("ProtThresh", 1.1f, 1.0f, 2.0f));
		addParameter(derivScale.set("DerivScale", 1.0f, 0.0f, 1000.0f));

		// si un punt es mou més que això d’un frame a l’altre → punt glitxat
		addParameter(glitchThresh.set("GlitchThresh", 20.0f, 0.0f, 1000.0f));
		addParameter(glitchBlend.set("GlitchBlend", 1.0f, 0.0f, 1.0f));

		// si la mitjana de tots els punts canvia més que això → frame glitxat
		addParameter(frameDiffThresh.set("FrameDiffThresh", 15.0f, 0.0f, 1000.0f));

		// outputs base
		addOutputParameter(centX.set("CentX", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(centY.set("CentY", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(width.set("Width", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(height.set("Height", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(aspect.set("Aspect", 0.0f, 0.0f, FLT_MAX));

		addOutputParameter(area.set("Area", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(perimeter.set("Perimeter", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(req.set("Req", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(circularity.set("Circularity", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(roughness.set("Roughness", 0.0f, 0.0f, FLT_MAX));

		addOutputParameter(radMean.set("RadMean", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(radStdNorm.set("RadStdNorm", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(numProt.set("NumProtrusions", 0.0f, 0.0f, FLT_MAX));

		addOutputParameter(valid.set("Valid", 0.0f, 0.0f, 1.0f));

		// derivades
		addOutputParameter(dCentX.set("dCentX", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dCentY.set("dCentY", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dArea.set("dArea", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dPerimeter.set("dPerimeter", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dCircularity.set("dCircularity", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dRoughness.set("dRoughness", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dRadStdNorm.set("dRadStdNorm", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dNumProt.set("dNumProtrusions", 0.0f, -FLT_MAX, FLT_MAX));

		listenerX = xs.newListener([this](vector<float> &){ compute(); });
		listenerY = ys.newListener([this](vector<float> &){ compute(); });

		firstFrame = true;
	}

private:
	// inputs
	ofParameter<vector<float>> xs;
	ofParameter<vector<float>> ys;
	ofParameter<float> protThresh;
	ofParameter<float> derivScale;
	ofParameter<float> glitchThresh;
	ofParameter<float> glitchBlend;
	ofParameter<float> frameDiffThresh;

	// outputs
	ofParameter<float> centX, centY;
	ofParameter<float> width, height, aspect;
	ofParameter<float> area, perimeter, req, circularity, roughness;
	ofParameter<float> radMean, radStdNorm, numProt;
	ofParameter<float> valid;

	// derivs
	ofParameter<float> dCentX, dCentY;
	ofParameter<float> dArea, dPerimeter, dCircularity, dRoughness;
	ofParameter<float> dRadStdNorm, dNumProt;

	// prev values
	bool firstFrame;
	float pCentX = 0.0f, pCentY = 0.0f;
	float pArea = 0.0f, pPerimeter = 0.0f, pCircularity = 0.0f, pRoughness = 0.0f;
	float pRadStdNorm = 0.0f, pNumProt = 0.0f;

	// last good frame
	std::vector<float> lastX;
	std::vector<float> lastY;

	ofEventListener listenerX, listenerY;

	static inline float safeFloat(float v, float fb = 0.0f){
		return (std::isnan(v) || std::isinf(v)) ? fb : v;
	}

	void compute(){
		std::vector<float> fx = xs.get();
		std::vector<float> fy = ys.get();

		// primera vegada: processa i desa
		if(firstFrame){
			processFiltered(fx, fy);
			lastX = fx;
			lastY = fy;
			firstFrame = false;
			return;
		}

		// COMPROVACIÓ DE FRAME ENTER: mida igual, mirem diferència mitjana
		if(fx.size() == lastX.size() && fy.size() == lastY.size()){
			float avgDist = avgFrameDiff(fx, fy, lastX, lastY);
			if(avgDist > frameDiffThresh.get()){
				// frame glitxat → reutilitzem l’anterior
				processFiltered(lastX, lastY);
				return;
			}

			// si no és glitxat, fem també el filtre punt a punt
			filterPointGlitches(fx, fy, lastX, lastY);
		}
		// si per alguna raó la mida no coincideix (tot i que dius que no canviarà), agafem tal qual

		processFiltered(fx, fy);
		// guardar com a bo
		lastX = fx;
		lastY = fy;
	}

	float avgFrameDiff(const std::vector<float> &x1, const std::vector<float> &y1,
					   const std::vector<float> &x2, const std::vector<float> &y2)
	{
		size_t n = std::min( std::min(x1.size(), y1.size()), std::min(x2.size(), y2.size()) );
		if(n == 0) return 0.0f;
		double acc = 0.0;
		for(size_t i=0;i<n;++i){
			double dx = (double)x1[i] - (double)x2[i];
			double dy = (double)y1[i] - (double)y2[i];
			acc += std::sqrt(dx*dx + dy*dy);
		}
		return (float)(acc / (double)n);
	}

	void filterPointGlitches(std::vector<float> &fx, std::vector<float> &fy,
							 const std::vector<float> &px, const std::vector<float> &py)
	{
		float th = glitchThresh.get();
		float blend = glitchBlend.get();
		size_t n = std::min(fx.size(), fy.size());
		for(size_t i=0;i<n;++i){
			float dx = fx[i] - px[i];
			float dy = fy[i] - py[i];
			float d  = std::sqrt(dx*dx + dy*dy);
			if(d > th){
				fx[i] = px[i] * blend + fx[i] * (1.0f - blend);
				fy[i] = py[i] * blend + fy[i] * (1.0f - blend);
			}
		}
	}

	void processFiltered(const std::vector<float> &vx, const std::vector<float> &vy){
		size_t n = std::min(vx.size(), vy.size());
		if(n < 3){
			setAllZero();
			return;
		}

		// centroid
		double sx=0.0, sy=0.0;
		for(size_t i=0;i<n;++i){ sx += vx[i]; sy += vy[i]; }
		double cx = sx / (double)n;
		double cy = sy / (double)n;
		float cxF = safeFloat((float)cx);
		float cyF = safeFloat((float)cy);

		// bbox
		double minx = std::numeric_limits<double>::infinity();
		double maxx = -std::numeric_limits<double>::infinity();
		double miny = std::numeric_limits<double>::infinity();
		double maxy = -std::numeric_limits<double>::infinity();
		for(size_t i=0;i<n;++i){
			double xx = vx[i], yy = vy[i];
			if(xx < minx) minx = xx;
			if(xx > maxx) maxx = xx;
			if(yy < miny) miny = yy;
			if(yy > maxy) maxy = yy;
		}
		float wF = safeFloat((float)(maxx - minx));
		float hF = safeFloat((float)(maxy - miny));
		float aspectF = (hF > 0.0f) ? safeFloat(wF / hF) : 0.0f;

		// area (shoelace)
		double accA = 0.0;
		for(size_t i=0;i<n;++i){
			size_t j = (i+1)%n;
			accA += (double)vx[i] * (double)vy[j] - (double)vx[j] * (double)vy[i];
		}
		double A = std::abs(accA) * 0.5;
		float AF = safeFloat((float)A);

		// perimeter
		double accP = 0.0;
		for(size_t i=0;i<n;++i){
			size_t j = (i+1)%n;
			double dx = (double)vx[j] - (double)vx[i];
			double dy = (double)vy[j] - (double)vy[i];
			accP += std::sqrt(dx*dx + dy*dy);
		}
		float PF = safeFloat((float)accP);

		// eq radius
		double r_eq = (A > 0.0) ? std::sqrt(A / M_PI) : 0.0;
		float rEqF = safeFloat((float)r_eq);

		// circularity
		float circF = 0.0f;
		if(PF > 0.0f){
			circF = safeFloat((float)((4.0 * M_PI * A) / (accP * accP)));
		}

		// roughness
		float roughF = 0.0f;
		if(r_eq > 0.0){
			double perimCircle = 2.0 * M_PI * r_eq;
			roughF = safeFloat((float)(accP / perimCircle));
		}

		// radial
		std::vector<double> rad;
		rad.reserve(n);
		for(size_t i=0;i<n;++i){
			double dx = (double)vx[i] - cx;
			double dy = (double)vy[i] - cy;
			rad.push_back(std::sqrt(dx*dx + dy*dy));
		}
		double rSum = std::accumulate(rad.begin(), rad.end(), 0.0);
		double rMean = rSum / (double)n;
		float rMeanF = safeFloat((float)rMean);

		double var = 0.0;
		for(auto &rv : rad){
			double d = rv - rMean;
			var += d*d;
		}
		var /= (double)n;
		double rStd = std::sqrt(var);
		double rStdNormD = (rMean > 0.0) ? (rStd / rMean) : 0.0;
		float rStdNormF = safeFloat((float)rStdNormD);

		// protus
		double th = rMean * (double)protThresh.get();
		int countProt = 0;
		for(auto &rv : rad){
			if(rv >= th) countProt++;
		}
		float nProtF = (float)countProt;

		// derivades
		float s = derivScale.get();
		if(firstFrame){
			dCentX = dCentY = 0.0f;
			dArea = dPerimeter = 0.0f;
			dCircularity = dRoughness = 0.0f;
			dRadStdNorm = dNumProt = 0.0f;
			firstFrame = false;
		}else{
			dCentX = (cxF - pCentX) * s;
			dCentY = (cyF - pCentY) * s;
			dArea = (AF - pArea) * s;
			dPerimeter = (PF - pPerimeter) * s;
			dCircularity = (circF - pCircularity) * s;
			dRoughness   = (roughF - pRoughness) * s;
			dRadStdNorm  = (rStdNormF - pRadStdNorm) * s;
			dNumProt     = (nProtF - pNumProt) * s;
		}

		// store prev
		pCentX = cxF;
		pCentY = cyF;
		pArea = AF;
		pPerimeter = PF;
		pCircularity = circF;
		pRoughness = roughF;
		pRadStdNorm = rStdNormF;
		pNumProt = nProtF;

		// outputs
		centX = cxF;
		centY = cyF;
		width = wF;
		height = hF;
		aspect = aspectF;
		area = AF;
		perimeter = PF;
		req = rEqF;
		circularity = circF;
		roughness = roughF;
		radMean = rMeanF;
		radStdNorm = rStdNormF;
		numProt = nProtF;
		valid = 1.0f;

		// sanitize derivs
		dCentX = safeFloat(dCentX.get());
		dCentY = safeFloat(dCentY.get());
		dArea = safeFloat(dArea.get());
		dPerimeter = safeFloat(dPerimeter.get());
		dCircularity = safeFloat(dCircularity.get());
		dRoughness = safeFloat(dRoughness.get());
		dRadStdNorm = safeFloat(dRadStdNorm.get());
		dNumProt = safeFloat(dNumProt.get());
	}

	void setAllZero(){
		centX = centY = 0.0f;
		width = height = aspect = 0.0f;
		area = perimeter = req = 0.0f;
		circularity = roughness = 0.0f;
		radMean = radStdNorm = 0.0f;
		numProt = 0.0f;
		dCentX = dCentY = 0.0f;
		dArea = dPerimeter = 0.0f;
		dCircularity = dRoughness = 0.0f;
		dRadStdNorm = dNumProt = 0.0f;
		valid = 0.0f;
	}
};
