#ifndef trimPathSequential_h
#define trimPathSequential_h

#include "ofxOceanodeNodeModel.h"

class trimPathSequential : public ofxOceanodeNodeModel
{
public:
	trimPathSequential() : ofxOceanodeNodeModel("Trim Path Sequential") {}
	
	void setup() override
	{
		description = "Sequential version of Trim Path. When Sequential is false, behaves like regular Trim Path. When Sequential is true, trimming progresses through segments sequentially from first to last, creating animated path reveal effects. Start and End parameters control the overall progress through all segments.";
		
		addParameter(pointsX.set("In.X", {0.5}, {0}, {1}));
		addParameter(pointsY.set("In.Y", {0.5}, {0}, {1}));
		addParameter(start.set("Start", {0.0}, {0.0}, {1.0}));
		addParameter(end.set("End", {1.0}, {0.0}, {1.0}));
		addParameter(keepOrder.set("Keep Order", false));
		addParameter(sequential.set("Sequential", false));
		addOutputParameter(outX.set("Out.X", {0}, {0}, {1}));
		addOutputParameter(outY.set("Out.Y", {0}, {0}, {1}));
		addOutputParameter(completeness.set("Completeness", {0}, {0}, {1}));
		addOutputParameter(fullSegmentOutX.set("FullSegment.X", {0}, {0}, {1}));
		addOutputParameter(fullSegmentOutY.set("FullSegment.Y", {0}, {0}, {1}));
		
		listener = pointsX.newListener([this](vector<float> &vf) {
			calculate();
		});
	}
	
	void calculate()
	{
		vector<vector<glm::vec2>> allPaths;
		vector<float> segmentCompleteness;
		vector<float> auxX, auxY;
		vector<glm::vec2> currentPath;
		size_t totalSegments = 0;
		
		bool inputSameSize = (pointsX.get().size() == pointsY.get().size());
		
		// Separate input vectors into multiple paths using -1 as a path separator
		for (size_t i = 0; i < pointsX.get().size() && i < pointsY.get().size(); i++)
		{
			if (pointsX.get()[i] == -1 || pointsY.get()[i] == -1)
			{
				if (!currentPath.empty())
				{
					totalSegments += currentPath.size() - 1;
					allPaths.push_back(currentPath);
					currentPath.clear();
				}
			}
			else
			{
				currentPath.push_back(glm::vec2(pointsX.get()[i], pointsY.get()[i]));
			}
		}
		
		if (!currentPath.empty())
		{
			totalSegments += currentPath.size() - 1;
			allPaths.push_back(currentPath);
		}
		
		segmentCompleteness.resize(totalSegments, 0.0f);
		
		// Initialize full segment output vectors
		vector<float> fullSegX(totalSegments * 3, 0);
		vector<float> fullSegY(totalSegments * 3, 0);
		
		if (!sequential)
		{
			// Regular trimming mode - same as original trimPath
			calculateRegularTrimming(allPaths, auxX, auxY, fullSegX, fullSegY, segmentCompleteness, totalSegments);
		}
		else
		{
			// Sequential trimming mode
			calculateSequentialTrimming(allPaths, auxX, auxY, fullSegX, fullSegY, segmentCompleteness, totalSegments);
		}
		
		outX = auxX;
		outY = auxY;
		completeness = segmentCompleteness;
		fullSegmentOutX = fullSegX;
		fullSegmentOutY = fullSegY;
	}
	
private:
	void calculateRegularTrimming(const vector<vector<glm::vec2>>& allPaths, vector<float>& auxX, vector<float>& auxY,
								 vector<float>& fullSegX, vector<float>& fullSegY, vector<float>& segmentCompleteness,
								 size_t totalSegments)
	{
		size_t globalSegmentIndex = 0;
		
		// Regular trimming logic - same as original trimPath
		if (start.get().size() == totalSegments && end.get().size() == totalSegments)
		{
			size_t segmentIndex = 0;
			for (size_t pathIndex = 0; pathIndex < allPaths.size(); pathIndex++)
			{
				const auto &path = allPaths[pathIndex];
				for (size_t i = 0; i < path.size() - 1; ++i)
				{
					processSegmentRegular(path[i], path[i + 1], segmentIndex, globalSegmentIndex,
										auxX, auxY, fullSegX, fullSegY, segmentCompleteness,
										start.get()[segmentIndex], end.get()[segmentIndex]);
					++segmentIndex;
					++globalSegmentIndex;
				}
			}
		}
		else if (start.get().size() == 1 && end.get().size() == 1)
		{
			float globalStart = start.get()[0];
			float globalEnd = end.get()[0];
			size_t segmentIndex = 0;
			
			for (size_t pathIndex = 0; pathIndex < allPaths.size(); pathIndex++)
			{
				const auto &path = allPaths[pathIndex];
				for (size_t i = 0; i < path.size() - 1; ++i)
				{
					processSegmentRegular(path[i], path[i + 1], segmentIndex, globalSegmentIndex,
										auxX, auxY, fullSegX, fullSegY, segmentCompleteness,
										globalStart, globalEnd);
					++segmentIndex;
					++globalSegmentIndex;
				}
			}
		}
		else if (start.get().size() == allPaths.size() && end.get().size() == allPaths.size())
		{
			size_t segmentIndex = 0;
			for (size_t pathIndex = 0; pathIndex < allPaths.size(); pathIndex++)
			{
				const auto &path = allPaths[pathIndex];
				float pathStart = start.get()[pathIndex];
				float pathEnd = end.get()[pathIndex];
				
				for (size_t i = 0; i < path.size() - 1; ++i)
				{
					processSegmentRegular(path[i], path[i + 1], segmentIndex, globalSegmentIndex,
										auxX, auxY, fullSegX, fullSegY, segmentCompleteness,
										pathStart, pathEnd);
					++segmentIndex;
					++globalSegmentIndex;
				}
			}
		}
	}
	
	void calculateSequentialTrimming(const vector<vector<glm::vec2>>& allPaths, vector<float>& auxX, vector<float>& auxY,
								   vector<float>& fullSegX, vector<float>& fullSegY, vector<float>& segmentCompleteness,
								   size_t totalSegments)
	{
		// In sequential mode, we use only the first values of start and end parameters
		float globalStart = start.get().size() > 0 ? start.get()[0] : 0.0f;
		float globalEnd = end.get().size() > 0 ? end.get()[0] : 1.0f;
		
		// Ensure proper order unless keepOrder is true
		if (!keepOrder && globalStart > globalEnd) {
			std::swap(globalStart, globalEnd);
		}
		
		// Handle keepOrder logic
		if (keepOrder && globalStart > globalEnd) {
			// When keeping order and start > end, output nothing
			return;
		}
		
		size_t globalSegmentIndex = 0;
		size_t segmentIndex = 0;
		
		// Sequential trimming: map global progress to individual segments
		for (size_t pathIndex = 0; pathIndex < allPaths.size(); pathIndex++)
		{
			const auto &path = allPaths[pathIndex];
			for (size_t i = 0; i < path.size() - 1; ++i)
			{
				// Calculate this segment's position in the overall sequence
				float segmentStartProgress = (float)globalSegmentIndex / (float)totalSegments;
				float segmentEndProgress = (float)(globalSegmentIndex + 1) / (float)totalSegments;
				
				// Calculate how much of this segment should be visible
				float segmentStart = 0.0f;
				float segmentEnd = 0.0f;
				
				if (globalEnd <= segmentStartProgress) {
					// Global end is before this segment starts - segment not visible
					segmentStart = 0.0f;
					segmentEnd = 0.0f;
				} else if (globalStart > segmentEndProgress) {
					// Global start is after this segment ends - segment not visible
					segmentStart = 0.0f;
					segmentEnd = 0.0f;
				} else if (globalStart == globalEnd) {
					// Special case: when start equals end, nothing should be visible
					segmentStart = 0.0f;
					segmentEnd = 0.0f;
				} else {
					// Segment is at least partially visible
					if (globalStart <= segmentStartProgress && globalEnd >= segmentEndProgress) {
						// Segment is fully visible
						segmentStart = 0.0f;
						segmentEnd = 1.0f;
					} else {
						// Segment is partially visible
						float segmentLength = segmentEndProgress - segmentStartProgress;
						
						if (globalStart > segmentStartProgress) {
							segmentStart = (globalStart - segmentStartProgress) / segmentLength;
						} else {
							segmentStart = 0.0f;
						}
						
						if (globalEnd < segmentEndProgress) {
							segmentEnd = (globalEnd - segmentStartProgress) / segmentLength;
						} else {
							segmentEnd = 1.0f;
						}
					}
				}
				
				processSegmentRegular(path[i], path[i + 1], segmentIndex, globalSegmentIndex,
									auxX, auxY, fullSegX, fullSegY, segmentCompleteness,
									segmentStart, segmentEnd);
				
				++segmentIndex;
				++globalSegmentIndex;
			}
		}
	}
	
	void processSegmentRegular(const glm::vec2& point1, const glm::vec2& point2, size_t segmentIndex, size_t globalSegmentIndex,
							  vector<float>& auxX, vector<float>& auxY, vector<float>& fullSegX, vector<float>& fullSegY,
							  vector<float>& segmentCompleteness, float segmentStart, float segmentEnd)
	{
		size_t outputPos = segmentIndex * 3;
		
		if (segmentStart == segmentEnd || (keepOrder && segmentStart > segmentEnd))
		{
			segmentCompleteness[globalSegmentIndex] = 0.0f;
			
			// Write zeros at the exact segment position
			fullSegX[outputPos] = 0;
			fullSegY[outputPos] = 0;
			fullSegX[outputPos + 1] = 0;
			fullSegY[outputPos + 1] = 0;
			fullSegX[outputPos + 2] = -1;
			fullSegY[outputPos + 2] = -1;
		}
		else
		{
			float startT = keepOrder ?
				glm::max(segmentStart, 0.0f) :
				glm::max(std::min(segmentStart, segmentEnd), 0.0f);
			
			float endT = keepOrder ?
				glm::min(segmentEnd, 1.0f) :
				glm::min(std::max(segmentStart, segmentEnd), 1.0f);
			
			glm::vec2 startPoint = point1 + startT * (point2 - point1);
			glm::vec2 endPoint = point1 + endT * (point2 - point1);
			
			// Regular output
			auxX.push_back(startPoint.x);
			auxY.push_back(startPoint.y);
			auxX.push_back(endPoint.x);
			auxY.push_back(endPoint.y);
			auxX.push_back(-1);
			auxY.push_back(-1);
			
			// Write to exact positions in full segment output
			fullSegX[outputPos] = startPoint.x;
			fullSegY[outputPos] = startPoint.y;
			fullSegX[outputPos + 1] = endPoint.x;
			fullSegY[outputPos + 1] = endPoint.y;
			fullSegX[outputPos + 2] = -1;
			fullSegY[outputPos + 2] = -1;
			
			segmentCompleteness[globalSegmentIndex] = glm::abs(endT - startT);
		}
	}
	
	ofParameter<vector<float>> pointsX, pointsY;
	ofParameter<vector<float>> start, end;
	ofParameter<vector<float>> outX, outY, completeness;
	ofParameter<vector<float>> fullSegmentOutX, fullSegmentOutY;
	ofParameter<bool> keepOrder, sequential;
	ofEventListener listener;
};

#endif /* trimPathSequential_h */
