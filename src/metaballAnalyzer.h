#pragma once
#include "ofxOceanodeNodeModel.h"
#include <numeric>
#include <cmath>
#include <limits>

class metaballAnalyzer : public ofxOceanodeNodeModel {
public:
	metaballAnalyzer() : ofxOceanodeNodeModel("metaballAnalyzer") {}

	void setup() override {
		// inputs (ja deglitxats)
		addParameter(xs.set("X", {0.0f}, {0.0f}, {1.0f}));
		addParameter(ys.set("Y", {0.0f}, {0.0f}, {1.0f}));

		// params
		addParameter(protThresh.set("ProtThresh", 1.1f, 1.0f, 2.0f));
		addParameter(derivScale.set("DerivScale", 1.0f, 0.0f, 1000.0f));

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

		// derivades bàsiques
		addOutputParameter(dCentX.set("dCentX", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dCentY.set("dCentY", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dArea.set("dArea", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dPerimeter.set("dPerimeter", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dCircularity.set("dCircularity", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dRoughness.set("dRoughness", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dRadStdNorm.set("dRadStdNorm", 0.0f, -FLT_MAX, FLT_MAX));
		addOutputParameter(dNumProt.set("dNumProtrusions", 0.0f, -FLT_MAX, FLT_MAX));

		// features derivats / musicals
		addOutputParameter(centroidSpeed.set("CentroidSpeed", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(blobiness.set("Blobiness", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(deformationSpeed.set("DeformationSpeed", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(sizeChange.set("SizeChange", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(activity.set("Activity", 0.0f, 0.0f, FLT_MAX));

		listenerX = xs.newListener([this](std::vector<float> &){ compute(); });
		listenerY = ys.newListener([this](std::vector<float> &){ compute(); });

		firstFrame = true;
	}

private:
	// inputs
	ofParameter<std::vector<float>> xs, ys;
	ofParameter<float> protThresh, derivScale;

	// base outputs
	ofParameter<float> centX, centY;
	ofParameter<float> width, height, aspect;
	ofParameter<float> area, perimeter, req, circularity, roughness;
	ofParameter<float> radMean, radStdNorm, numProt;
	ofParameter<float> valid;

	// derivs
	ofParameter<float> dCentX, dCentY;
	ofParameter<float> dArea, dPerimeter, dCircularity, dRoughness;
	ofParameter<float> dRadStdNorm, dNumProt;

	// derived
	ofParameter<float> centroidSpeed, blobiness, deformationSpeed, sizeChange, activity;

	bool firstFrame;
	float pCentX=0, pCentY=0;
	float pArea=0, pPerimeter=0, pCircularity=0, pRoughness=0;
	float pRadStdNorm=0, pNumProt=0;

	ofEventListener listenerX, listenerY;

	static inline float safe(float v){ return (std::isnan(v) || std::isinf(v)) ? 0.f : v; }
	static inline float absf(float v){ return v < 0 ? -v : v; }

	void compute(){
		const auto &vx = xs.get();
		const auto &vy = ys.get();
		size_t n = std::min(vx.size(), vy.size());
		if(n < 3){ setZeros(); return; }

		// centroid
		double sx=0, sy=0;
		for(size_t i=0;i<n;++i){ sx += vx[i]; sy += vy[i]; }
		double cx = sx / (double)n;
		double cy = sy / (double)n;
		float cxF = safe((float)cx);
		float cyF = safe((float)cy);

		// bbox
		double minx=INFINITY,maxx=-INFINITY,miny=INFINITY,maxy=-INFINITY;
		for(size_t i=0;i<n;++i){
			double xx=vx[i], yy=vy[i];
			if(xx<minx)minx=xx; if(xx>maxx)maxx=xx;
			if(yy<miny)miny=yy; if(yy>maxy)maxy=yy;
		}
		float wF=safe(maxx-minx), hF=safe(maxy-miny);
		float aspectF = (hF>0)?safe(wF/hF):0;

		// area (shoelace)
		double accA=0;
		for(size_t i=0;i<n;++i){
			size_t j=(i+1)%n;
			accA+=vx[i]*vy[j]-vx[j]*vy[i];
		}
		double A=fabs(accA)*0.5;
		float AF=safe((float)A);

		// perimeter
		double accP=0;
		for(size_t i=0;i<n;++i){
			size_t j=(i+1)%n;
			double dx=vx[j]-vx[i], dy=vy[j]-vy[i];
			accP+=sqrt(dx*dx+dy*dy);
		}
		float PF=safe((float)accP);

		// equivalent radius
		double r_eq=(A>0)?sqrt(A/M_PI):0;
		float rEqF=safe((float)r_eq);

		// circularity
		float circF=(PF>0)?safe((float)((4*M_PI*A)/(accP*accP))):0;
		// roughness
		float roughF=(r_eq>0)?safe((float)(accP/(2*M_PI*r_eq))):0;

		// radial
		std::vector<double> rad(n);
		for(size_t i=0;i<n;++i){
			double dx=vx[i]-cx, dy=vy[i]-cy;
			rad[i]=sqrt(dx*dx+dy*dy);
		}
		double rSum=std::accumulate(rad.begin(),rad.end(),0.0);
		double rMean=rSum/n;
		float rMeanF=safe((float)rMean);
		double var=0; for(auto r:rad){ double d=r-rMean; var+=d*d; }
		var/=n;
		double rStd=sqrt(var);
		float rStdNormF=safe((rMean>0)?(rStd/rMean):0);
		// protrusions
		double th=rMean*protThresh.get();
		int countProt=0; for(auto r:rad) if(r>=th) countProt++;
		float nProtF=(float)countProt;

		// derivades
		float s=derivScale.get();
		if(firstFrame){
			dCentX=dCentY=dArea=dPerimeter=dCircularity=dRoughness=dRadStdNorm=dNumProt=0;
			firstFrame=false;
		}else{
			dCentX=(cxF-pCentX)*s; dCentY=(cyF-pCentY)*s;
			dArea=(AF-pArea)*s; dPerimeter=(PF-pPerimeter)*s;
			dCircularity=(circF-pCircularity)*s; dRoughness=(roughF-pRoughness)*s;
			dRadStdNorm=(rStdNormF-pRadStdNorm)*s; dNumProt=(nProtF-pNumProt)*s;
		}

		// store prev
		pCentX=cxF; pCentY=cyF;
		pArea=AF; pPerimeter=PF; pCircularity=circF; pRoughness=roughF;
		pRadStdNorm=rStdNormF; pNumProt=nProtF;

		// derived
		float centSpd = sqrt(dCentX.get()*dCentX.get() + dCentY.get()*dCentY.get());
		float blobF = std::max(0.f, 1.f - circF);
		float deform = absf(dCircularity.get()) + absf(dRoughness.get());
		float szCh = absf(dArea.get());
		float act = absf(dArea.get())
				  + absf(dCircularity.get())
				  + absf(dRoughness.get())
				  + absf(dRadStdNorm.get())
				  + centSpd * 0.5f;

		// assign outputs
		centX=cxF; centY=cyF;
		width=wF; height=hF; aspect=aspectF;
		area=AF; perimeter=PF; req=rEqF;
		circularity=circF; roughness=roughF;
		radMean=rMeanF; radStdNorm=rStdNormF; numProt=nProtF;
		valid=1.0f;

		centroidSpeed=centSpd;
		blobiness=blobF;
		deformationSpeed=deform;
		sizeChange=szCh;
		activity=act;
	}

	void setZeros(){
		centX=centY=width=height=aspect=area=perimeter=req=circularity=roughness=radMean=radStdNorm=numProt=0;
		dCentX=dCentY=dArea=dPerimeter=dCircularity=dRoughness=dRadStdNorm=dNumProt=0;
		centroidSpeed=blobiness=deformationSpeed=sizeChange=activity=0;
		valid=0;
	}
};
