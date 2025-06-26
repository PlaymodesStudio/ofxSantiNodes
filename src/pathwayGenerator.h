#ifndef pathwayGenerator_h
#define pathwayGenerator_h

#include "ofxOceanodeNodeModel.h"
#include <random>

class pathwayGenerator : public ofxOceanodeNodeModel {
public:
	pathwayGenerator() : ofxOceanodeNodeModel("Pathway Generator") {}

	void setup() override {
		description = "Generates pathways over a matrix of segments by defining the angle of each segment. Angles are normalized 0-1 for 0-360°, -1 represents null values.";
		
		addParameter(width.set("Width", 4, 1, 32));
		addParameter(height.set("Height", 4, 1, 32));
		addParameter(numPaths.set("Num Paths", 1, 1, 8));
		addParameter(seed.set("Seed", 0, 0, 9999));
		addParameter(parallel.set("Parallel", false));
		addParameter(overlap.set("Overlap", true));
		addParameter(diagonalA.set("Diagonal A", true));
		addParameter(diagonalB.set("Diagonal B", true));
		addParameter(horizontal.set("Horizontal", true));
		addParameter(vertical.set("Vertical", true));
		
		addOutputParameter(output.set("Output", {0}, {-1}, {1}));
		addOutputParameter(diagA_mask.set("DiagA Mask", {0}, {0}, {1}));
		addOutputParameter(diagB_mask.set("DiagB Mask", {0}, {0}, {1}));
		addOutputParameter(hor_mask.set("Hor Mask", {0}, {0}, {1}));
		addOutputParameter(vert_mask.set("Vert Mask", {0}, {0}, {1}));
		
		// Generate initial pathways
		generatePathways();
		
		// Add listeners for all parameters that should retrigger calculation
		listeners.push(width.newListener([this](int &){ generatePathways(); }));
		listeners.push(height.newListener([this](int &){ generatePathways(); }));
		listeners.push(numPaths.newListener([this](int &){ generatePathways(); }));
		listeners.push(seed.newListener([this](int &){ generatePathways(); }));
		listeners.push(parallel.newListener([this](bool &){ generatePathways(); }));
		listeners.push(overlap.newListener([this](bool &){ generatePathways(); }));
		listeners.push(diagonalA.newListener([this](bool &){ generatePathways(); }));
		listeners.push(diagonalB.newListener([this](bool &){ generatePathways(); }));
		listeners.push(horizontal.newListener([this](bool &){ generatePathways(); }));
		listeners.push(vertical.newListener([this](bool &){ generatePathways(); }));
	}

private:
	ofParameter<int> width, height, numPaths, seed;
	ofParameter<bool> parallel, overlap, diagonalA, diagonalB, horizontal, vertical;
	ofParameter<vector<float>> output, diagA_mask, diagB_mask, hor_mask, vert_mask;
	ofEventListeners listeners;
	
	enum PathType {
		DIAGONAL_A,  // bottom-left to top-right
		DIAGONAL_B,  // top-left to bottom-right
		HORIZONTAL,  // left-to-right or right-to-left
		VERTICAL     // top-to-bottom or bottom-to-top
	};
	
	struct PathSegment {
		int x, y;
		float angle;
	};
	
	struct PathInfo {
		PathType type;
		vector<PathSegment> segments;
	};
	
	void generatePathways() {
		int w = width.get();
		int h = height.get();
		int paths = numPaths.get();
		
		// Initialize matrix and masks with zeros/null values
		vector<float> matrix(w * h, -1.0f);
		vector<float> maskDiagA(w * h, 0.0f);
		vector<float> maskDiagB(w * h, 0.0f);
		vector<float> maskHor(w * h, 0.0f);
		vector<float> maskVert(w * h, 0.0f);
		
		// Set up random generator with seed
		std::mt19937 rng(seed.get());
		
		// Get available path types
		vector<PathType> availableTypes;
		if (diagonalA.get()) availableTypes.push_back(DIAGONAL_A);
		if (diagonalB.get()) availableTypes.push_back(DIAGONAL_B);
		if (horizontal.get()) availableTypes.push_back(HORIZONTAL);
		if (vertical.get()) availableTypes.push_back(VERTICAL);
		
		if (availableTypes.empty()) {
			output = matrix;
			diagA_mask = maskDiagA;
			diagB_mask = maskDiagB;
			hor_mask = maskHor;
			vert_mask = maskVert;
			return;
		}
		
		// Store all generated paths for mask creation
		vector<PathInfo> allPaths;
		
		// Generate each path
		for (int pathIndex = 0; pathIndex < paths; pathIndex++) {
			// Randomly select path type
			PathType pathType = availableTypes[rng() % availableTypes.size()];
			
			// Generate path segments
			vector<PathSegment> pathSegments = generatePath(pathType, w, h, rng, allPaths);
			
			// Store path info for mask generation
			PathInfo pathInfo;
			pathInfo.type = pathType;
			pathInfo.segments = pathSegments;
			allPaths.push_back(pathInfo);
			
			// Apply segments to matrix
			applyPathToMatrix(matrix, pathSegments, w, h, rng);
		}
		
		// Generate masks for each path type based on final output
		generateMasks(allPaths, maskDiagA, maskDiagB, maskHor, maskVert, w, h, matrix);
		
		// Set outputs
		output = matrix;
		diagA_mask = maskDiagA;
		diagB_mask = maskDiagB;
		hor_mask = maskHor;
		vert_mask = maskVert;
	}
	
	vector<PathSegment> generatePath(PathType type, int w, int h, std::mt19937& rng, const vector<PathInfo>& existingPaths) {
		vector<PathSegment> segments;
		std::uniform_real_distribution<float> angleDist(0.0f, 1.0f);
		
		switch (type) {
			case DIAGONAL_A: {
				// Bottom-left to top-right diagonal
				vector<pair<int, int>> startPositions;
				// Bottom row
				for (int x = 0; x < w; x++) startPositions.push_back({x, 0});
				// Left column (excluding bottom-left corner already added)
				for (int y = 1; y < h; y++) startPositions.push_back({0, y});
				
				auto start = getValidDiagonalAPosition(startPositions, w, h, rng, existingPaths);
				int x = start.first, y = start.second;
				
				float baseAngle = 0.125f; // 45° normalized (bottom-left to top-right movement)
				
				while (x < w && y < h) {
					PathSegment segment;
					segment.x = x;
					segment.y = y;
					segment.angle = baseAngle;
					segments.push_back(segment);
					x++;
					y++;
				}
				break;
			}
			
			case DIAGONAL_B: {
				// Top-left to bottom-right diagonal
				vector<pair<int, int>> startPositions;
				// Top row
				for (int x = 0; x < w; x++) startPositions.push_back({x, h-1});
				// Left column (excluding top-left corner already added)
				for (int y = 0; y < h-1; y++) startPositions.push_back({0, y});
				
				auto start = getValidDiagonalBPosition(startPositions, w, h, rng, existingPaths);
				int x = start.first, y = start.second;
				
				float baseAngle = 0.375f; // 135° normalized (315° + 180° = 135°, shifted 180 degrees)
				
				while (x < w && y >= 0) {
					PathSegment segment;
					segment.x = x;
					segment.y = y;
					segment.angle = baseAngle;
					segments.push_back(segment);
					x++;
					y--;
				}
				break;
			}
			
			case HORIZONTAL: {
				// Horizontal path
				int y = getValidHorizontalPosition(h, rng, existingPaths, w);
				if (y == -1) y = rng() % h; // Fallback if no valid position found
				
				float baseAngle = 0.5f; // 180° normalized (horizontal)
				
				for (int x = 0; x < w; x++) {
					PathSegment segment;
					segment.x = x;
					segment.y = y;
					segment.angle = baseAngle;
					segments.push_back(segment);
				}
				break;
			}
			
			case VERTICAL: {
				// Vertical path
				int x = getValidVerticalPosition(w, rng, existingPaths, h);
				if (x == -1) x = rng() % w; // Fallback if no valid position found
				
				float baseAngle = 0.25f; // 90° normalized (vertical)
				
				for (int y = 0; y < h; y++) {
					PathSegment segment;
					segment.x = x;
					segment.y = y;
					segment.angle = baseAngle;
					segments.push_back(segment);
				}
				break;
			}
		}
		
		return segments;
	}
	
	void applyPathToMatrix(vector<float>& matrix, const vector<PathSegment>& segments, int w, int h, std::mt19937& rng) {
		if (parallel.get()) {
			applyParallelPathToMatrix(matrix, segments, w, h);
		} else {
			// Normal mode - just place the segments
			for (const PathSegment& segment : segments) {
				int index = segment.y * w + segment.x;
				
				if (index >= 0 && index < matrix.size()) {
					// Check for overlap
					if (!overlap.get() && matrix[index] != -1.0f) {
						continue; // Skip this segment if overlap is disabled and position is occupied
					}
					
					matrix[index] = segment.angle;
				}
			}
		}
	}
	
	int getValidHorizontalPosition(int h, std::mt19937& rng, const vector<PathInfo>& existingPaths, int w) {
		if (!parallel.get()) return rng() % h; // No separation needed in non-parallel mode
		
		vector<int> usedRows;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == HORIZONTAL) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedRows.push_back(segment.y);
					usedRows.push_back(segment.y + 1); // Adjacent row used in parallel mode
					usedRows.push_back(segment.y - 1); // Adjacent row used in parallel mode
					break; // Only need to check first segment for horizontal paths
				}
			}
		}
		
		// Find valid positions with at least 1 row separation from existing path pairs
		// and ensure the whole path (pair) fits inside the matrix
		vector<int> validPositions;
		for (int y = 0; y < h; y++) {
			bool valid = true;
			for (int usedRow : usedRows) {
				if (abs(y - usedRow) <= 1) { // Too close to existing path pair
					valid = false;
					break;
				}
			}
			// Check if both the main path and parallel path fit inside matrix
			if (valid && y + 1 < h) { // Ensure both y and y+1 are inside matrix
				validPositions.push_back(y);
			} else if (valid && y - 1 >= 0) { // Or both y-1 and y are inside matrix
				validPositions.push_back(y - 1); // Shift the whole pair to fit
			}
		}
		
		if (validPositions.empty()) return -1;
		return validPositions[rng() % validPositions.size()];
	}
	
	int getValidVerticalPosition(int w, std::mt19937& rng, const vector<PathInfo>& existingPaths, int h) {
		if (!parallel.get()) return rng() % w; // No separation needed in non-parallel mode
		
		vector<int> usedCols;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == VERTICAL) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedCols.push_back(segment.x);
					usedCols.push_back(segment.x + 1); // Adjacent column used in parallel mode
					usedCols.push_back(segment.x - 1); // Adjacent column used in parallel mode
					break; // Only need to check first segment for vertical paths
				}
			}
		}
		
		// Find valid positions with at least 1 column separation from existing path pairs
		// and ensure the whole path (pair) fits inside the matrix
		vector<int> validPositions;
		for (int x = 0; x < w; x++) {
			bool valid = true;
			for (int usedCol : usedCols) {
				if (abs(x - usedCol) <= 1) { // Too close to existing path pair
					valid = false;
					break;
				}
			}
			// Check if both the main path and parallel path fit inside matrix
			if (valid && x + 1 < w) { // Ensure both x and x+1 are inside matrix
				validPositions.push_back(x);
			} else if (valid && x - 1 >= 0) { // Or both x-1 and x are inside matrix
				validPositions.push_back(x - 1); // Shift the whole pair to fit
			}
		}
		
		if (validPositions.empty()) return -1;
		return validPositions[rng() % validPositions.size()];
	}
	
	pair<int, int> getValidDiagonalAPosition(const vector<pair<int, int>>& startPositions, int w, int h,
										   std::mt19937& rng, const vector<PathInfo>& existingPaths) {
		if (!parallel.get()) return startPositions[rng() % startPositions.size()];
		
		vector<pair<int, int>> usedPositions;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == DIAGONAL_A) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedPositions.push_back({segment.x, segment.y});
					usedPositions.push_back({segment.x + 1, segment.y}); // Adjacent diagonal used in parallel mode
					usedPositions.push_back({segment.x - 1, segment.y}); // Adjacent diagonal used in parallel mode
				}
			}
		}
		
		vector<pair<int, int>> validPositions;
		for (const auto& pos : startPositions) {
			bool valid = true;
			for (const auto& usedPos : usedPositions) {
				if (abs(pos.first - usedPos.first) <= 1 && pos.second == usedPos.second) {
					valid = false;
					break;
				}
			}
			
			// Check if the whole diagonal pair fits inside matrix
			if (valid) {
				bool canFitRight = true, canFitLeft = true;
				int x = pos.first, y = pos.second;
				// Check if main diagonal + right parallel fits
				while (x < w && y < h) {
					if (x + 1 >= w) { canFitRight = false; break; }
					x++; y++;
				}
				// Check if main diagonal + left parallel fits
				x = pos.first; y = pos.second;
				while (x < w && y < h) {
					if (x - 1 < 0) { canFitLeft = false; break; }
					x++; y++;
				}
				
				if (canFitRight || canFitLeft) {
					validPositions.push_back(pos);
				}
			}
		}
		
		if (validPositions.empty()) return startPositions[rng() % startPositions.size()];
		return validPositions[rng() % validPositions.size()];
	}
	
	pair<int, int> getValidDiagonalBPosition(const vector<pair<int, int>>& startPositions, int w, int h,
										   std::mt19937& rng, const vector<PathInfo>& existingPaths) {
		if (!parallel.get()) return startPositions[rng() % startPositions.size()];
		
		vector<pair<int, int>> usedPositions;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == DIAGONAL_B) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedPositions.push_back({segment.x, segment.y});
					usedPositions.push_back({segment.x + 1, segment.y}); // Adjacent diagonal used in parallel mode
					usedPositions.push_back({segment.x - 1, segment.y}); // Adjacent diagonal used in parallel mode
				}
			}
		}
		
		vector<pair<int, int>> validPositions;
		for (const auto& pos : startPositions) {
			bool valid = true;
			for (const auto& usedPos : usedPositions) {
				if (abs(pos.first - usedPos.first) <= 1 && pos.second == usedPos.second) {
					valid = false;
					break;
				}
			}
			
			// Check if the whole diagonal pair fits inside matrix
			if (valid) {
				bool canFitRight = true, canFitLeft = true;
				int x = pos.first, y = pos.second;
				// Check if main diagonal + right parallel fits
				while (x < w && y >= 0) {
					if (x + 1 >= w) { canFitRight = false; break; }
					x++; y--;
				}
				// Check if main diagonal + left parallel fits
				x = pos.first; y = pos.second;
				while (x < w && y >= 0) {
					if (x - 1 < 0) { canFitLeft = false; break; }
					x++; y--;
				}
				
				if (canFitRight || canFitLeft) {
					validPositions.push_back(pos);
				}
			}
		}
		
		if (validPositions.empty()) return startPositions[rng() % startPositions.size()];
		return validPositions[rng() % validPositions.size()];
	}
	
	void applyParallelPathToMatrix(vector<float>& matrix, const vector<PathSegment>& segments, int w, int h) {
		// In parallel mode, we create alternating parallel segments
		// For each segment in the path, we place both the original angle and inverted angle
		
		for (const PathSegment& segment : segments) {
			int index = segment.y * w + segment.x;
			
			if (index >= 0 && index < matrix.size()) {
				// Check for overlap
				if (!overlap.get() && matrix[index] != -1.0f) {
					continue;
				}
				
				// Place original angle
				matrix[index] = segment.angle;
				
				// Place inverted angle in adjacent position
				int adjacentIndex = getAdjacentIndex(segment, w, h);
				if (adjacentIndex >= 0 && adjacentIndex < matrix.size()) {
					if (overlap.get() || matrix[adjacentIndex] == -1.0f) {
						float invertedAngle = fmod(segment.angle + 0.5f, 1.0f);
						matrix[adjacentIndex] = invertedAngle;
					}
				}
			}
		}
	}
	
	int getAdjacentIndex(const PathSegment& segment, int w, int h) {
		// Find adjacent position based on the path direction
		// This creates the parallel pattern with adjacent segments
		
		float angle = segment.angle;
		
		// Determine which direction to place the parallel segment
		if (abs(angle - 0.125f) < 0.01f) {
			// Diagonal A path - place parallel segment offset
			if (segment.x + 1 < w) {
				return segment.y * w + (segment.x + 1);
			}
		} else if (abs(angle - 0.375f) < 0.01f) {
			// Diagonal B path - place parallel segment offset
			if (segment.x + 1 < w) {
				return segment.y * w + (segment.x + 1);
			}
		} else if (abs(angle - 0.5f) < 0.01f) {
			// Horizontal path - place parallel segment in adjacent row
			if (segment.y + 1 < h) {
				return (segment.y + 1) * w + segment.x;
			} else if (segment.y - 1 >= 0) {
				return (segment.y - 1) * w + segment.x;
			}
		} else if (abs(angle - 0.25f) < 0.01f) {
			// Vertical path - place parallel segment in adjacent column
			if (segment.x + 1 < w) {
				return segment.y * w + (segment.x + 1);
			} else if (segment.x - 1 >= 0) {
				return segment.y * w + (segment.x - 1);
			}
		}
		
		return -1; // No valid adjacent position
	}
	
	void generateMasks(const vector<PathInfo>& allPaths, vector<float>& maskDiagA, vector<float>& maskDiagB,
					  vector<float>& maskHor, vector<float>& maskVert, int w, int h, const vector<float>& finalMatrix) {
		
		// For each position in the matrix, determine which path type "won" (is present in final output)
		for (int i = 0; i < finalMatrix.size(); i++) {
			if (finalMatrix[i] == -1.0f) continue; // Skip empty positions
			
			float finalAngle = finalMatrix[i];
			
			// Determine which path type this angle belongs to
			if (abs(finalAngle - 0.125f) < 0.01f) {
				// Diagonal A angle
				maskDiagA[i] = 1.0f;
			} else if (abs(finalAngle - 0.375f) < 0.01f) {
				// Diagonal B angle
				maskDiagB[i] = 1.0f;
			} else if (abs(finalAngle - 0.5f) < 0.01f) {
				// Horizontal angle
				maskHor[i] = 1.0f;
			} else if (abs(finalAngle - 0.25f) < 0.01f) {
				// Vertical angle
				maskVert[i] = 1.0f;
			} else {
				// Handle parallel mode inverted angles
				float invertedAngle = fmod(finalAngle + 0.5f, 1.0f);
				
				if (abs(invertedAngle - 0.125f) < 0.01f) {
					// Inverted Diagonal A angle
					maskDiagA[i] = 1.0f;
				} else if (abs(invertedAngle - 0.375f) < 0.01f) {
					// Inverted Diagonal B angle
					maskDiagB[i] = 1.0f;
				} else if (abs(invertedAngle - 0.5f) < 0.01f) {
					// Inverted Horizontal angle
					maskHor[i] = 1.0f;
				} else if (abs(invertedAngle - 0.25f) < 0.01f) {
					// Inverted Vertical angle
					maskVert[i] = 1.0f;
				}
			}
		}
	}
};

#endif /* pathwayGenerator_h */
