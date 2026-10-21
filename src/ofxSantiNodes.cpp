#include "ofxSantiNodes.h"
#include "ofxOceanode.h"
#include "ofxOceanodeTransport.h"

// The nodes are registered in ofxSantiNodesSequencing/Math/Pitch/Utility.cpp,
// so the node headers compile in parallel and an edited node header rebuilds
// only its part.

namespace ofxOceanodeSanti{

void registerSequencingNodes(ofxOceanode &o);
void registerMathNodes(ofxOceanode &o);
void registerPitchNodes(ofxOceanode &o);
void registerUtilityNodes(ofxOceanode &o);

void registerModels(ofxOceanode &o)
{
    registerSequencingNodes(o);   // ofxSantiNodesSequencing.cpp
    registerMathNodes(o);   // ofxSantiNodesMath.cpp
    registerPitchNodes(o);   // ofxSantiNodesPitch.cpp
    registerUtilityNodes(o);   // ofxSantiNodesUtility.cpp
}

void registerCollection(ofxOceanode &o)
{
    registerModels(o);
}

}
