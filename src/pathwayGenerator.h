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
		addParameter(minLength.set("Min Length", 1, 1, 32));
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
		listeners.push(minLength.newListener([this](int &){ generatePathways(); }));
		listeners.push(parallel.newListener([this](bool &){ generatePathways(); }));
		listeners.push(overlap.newListener([this](bool &){ generatePathways(); }));
		listeners.push(diagonalA.newListener([this](bool &){ generatePathways(); }));
		listeners.push(diagonalB.newListener([this](bool &){ generatePathways(); }));
		listeners.push(horizontal.newListener([this](bool &){ generatePathways(); }));
		listeners.push(vertical.newListener([this](bool &){ generatePathways(); }));
	}

private:
	ofParameter<int> width, height, numPaths, seed, minLength;
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
		PathType type;
	};
	
	struct PathInfo {
		PathType type;
		vector<PathSegment> segments;
		int offset; // For parallel paths: 0 = main path, 1 = parallel path
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
			
			// Generate path segments (both main and parallel if enabled)
			vector<PathInfo> pathPair = generatePath(pathType, w, h, rng, allPaths);
			
			// Add generated paths to our collection
			for (const auto& path : pathPair) {
				allPaths.push_back(path);
			}
		}
		
		// Apply all paths to matrix and validate minimum lengths
		vector<PathInfo> validPaths = applyPathsAndValidate(allPaths, matrix, w, h);
		
		// Generate masks for each path type based on final output
		generateMasks(validPaths, maskDiagA, maskDiagB, maskHor, maskVert, w, h, matrix);
		
		// Set outputs
		output = matrix;
		diagA_mask = maskDiagA;
		diagB_mask = maskDiagB;
		hor_mask = maskHor;
		vert_mask = maskVert;
	}
	
	vector<PathInfo> generatePath(PathType type, int w, int h, std::mt19937& rng, const vector<PathInfo>& existingPaths) {
		vector<PathInfo> result;
		std::uniform_real_distribution<float> angleDist(0.0f, 1.0f);
		
		switch (type) {
			case DIAGONAL_A: {
				// Bottom-left to top-right diagonal
				vector<pair<int, int>> startPositions;
				// Bottom row
				for (int x = 0; x < w; x++) startPositions.push_back({x, 0});
				// Left column (excluding bottom-left corner already added)
				for (int y = 1; y < h; y++) startPositions.push_back({0, y});
				
				auto validOffsets = getValidDiagonalAOffsets(startPositions, w, h, rng, existingPaths);
				
				for (const auto& offsetInfo : validOffsets) {
					PathInfo pathInfo;
					pathInfo.type = DIAGONAL_A;
					pathInfo.offset = offsetInfo.second;
					
					int x = offsetInfo.first.first;
					int y = offsetInfo.first.second;
					
					// Check if this diagonal meets minimum length requirement
					int projectedLength = calculateDiagonalLength(x, y, w, h, DIAGONAL_A);
					if (projectedLength < minLength.get()) {
						continue; // Skip this diagonal if it's too short
					}
					
					float baseAngle = 0.125f; // 45° normalized
					if (pathInfo.offset == 1) {
						baseAngle = fmod(baseAngle + 0.5f, 1.0f); // Inverted for parallel
					}
					
					// Generate the full diagonal path to its maximum extent
					while (x < w && y < h) {
						PathSegment segment;
						segment.x = x;
						segment.y = y;
						segment.angle = baseAngle;
						segment.type = DIAGONAL_A;
						pathInfo.segments.push_back(segment);
						x++;
						y++;
					}
					
					if (!pathInfo.segments.empty()) {
						result.push_back(pathInfo);
					}
					
					if (!parallel.get()) break; // Only generate main path if not parallel
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
				
				auto validOffsets = getValidDiagonalBOffsets(startPositions, w, h, rng, existingPaths);
				
				for (const auto& offsetInfo : validOffsets) {
					PathInfo pathInfo;
					pathInfo.type = DIAGONAL_B;
					pathInfo.offset = offsetInfo.second;
					
					int x = offsetInfo.first.first;
					int y = offsetInfo.first.second;
					
					// Check if this diagonal meets minimum length requirement
					int projectedLength = calculateDiagonalLength(x, y, w, h, DIAGONAL_B);
					if (projectedLength < minLength.get()) {
						continue; // Skip this diagonal if it's too short
					}
					
					float baseAngle = 0.375f; // 135° normalized
					if (pathInfo.offset == 1) {
						baseAngle = fmod(baseAngle + 0.5f, 1.0f); // Inverted for parallel
					}
					
					// Generate the full diagonal path to its maximum extent
					while (x < w && y >= 0) {
						PathSegment segment;
						segment.x = x;
						segment.y = y;
						segment.angle = baseAngle;
						segment.type = DIAGONAL_B;
						pathInfo.segments.push_back(segment);
						x++;
						y--;
					}
					
					if (!pathInfo.segments.empty()) {
						result.push_back(pathInfo);
					}
					
					if (!parallel.get()) break; // Only generate main path if not parallel
				}
				break;
			}
			
			case HORIZONTAL: {
				// Horizontal path
				auto validOffsets = getValidHorizontalOffsets(h, rng, existingPaths, w);
				
				for (const auto& offsetInfo : validOffsets) {
					PathInfo pathInfo;
					pathInfo.type = HORIZONTAL;
					pathInfo.offset = offsetInfo.second;
					
					int y = offsetInfo.first;
					
					float baseAngle = 0.5f; // 180° normalized
					if (pathInfo.offset == 1) {
						baseAngle = fmod(baseAngle + 0.5f, 1.0f); // Inverted for parallel
					}
					
					for (int x = 0; x < w; x++) {
						PathSegment segment;
						segment.x = x;
						segment.y = y;
						segment.angle = baseAngle;
						segment.type = HORIZONTAL;
						pathInfo.segments.push_back(segment);
					}
					
					if (!pathInfo.segments.empty()) {
						result.push_back(pathInfo);
					}
					
					if (!parallel.get()) break; // Only generate main path if not parallel
				}
				break;
			}
			
			case VERTICAL: {
				// Vertical path
				auto validOffsets = getValidVerticalOffsets(w, rng, existingPaths, h);
				
				for (const auto& offsetInfo : validOffsets) {
					PathInfo pathInfo;
					pathInfo.type = VERTICAL;
					pathInfo.offset = offsetInfo.second;
					
					int x = offsetInfo.first;
					
					float baseAngle = 0.25f; // 90° normalized
					if (pathInfo.offset == 1) {
						baseAngle = fmod(baseAngle + 0.5f, 1.0f); // Inverted for parallel
					}
					
					for (int y = 0; y < h; y++) {
						PathSegment segment;
						segment.x = x;
						segment.y = y;
						segment.angle = baseAngle;
						segment.type = VERTICAL;
						pathInfo.segments.push_back(segment);
					}
					
					if (!pathInfo.segments.empty()) {
						result.push_back(pathInfo);
					}
					
					if (!parallel.get()) break; // Only generate main path if not parallel
				}
				break;
			}
		}
		
		return result;
	}
	
	int calculateDiagonalLength(int startX, int startY, int w, int h, PathType type) {
		switch (type) {
			case DIAGONAL_A:
				// Bottom-left to top-right: limited by remaining width or height
				return std::min(w - startX, h - startY);
			case DIAGONAL_B:
				// Top-left to bottom-right: limited by remaining width or remaining downward movement
				return std::min(w - startX, startY + 1);
			default:
				return 0; // Horizontal and vertical paths always span full matrix
		}
	}
	
	vector<PathInfo> applyPathsAndValidate(const vector<PathInfo>& allPaths, vector<float>& matrix, int w, int h) {
		vector<PathInfo> validPaths;
		
		// Apply all paths to matrix (overlap handled by "top wins" rule)
		for (const auto& path : allPaths) {
			applyPathToMatrix(matrix, path.segments, w, h);
		}
		
		// Now validate each path based on visible segments in final matrix
		for (const auto& path : allPaths) {
			int visibleSegments = countVisibleSegments(path, matrix, w, h);
			
			// For diagonal paths, check minimum length requirement
			if (path.type == DIAGONAL_A || path.type == DIAGONAL_B) {
				if (visibleSegments >= minLength.get()) {
					validPaths.push_back(path);
				}
			} else {
				// Horizontal and vertical paths are always valid (they span full matrix)
				validPaths.push_back(path);
			}
		}
		
		return validPaths;
	}
	
	int countVisibleSegments(const PathInfo& path, const vector<float>& matrix, int w, int h) {
		int count = 0;
		
		for (const auto& segment : path.segments) {
			int index = segment.y * w + segment.x;
			if (index >= 0 && index < matrix.size()) {
				// Check if this segment is visible in the final matrix
				if (abs(matrix[index] - segment.angle) < 0.01f) {
					count++;
				}
			}
		}
		
		return count;
	}
	
	void applyPathToMatrix(vector<float>& matrix, const vector<PathSegment>& segments, int w, int h) {
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
	
	vector<pair<int, int>> getValidHorizontalOffsets(int h, std::mt19937& rng, const vector<PathInfo>& existingPaths, int w) {
		vector<pair<int, int>> result;
		
		if (!parallel.get()) {
			// Non-parallel mode: just return a random row
			int y = rng() % h;
			result.push_back({y, 0});
			return result;
		}
		
		// Get all used rows by existing horizontal paths
		vector<int> usedRows;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == HORIZONTAL) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedRows.push_back(segment.y);
					break; // Only need first segment for horizontal paths
				}
			}
		}
		
		// Find valid positions for parallel pairs
		vector<int> validMainRows;
		for (int y = 0; y < h - 1; y++) { // Ensure we have space for parallel row
			bool valid = true;
			
			// Check if this row or the next row conflicts with existing paths
			for (int usedRow : usedRows) {
				if (abs(y - usedRow) <= 1 || abs((y + 1) - usedRow) <= 1) {
					valid = false;
					break;
				}
			}
			
			if (valid) {
				validMainRows.push_back(y);
			}
		}
		
		if (validMainRows.empty()) {
			// Fallback: use any available row
			int y = rng() % h;
			result.push_back({y, 0});
			return result;
		}
		
		// Select a random valid main row
		int selectedRow = validMainRows[rng() % validMainRows.size()];
		
		// Return both main and parallel rows
		result.push_back({selectedRow, 0});     // Main path
		result.push_back({selectedRow + 1, 1}); // Parallel path
		
		return result;
	}
	
	vector<pair<int, int>> getValidVerticalOffsets(int w, std::mt19937& rng, const vector<PathInfo>& existingPaths, int h) {
		vector<pair<int, int>> result;
		
		if (!parallel.get()) {
			// Non-parallel mode: just return a random column
			int x = rng() % w;
			result.push_back({x, 0});
			return result;
		}
		
		// Get all used columns by existing vertical paths
		vector<int> usedCols;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == VERTICAL) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedCols.push_back(segment.x);
					break; // Only need first segment for vertical paths
				}
			}
		}
		
		// Find valid positions for parallel pairs
		vector<int> validMainCols;
		for (int x = 0; x < w - 1; x++) { // Ensure we have space for parallel column
			bool valid = true;
			
			// Check if this column or the next column conflicts with existing paths
			for (int usedCol : usedCols) {
				if (abs(x - usedCol) <= 1 || abs((x + 1) - usedCol) <= 1) {
					valid = false;
					break;
				}
			}
			
			if (valid) {
				validMainCols.push_back(x);
			}
		}
		
		if (validMainCols.empty()) {
			// Fallback: use any available column
			int x = rng() % w;
			result.push_back({x, 0});
			return result;
		}
		
		// Select a random valid main column
		int selectedCol = validMainCols[rng() % validMainCols.size()];
		
		// Return both main and parallel columns
		result.push_back({selectedCol, 0});     // Main path
		result.push_back({selectedCol + 1, 1}); // Parallel path
		
		return result;
	}
	
	vector<pair<pair<int, int>, int>> getValidDiagonalAOffsets(const vector<pair<int, int>>& startPositions, int w, int h,
														   std::mt19937& rng, const vector<PathInfo>& existingPaths) {
		vector<pair<pair<int, int>, int>> result;
		
		if (!parallel.get()) {
			// Non-parallel mode: just return a random start position
			auto pos = startPositions[rng() % startPositions.size()];
			result.push_back({{pos.first, pos.second}, 0});
			return result;
		}
		
		// Get all used diagonal positions by existing diagonal A paths
		vector<pair<int, int>> usedPositions;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == DIAGONAL_A) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedPositions.push_back({segment.x, segment.y});
				}
			}
		}
		
		// Find valid start positions for parallel pairs
		vector<pair<int, int>> validMainStarts;
		for (const auto& pos : startPositions) {
			// Calculate lengths for both main and parallel paths independently
			int mainLength = calculateDiagonalLength(pos.first, pos.second, w, h, DIAGONAL_A);
			int parallelLength = calculateDiagonalLength(pos.first + 1, pos.second, w, h, DIAGONAL_A);
			
			// Check if BOTH paths meet minimum length requirement
			if (mainLength < minLength.get() || parallelLength < minLength.get()) {
				continue; // Skip if either path would be too short
			}
			
			// Check if parallel path start position is valid
			if (pos.first + 1 >= w) {
				continue; // Skip if parallel path would start outside matrix
			}
			
			// Only check for conflicts at the start positions, not along the entire path
			// This allows parallel paths to extend beyond the original paths
			bool mainStartValid = true, parallelStartValid = true;
			
			// Check if start positions conflict with existing path start positions only
			for (const auto& usedPos : usedPositions) {
				// Check main path start
				if (abs(pos.first - usedPos.first) <= 1 && abs(pos.second - usedPos.second) <= 1) {
					mainStartValid = false;
					break;
				}
			}
			
			// Check parallel path start position
			if (mainStartValid) {
				for (const auto& usedPos : usedPositions) {
					if (abs((pos.first + 1) - usedPos.first) <= 1 && abs(pos.second - usedPos.second) <= 1) {
						parallelStartValid = false;
						break;
					}
				}
			}
			
			if (mainStartValid && parallelStartValid) {
				validMainStarts.push_back(pos);
			}
		}
		
		if (validMainStarts.empty()) {
			// More permissive fallback: try to generate parallel paths even with potential conflicts
			// This ensures parallel paths can extend beyond original paths
			for (const auto& pos : startPositions) {
				int mainLength = calculateDiagonalLength(pos.first, pos.second, w, h, DIAGONAL_A);
				int parallelLength = calculateDiagonalLength(pos.first + 1, pos.second, w, h, DIAGONAL_A);
				
				// Only check minimum length and boundary conditions
				if (mainLength >= minLength.get() &&
					parallelLength >= minLength.get() &&
					pos.first + 1 < w) {
					validMainStarts.push_back(pos);
				}
			}
		}
		
		if (validMainStarts.empty()) {
			// Final fallback: use any available start position that meets minimum length
			for (const auto& pos : startPositions) {
				int mainLength = calculateDiagonalLength(pos.first, pos.second, w, h, DIAGONAL_A);
				if (mainLength >= minLength.get()) {
					result.push_back({{pos.first, pos.second}, 0});
					return result;
				}
			}
			// If no valid positions found, return empty
			return result;
		}
		
		// Select a random valid main start position
		auto selectedStart = validMainStarts[rng() % validMainStarts.size()];
		
		// Return both main and parallel start positions
		result.push_back({{selectedStart.first, selectedStart.second}, 0});     // Main path
		result.push_back({{selectedStart.first + 1, selectedStart.second}, 1}); // Parallel path
		
		return result;
	}
	
	vector<pair<pair<int, int>, int>> getValidDiagonalBOffsets(const vector<pair<int, int>>& startPositions, int w, int h,
														   std::mt19937& rng, const vector<PathInfo>& existingPaths) {
		vector<pair<pair<int, int>, int>> result;
		
		if (!parallel.get()) {
			// Non-parallel mode: just return a random start position
			auto pos = startPositions[rng() % startPositions.size()];
			result.push_back({{pos.first, pos.second}, 0});
			return result;
		}
		
		// Get all used diagonal positions by existing diagonal B paths
		vector<pair<int, int>> usedPositions;
		for (const PathInfo& pathInfo : existingPaths) {
			if (pathInfo.type == DIAGONAL_B) {
				for (const PathSegment& segment : pathInfo.segments) {
					usedPositions.push_back({segment.x, segment.y});
				}
			}
		}
		
		// Find valid start positions for parallel pairs
		vector<pair<int, int>> validMainStarts;
		for (const auto& pos : startPositions) {
			// Calculate lengths for both main and parallel paths independently
			int mainLength = calculateDiagonalLength(pos.first, pos.second, w, h, DIAGONAL_B);
			int parallelLength = calculateDiagonalLength(pos.first + 1, pos.second, w, h, DIAGONAL_B);
			
			// Check if BOTH paths meet minimum length requirement
			if (mainLength < minLength.get() || parallelLength < minLength.get()) {
				continue; // Skip if either path would be too short
			}
			
			// Check if parallel path start position is valid
			if (pos.first + 1 >= w) {
				continue; // Skip if parallel path would start outside matrix
			}
			
			// Only check for conflicts at the start positions, not along the entire path
			// This allows parallel paths to extend beyond the original paths
			bool mainStartValid = true, parallelStartValid = true;
			
			// Check if start positions conflict with existing path start positions only
			for (const auto& usedPos : usedPositions) {
				// Check main path start
				if (abs(pos.first - usedPos.first) <= 1 && abs(pos.second - usedPos.second) <= 1) {
					mainStartValid = false;
					break;
				}
			}
			
			// Check parallel path start position
			if (mainStartValid) {
				for (const auto& usedPos : usedPositions) {
					if (abs((pos.first + 1) - usedPos.first) <= 1 && abs(pos.second - usedPos.second) <= 1) {
						parallelStartValid = false;
						break;
					}
				}
			}
			
			if (mainStartValid && parallelStartValid) {
				validMainStarts.push_back(pos);
			}
		}
		
		if (validMainStarts.empty()) {
			// More permissive fallback: try to generate parallel paths even with potential conflicts
			// This ensures parallel paths can extend beyond original paths
			for (const auto& pos : startPositions) {
				int mainLength = calculateDiagonalLength(pos.first, pos.second, w, h, DIAGONAL_B);
				int parallelLength = calculateDiagonalLength(pos.first + 1, pos.second, w, h, DIAGONAL_B);
				
				// Only check minimum length and boundary conditions
				if (mainLength >= minLength.get() &&
					parallelLength >= minLength.get() &&
					pos.first + 1 < w) {
					validMainStarts.push_back(pos);
				}
			}
		}
		
		if (validMainStarts.empty()) {
			// Final fallback: use any available start position that meets minimum length
			for (const auto& pos : startPositions) {
				int mainLength = calculateDiagonalLength(pos.first, pos.second, w, h, DIAGONAL_B);
				if (mainLength >= minLength.get()) {
					result.push_back({{pos.first, pos.second}, 0});
					return result;
				}
			}
			// If no valid positions found, return empty
			return result;
		}
		
		// Select a random valid main start position
		auto selectedStart = validMainStarts[rng() % validMainStarts.size()];
		
		// Return both main and parallel start positions
		result.push_back({{selectedStart.first, selectedStart.second}, 0});     // Main path
		result.push_back({{selectedStart.first + 1, selectedStart.second}, 1}); // Parallel path
		
		return result;
	}
	
	void generateMasks(const vector<PathInfo>& allPaths, vector<float>& maskDiagA, vector<float>& maskDiagB,
					  vector<float>& maskHor, vector<float>& maskVert, int w, int h, const vector<float>& finalMatrix) {
		
		// For each position in the matrix, determine which path type is present
		for (int i = 0; i < finalMatrix.size(); i++) {
			if (finalMatrix[i] == -1.0f) continue; // Skip empty positions
			
			float finalAngle = finalMatrix[i];
			
			// Check against all possible angles for each path type
			// Main angles
			if (abs(finalAngle - 0.125f) < 0.01f) {
				maskDiagA[i] = 1.0f;
			} else if (abs(finalAngle - 0.375f) < 0.01f) {
				maskDiagB[i] = 1.0f;
			} else if (abs(finalAngle - 0.5f) < 0.01f) {
				maskHor[i] = 1.0f;
			} else if (abs(finalAngle - 0.25f) < 0.01f) {
				maskVert[i] = 1.0f;
			}
			// Inverted angles (for parallel paths)
			else if (abs(finalAngle - 0.625f) < 0.01f) { // 0.125 + 0.5
				maskDiagA[i] = 1.0f;
			} else if (abs(finalAngle - 0.875f) < 0.01f) { // 0.375 + 0.5
				maskDiagB[i] = 1.0f;
			} else if (abs(finalAngle - 0.0f) < 0.01f) { // 0.5 + 0.5 = 1.0 -> 0.0
				maskHor[i] = 1.0f;
			} else if (abs(finalAngle - 0.75f) < 0.01f) { // 0.25 + 0.5
				maskVert[i] = 1.0f;
			}
		}
	}
};

#endif /* pathwayGenerator_h */
