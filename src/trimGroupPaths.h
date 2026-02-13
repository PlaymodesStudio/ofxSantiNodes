#ifndef trimGroupPaths_h
#define trimGroupPaths_h

#include "ofxOceanodeNodeModel.h"
#include <map>
#include <set>

class trimGroupPaths : public ofxOceanodeNodeModel
{
public:
	trimGroupPaths() : ofxOceanodeNodeModel("Trim Group Paths"), focusedGroup(-1) {}
	
	void setup() override
	{
		description = "Groups multiple input paths and trims each group sequentially. Click 'Focus' on a group to select paths by clicking on them in the visual preview. Paths can belong to multiple groups. Start/End/Opacity vectors control each group independently. Endpoint Dots adds a dot at each segment endpoint.";
		
		addParameter(pointsX.set("In.X", {0.5}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(pointsY.set("In.Y", {0.5}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(showWindow.set("Show GUI", false));
		
		addParameter(start.set("Start", {0.0}, {0.0}, {1.0}));
		addParameter(end.set("End", {1.0}, {0.0}, {1.0}));
		addParameter(endpointDots.set("Endpoint Dots", false));
		addParameter(opacity.set("Opacity", {1.0}, {0.0}, {1.0}));
		addParameter(red.set("Red", {1.0}, {0.0}, {1.0}));
		addParameter(green.set("Green", {1.0}, {0.0}, {1.0}));
		addParameter(blue.set("Blue", {1.0}, {0.0}, {1.0}));
		
		addOutputParameter(outX.set("Out.X", {0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(outY.set("Out.Y", {0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(opacityOut.set("Opacity Out", {0}, {0}, {1}));
		addOutputParameter(outR.set("Out.R", {0}, {0}, {1}));
		addOutputParameter(outG.set("Out.G", {0}, {0}, {1}));
		addOutputParameter(outB.set("Out.B", {0}, {0}, {1}));
		
		addInspectorParameter(numGroups.set("Num Groups", 1, 1, 100));
		
		// Initialize with one empty group
		pathGroups.resize(1);
		
		listeners.push(numGroups.newListener([this](int &n){
			pathGroups.resize(n);
			if(focusedGroup >= n) focusedGroup = -1;
			calculate();
		}));
		
		listeners.push(endpointDots.newListener([this](bool &b){
			calculate();
		}));
		
		listener = pointsX.newListener([this](vector<float> &vf) {
			calculate();
		});
	}
	
	void draw(ofEventArgs &a) override
	{
		if(showWindow)
		{
			if(ImGui::Begin(("Trim Group Paths " + ofToString(getNumIdentifier())).c_str()))
			{
				// Parse input paths for display
				vector<vector<glm::vec2>> inputPaths;
				vector<glm::vec2> currentPath;
				
				for (size_t i = 0; i < pointsX.get().size() && i < pointsY.get().size(); i++)
				{
					if (pointsX.get()[i] == -1 || pointsY.get()[i] == -1)
					{
						if (!currentPath.empty())
						{
							inputPaths.push_back(currentPath);
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
					inputPaths.push_back(currentPath);
				}
				
				ImGui::Text("Total Paths: %d", (int)inputPaths.size());
				
				// Group management buttons
				ImGui::Text("Groups: %d", numGroups.get());
				ImGui::SameLine();
				if(ImGui::Button("+ Add Group"))
				{
					numGroups = numGroups.get() + 1;
				}
				ImGui::SameLine();
				if(ImGui::Button("- Remove Group"))
				{
					if(numGroups.get() > 1)
					{
						numGroups = numGroups.get() - 1;
					}
				}
				ImGui::Separator();
				
				// Display groups with focus buttons
				ImGui::Text("Click 'Focus' on a group, then click paths in the preview to add/remove them.");
				ImGui::Separator();
				
				for(int groupIdx = 0; groupIdx < numGroups; groupIdx++)
				{
					ImGui::PushID(groupIdx);
					
					ImVec4 groupColor = getGroupColor(groupIdx);
					bool isFocused = (focusedGroup == groupIdx);
					
					// Focus button
					if(isFocused)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, groupColor);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(groupColor.x*1.2f, groupColor.y*1.2f, groupColor.z*1.2f, 1.0f));
						if(ImGui::Button("FOCUSED"))
						{
							focusedGroup = -1; // Unfocus
						}
						ImGui::PopStyleColor(2);
					}
					else
					{
						if(ImGui::Button("Focus"))
						{
							focusedGroup = groupIdx;
						}
					}
					
					ImGui::SameLine();
					
					// Group info with color indicator
					ImGui::PushStyleColor(ImGuiCol_Text, groupColor);
					ImGui::Text("Group %d:", groupIdx);
					ImGui::PopStyleColor();
					
					ImGui::SameLine();
					
					// Display current paths in this group
					if(pathGroups[groupIdx].empty())
					{
						ImGui::TextDisabled("(empty)");
					}
					else
					{
						string pathList;
						for(int pathIdx : pathGroups[groupIdx])
						{
							pathList += ofToString(pathIdx) + ", ";
						}
						if(!pathList.empty())
						{
							pathList.erase(pathList.end()-2, pathList.end());
						}
						ImGui::Text("%s", pathList.c_str());
					}
					
					ImGui::PopID();
				}
				
				ImGui::Separator();
				
				// Visual preview section with interaction
				ImGui::Text("Visual Preview - Click paths to select/deselect");
				if(focusedGroup >= 0)
				{
					ImVec4 focusColor = getGroupColor(focusedGroup);
					ImGui::PushStyleColor(ImGuiCol_Text, focusColor);
					ImGui::Text("Editing: Group %d", focusedGroup);
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::TextDisabled("Focus a group to edit");
				}
				
				auto screenSize = ImGui::GetContentRegionAvail();
				
				// Ensure valid size
				if(screenSize.x <= 1.0f || screenSize.y <= 1.0f ||
				   screenSize.x != screenSize.x || screenSize.y != screenSize.y) // Check for NaN
				{
					ImGui::Text("Window too small for preview");
				}
				else
				{
					screenSize.y = std::min(screenSize.y, 500.0f);
					screenSize.x = std::max(screenSize.x, 100.0f);
					screenSize.y = std::max(screenSize.y, 100.0f);
					
					auto screenPos = ImGui::GetCursorScreenPos();
					
					ImDrawList* draw_list = ImGui::GetWindowDrawList();
					
					// Draw background
					draw_list->AddRectFilled(screenPos,
											ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
											IM_COL32(20, 20, 20, 255));
					
					// Track hovered path
					int hoveredPath = -1;
					bool isPreviewHovered = false;
					
					// Create invisible button for interaction
					ImGui::InvisibleButton("PreviewArea", screenSize);
					isPreviewHovered = ImGui::IsItemHovered();
					
					if(isPreviewHovered)
					{
						ImVec2 mousePos = ImGui::GetMousePos();
						glm::vec2 normMousePos((mousePos.x - screenPos.x) / screenSize.x,
											  (mousePos.y - screenPos.y) / screenSize.y);
						
						// Find which path is being hovered
						float minDist = 0.02f; // Threshold distance
						
						for(int pathIdx = 0; pathIdx < inputPaths.size(); pathIdx++)
						{
							const auto& path = inputPaths[pathIdx];
							
							for(size_t i = 0; i < path.size() - 1; i++)
							{
								// Check distance to line segment
								glm::vec2 p1 = path[i];
								glm::vec2 p2 = path[i+1];
								
								float dist = distanceToSegment(normMousePos, p1, p2);
								
								if(dist < minDist)
								{
									minDist = dist;
									hoveredPath = pathIdx;
								}
							}
						}
						
						// Handle click to add/remove path from focused group
						if(ImGui::IsMouseClicked(0) && focusedGroup >= 0 && hoveredPath >= 0)
						{
							auto& group = pathGroups[focusedGroup];
							auto it = std::find(group.begin(), group.end(), hoveredPath);
							
							if(it != group.end())
							{
								// Remove from group
								group.erase(it);
							}
							else
							{
								// Add to group
								group.push_back(hoveredPath);
								std::sort(group.begin(), group.end());
							}
							calculate();
						}
					}
					
					// Draw all input paths
					for(size_t pathIdx = 0; pathIdx < inputPaths.size(); pathIdx++)
					{
						const auto& path = inputPaths[pathIdx];
						
						// Determine if this path is in the focused group
						bool isInFocusedGroup = false;
						ImVec4 focusedGroupColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
						
						if(focusedGroup >= 0)
						{
							// Check if path is in focused group
							if(std::find(pathGroups[focusedGroup].begin(),
										pathGroups[focusedGroup].end(),
										pathIdx) != pathGroups[focusedGroup].end())
							{
								isInFocusedGroup = true;
								focusedGroupColor = getGroupColor(focusedGroup);
							}
						}
						
						// Determine draw color
						ImVec4 drawColor;
						float alpha = 1.0f;
						
						if(focusedGroup >= 0)
						{
							// A group is focused
							if(isInFocusedGroup)
							{
								// This path is in the focused group - show in group color
								drawColor = focusedGroupColor;
								alpha = 1.0f;
							}
							else
							{
								// This path is NOT in the focused group - dim it heavily
								drawColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
								alpha = 0.3f;
							}
						}
						else
						{
							// No group focused - show all groups' colors
							vector<ImVec4> pathColors;
							for(int groupIdx = 0; groupIdx < pathGroups.size(); groupIdx++)
							{
								if(std::find(pathGroups[groupIdx].begin(),
											pathGroups[groupIdx].end(),
											pathIdx) != pathGroups[groupIdx].end())
								{
									pathColors.push_back(getGroupColor(groupIdx));
								}
							}
							
							if(pathColors.empty())
							{
								drawColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
							}
							else
							{
								drawColor = pathColors[0];
							}
							alpha = 1.0f;
						}
						
						// Highlight if hovered
						bool isHovered = (hoveredPath == pathIdx);
						float lineWidth = isHovered ? 6.0f : 3.0f;
						
						// Draw path
						for(size_t i = 0; i < path.size() - 1; i++)
						{
							ImVec2 p1(screenPos.x + path[i].x * screenSize.x,
									 screenPos.y + path[i].y * screenSize.y);
							ImVec2 p2(screenPos.x + path[i+1].x * screenSize.x,
									 screenPos.y + path[i+1].y * screenSize.y);
							
							draw_list->AddLine(p1, p2,
											 IM_COL32(drawColor.x*255, drawColor.y*255, drawColor.z*255, alpha*255),
											 lineWidth);
							
							// Draw endpoint dots in preview if enabled
							if(endpointDots.get())
							{
								float dotRadius = 4.0f;
								draw_list->AddCircleFilled(p1, dotRadius,
									IM_COL32(drawColor.x*255, drawColor.y*255, drawColor.z*255, alpha*255), 8);
								draw_list->AddCircleFilled(p2, dotRadius,
									IM_COL32(drawColor.x*255, drawColor.y*255, drawColor.z*255, alpha*255), 8);
							}
						}
						
						// Draw path index label
						if(!path.empty())
						{
							ImVec2 labelPos(screenPos.x + path[0].x * screenSize.x + 5,
										  screenPos.y + path[0].y * screenSize.y - 10);
							string label = ofToString(pathIdx);
							
							// Background for label
							ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
							draw_list->AddRectFilled(ImVec2(labelPos.x - 2, labelPos.y - 2),
													ImVec2(labelPos.x + textSize.x + 2, labelPos.y + textSize.y + 2),
													IM_COL32(0, 0, 0, 200));
							
							draw_list->AddText(labelPos, IM_COL32(255, 255, 255, 255), label.c_str());
						}
					}
					
					// Show hover tooltip
					if(hoveredPath >= 0 && focusedGroup >= 0)
					{
						bool isInFocusedGroup = std::find(pathGroups[focusedGroup].begin(),
														  pathGroups[focusedGroup].end(),
														  hoveredPath) != pathGroups[focusedGroup].end();
						
						ImGui::BeginTooltip();
						ImGui::Text("Path %d", hoveredPath);
						ImGui::Text(isInFocusedGroup ? "Click to remove from group" : "Click to add to group");
						ImGui::EndTooltip();
					}
				}
			}
			ImGui::End();
		}
	}
	
	void calculate()
	{
		// Parse input into individual paths
		vector<vector<glm::vec2>> inputPaths;
		vector<glm::vec2> currentPath;
		
		for (size_t i = 0; i < pointsX.get().size() && i < pointsY.get().size(); i++)
		{
			if (pointsX.get()[i] == -1 || pointsY.get()[i] == -1)
			{
				if (!currentPath.empty())
				{
					inputPaths.push_back(currentPath);
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
			inputPaths.push_back(currentPath);
		}
		
		if (inputPaths.empty())
		{
			outX = vector<float>();
			outY = vector<float>();
			opacityOut = vector<float>();
			outR = vector<float>();
			outG = vector<float>();
			outB = vector<float>();
			return;
		}
		
		// Store trimmed results per path per group
		// We need to track which group each segment belongs to for proper opacity and color mapping
		vector<map<int, vector<glm::vec2>>> trimmedPathsByGroup(inputPaths.size());
		vector<map<int, vector<float>>> pathOpacitiesByGroup(inputPaths.size());
		vector<map<int, vector<float>>> pathColorsRByGroup(inputPaths.size());
		vector<map<int, vector<float>>> pathColorsGByGroup(inputPaths.size());
		vector<map<int, vector<float>>> pathColorsBByGroup(inputPaths.size());
		
		// Store endpoint dots separately per path per group
		vector<map<int, vector<glm::vec2>>> endpointDotsByGroup(inputPaths.size());
		vector<map<int, vector<float>>> endpointDotsOpacityByGroup(inputPaths.size());
		vector<map<int, vector<float>>> endpointDotsRByGroup(inputPaths.size());
		vector<map<int, vector<float>>> endpointDotsGByGroup(inputPaths.size());
		vector<map<int, vector<float>>> endpointDotsBByGroup(inputPaths.size());
		
		bool addDots = endpointDots.get();
		
		// Process each group
		for (int groupIdx = 0; groupIdx < pathGroups.size(); groupIdx++)
		{
			const vector<int>& pathIndices = pathGroups[groupIdx];
			if(pathIndices.empty()) continue;
			
			// Get trim parameters for this group
			float groupStart = getValueForIndex(start.get(), groupIdx, 0.0f);
			float groupEnd = getValueForIndex(end.get(), groupIdx, 1.0f);
			float groupOpacity = getValueForIndex(opacity.get(), groupIdx, 1.0f);
			float groupR = getValueForIndex(red.get(), groupIdx, 1.0f);
			float groupG = getValueForIndex(green.get(), groupIdx, 1.0f);
			float groupB = getValueForIndex(blue.get(), groupIdx, 1.0f);
			
			// Handle start > end case
			if (groupStart > groupEnd)
			{
				continue;
			}
			
			// Calculate total segments in this group
			size_t totalSegments = 0;
			for (int pathIdx : pathIndices)
			{
				if (pathIdx < inputPaths.size() && inputPaths[pathIdx].size() > 1)
				{
					totalSegments += inputPaths[pathIdx].size() - 1;
				}
			}
			
			if (totalSegments == 0) continue;
			
			// Process each path in the group sequentially
			size_t globalSegmentIndex = 0;
			
			for (int pathIdx : pathIndices)
			{
				if(pathIdx >= inputPaths.size()) continue;
				
				const auto& path = inputPaths[pathIdx];
				if (path.size() < 2) continue;
				
				vector<glm::vec2> groupTrimmedPath;
				vector<float> groupPathOpacity;
				vector<float> groupPathR;
				vector<float> groupPathG;
				vector<float> groupPathB;
				
				vector<glm::vec2> groupEndpointDots;
				vector<float> groupEndpointDotsOpacity;
				vector<float> groupEndpointDotsR;
				vector<float> groupEndpointDotsG;
				vector<float> groupEndpointDotsB;
				
				// Use a set to track unique endpoint positions (avoid duplicates)
				set<pair<float, float>> addedDots;
				
				// Process each segment in this path
				for (size_t segIdx = 0; segIdx < path.size() - 1; segIdx++)
				{
					float segmentStartProgress = (float)globalSegmentIndex / (float)totalSegments;
					float segmentEndProgress = (float)(globalSegmentIndex + 1) / (float)totalSegments;
					
					float segmentStart = 0.0f;
					float segmentEnd = 0.0f;
					
					if (groupEnd <= segmentStartProgress)
					{
						segmentStart = 0.0f;
						segmentEnd = 0.0f;
					}
					else if (groupStart > segmentEndProgress)
					{
						segmentStart = 0.0f;
						segmentEnd = 0.0f;
					}
					else if (groupStart == groupEnd)
					{
						segmentStart = 0.0f;
						segmentEnd = 0.0f;
					}
					else
					{
						if (groupStart <= segmentStartProgress && groupEnd >= segmentEndProgress)
						{
							segmentStart = 0.0f;
							segmentEnd = 1.0f;
						}
						else
						{
							float segmentLength = segmentEndProgress - segmentStartProgress;
							
							if (groupStart > segmentStartProgress)
							{
								segmentStart = (groupStart - segmentStartProgress) / segmentLength;
							}
							else
							{
								segmentStart = 0.0f;
							}
							
							if (groupEnd < segmentEndProgress)
							{
								segmentEnd = (groupEnd - segmentStartProgress) / segmentLength;
							}
							else
							{
								segmentEnd = 1.0f;
							}
						}
					}
					
					// Add trimmed segment if visible
					if (segmentStart != segmentEnd && abs(segmentEnd - segmentStart) > 0.001f)
					{
						glm::vec2 point1 = path[segIdx];
						glm::vec2 point2 = path[segIdx + 1];
						
						float startT = glm::clamp(segmentStart, 0.0f, 1.0f);
						float endT = glm::clamp(segmentEnd, 0.0f, 1.0f);
						
						glm::vec2 startPoint = point1 + startT * (point2 - point1);
						glm::vec2 endPoint = point1 + endT * (point2 - point1);
						
						if (glm::distance(startPoint, endPoint) > 0.0001f)
						{
							groupTrimmedPath.push_back(startPoint);
							groupTrimmedPath.push_back(endPoint);
							
							groupPathOpacity.push_back(groupOpacity);
							groupPathOpacity.push_back(groupOpacity);
							
							groupPathR.push_back(groupR);
							groupPathR.push_back(groupR);
							
							groupPathG.push_back(groupG);
							groupPathG.push_back(groupG);
							
							groupPathB.push_back(groupB);
							groupPathB.push_back(groupB);
							
							// Collect endpoint dots (avoid duplicates using set)
							if (addDots)
							{
								auto startKey = make_pair(startPoint.x, startPoint.y);
								auto endKey = make_pair(endPoint.x, endPoint.y);
								
								if (addedDots.find(startKey) == addedDots.end())
								{
									addedDots.insert(startKey);
									groupEndpointDots.push_back(startPoint);
									groupEndpointDotsOpacity.push_back(groupOpacity);
									groupEndpointDotsR.push_back(groupR);
									groupEndpointDotsG.push_back(groupG);
									groupEndpointDotsB.push_back(groupB);
								}
								
								if (addedDots.find(endKey) == addedDots.end())
								{
									addedDots.insert(endKey);
									groupEndpointDots.push_back(endPoint);
									groupEndpointDotsOpacity.push_back(groupOpacity);
									groupEndpointDotsR.push_back(groupR);
									groupEndpointDotsG.push_back(groupG);
									groupEndpointDotsB.push_back(groupB);
								}
							}
						}
					}
					
					globalSegmentIndex++;
				}
				
				// Store results for this path and group
				if(!groupTrimmedPath.empty())
				{
					trimmedPathsByGroup[pathIdx][groupIdx] = groupTrimmedPath;
					pathOpacitiesByGroup[pathIdx][groupIdx] = groupPathOpacity;
					pathColorsRByGroup[pathIdx][groupIdx] = groupPathR;
					pathColorsGByGroup[pathIdx][groupIdx] = groupPathG;
					pathColorsBByGroup[pathIdx][groupIdx] = groupPathB;
				}
				
				if(!groupEndpointDots.empty())
				{
					endpointDotsByGroup[pathIdx][groupIdx] = groupEndpointDots;
					endpointDotsOpacityByGroup[pathIdx][groupIdx] = groupEndpointDotsOpacity;
					endpointDotsRByGroup[pathIdx][groupIdx] = groupEndpointDotsR;
					endpointDotsGByGroup[pathIdx][groupIdx] = groupEndpointDotsG;
					endpointDotsBByGroup[pathIdx][groupIdx] = groupEndpointDotsB;
				}
			}
		}
		
		// Assemble output in original path order
		// For each path, concatenate all its groups' segments
		vector<float> finalX, finalY, finalOpacity;
		vector<float> finalR, finalG, finalB;
		
		for (size_t pathIdx = 0; pathIdx < inputPaths.size(); pathIdx++)
		{
			vector<float> pathX, pathY, pathOp, pathR, pathG, pathB;
			
			// Combine all groups for this path
			for(int groupIdx = 0; groupIdx < pathGroups.size(); groupIdx++)
			{
				if(trimmedPathsByGroup[pathIdx].count(groupIdx) > 0)
				{
					const auto& path = trimmedPathsByGroup[pathIdx][groupIdx];
					const auto& opacities = pathOpacitiesByGroup[pathIdx][groupIdx];
					const auto& colorsR = pathColorsRByGroup[pathIdx][groupIdx];
					const auto& colorsG = pathColorsGByGroup[pathIdx][groupIdx];
					const auto& colorsB = pathColorsBByGroup[pathIdx][groupIdx];
					
					for (size_t i = 0; i < path.size(); i++)
					{
						pathX.push_back(path[i].x);
						pathY.push_back(path[i].y);
						
						if (i < opacities.size())
						{
							pathOp.push_back(opacities[i]);
						}
						else
						{
							pathOp.push_back(1.0f);
						}
						
						if (i < colorsR.size()) pathR.push_back(colorsR[i]);
						else pathR.push_back(1.0f);
						
						if (i < colorsG.size()) pathG.push_back(colorsG[i]);
						else pathG.push_back(1.0f);
						
						if (i < colorsB.size()) pathB.push_back(colorsB[i]);
						else pathB.push_back(1.0f);
					}
				}
			}
			
			// Add this path's data to final output
			if(!pathX.empty())
			{
				finalX.insert(finalX.end(), pathX.begin(), pathX.end());
				finalY.insert(finalY.end(), pathY.begin(), pathY.end());
				finalOpacity.insert(finalOpacity.end(), pathOp.begin(), pathOp.end());
				finalR.insert(finalR.end(), pathR.begin(), pathR.end());
				finalG.insert(finalG.end(), pathG.begin(), pathG.end());
				finalB.insert(finalB.end(), pathB.begin(), pathB.end());
				
				// Add separator
				finalX.push_back(-1);
				finalY.push_back(-1);
				// No opacity or color for separator
			}
			
			// Add endpoint dots for this path (after all segments)
			if(addDots)
			{
				for(int groupIdx = 0; groupIdx < pathGroups.size(); groupIdx++)
				{
					if(endpointDotsByGroup[pathIdx].count(groupIdx) > 0)
					{
						const auto& dots = endpointDotsByGroup[pathIdx][groupIdx];
						const auto& opacities = endpointDotsOpacityByGroup[pathIdx][groupIdx];
						const auto& colorsR = endpointDotsRByGroup[pathIdx][groupIdx];
						const auto& colorsG = endpointDotsGByGroup[pathIdx][groupIdx];
						const auto& colorsB = endpointDotsBByGroup[pathIdx][groupIdx];
						
						for(size_t i = 0; i < dots.size(); i++)
						{
							// Each dot is a single point followed by separator
							finalX.push_back(dots[i].x);
							finalY.push_back(dots[i].y);
							
							if(i < opacities.size()) finalOpacity.push_back(opacities[i]);
							else finalOpacity.push_back(1.0f);
							
							if(i < colorsR.size()) finalR.push_back(colorsR[i]);
							else finalR.push_back(1.0f);
							
							if(i < colorsG.size()) finalG.push_back(colorsG[i]);
							else finalG.push_back(1.0f);
							
							if(i < colorsB.size()) finalB.push_back(colorsB[i]);
							else finalB.push_back(1.0f);
							
							// Add separator after each dot
							finalX.push_back(-1);
							finalY.push_back(-1);
						}
					}
				}
			}
		}
		
		// Debug logging - detailed per-path breakdown
		ofLogNotice("trimGroupPaths") << "=== OUTPUT BREAKDOWN ===";
		int pathCount = 0;
		int vertexCount = 0;
		int separatorCount = 0;
		for(float val : finalX) {
			if(val == -1) {
				separatorCount++;
				ofLogNotice("trimGroupPaths") << "Path " << pathCount << ": " << vertexCount << " vertices";
				pathCount++;
				vertexCount = 0;
			} else {
				vertexCount++;
			}
		}
		
		ofLogNotice("trimGroupPaths") << "Total paths: " << pathCount;
		ofLogNotice("trimGroupPaths") << "Total vertices (no -1): " << finalOpacity.size();
		ofLogNotice("trimGroupPaths") << "Total X size (with -1): " << finalX.size();
		
		// Check if any opacity values are problematic
		int lowOpacityCount = 0;
		for(float op : finalOpacity) {
			if(op < 0.99f && op > 0.01f) lowOpacityCount++;
		}
		ofLogNotice("trimGroupPaths") << "Vertices with partial opacity (0.01-0.99): " << lowOpacityCount;
		
		outX = finalX;
		outY = finalY;
		opacityOut = finalOpacity;
		outR = finalR;
		outG = finalG;
		outB = finalB;
	}
	
	void presetSave(ofJson &json) override
	{
		json["NumGroups"] = numGroups.get();
		for(int i = 0; i < pathGroups.size(); i++)
		{
			string groupStr;
			for(int pathIdx : pathGroups[i])
			{
				groupStr += ofToString(pathIdx) + ",";
			}
			if(!groupStr.empty())
			{
				groupStr.pop_back();
			}
			json["Group_" + ofToString(i)] = groupStr;
		}
	}
	
	void presetRecallBeforeSettingParameters(ofJson &json) override
	{
		if(json.count("NumGroups") > 0)
		{
			int savedNumGroups = json["NumGroups"];
			numGroups = savedNumGroups;
			pathGroups.resize(savedNumGroups);
			
			for(int i = 0; i < savedNumGroups; i++)
			{
				string key = "Group_" + ofToString(i);
				if(json.count(key) > 0)
				{
					string groupStr = json[key];
					if(!groupStr.empty())
					{
						vector<string> indices = ofSplitString(groupStr, ",");
						pathGroups[i].clear();
						for(const string& idxStr : indices)
						{
							pathGroups[i].push_back(ofToInt(idxStr));
						}
					}
				}
			}
		}
	}
	
private:
	float distanceToSegment(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
	{
		glm::vec2 pa = p - a;
		glm::vec2 ba = b - a;
		float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
		return glm::length(pa - ba * h);
	}
	
	template<typename T>
	T getValueForIndex(const vector<T>& vec, int index, T defaultValue)
	{
		if (vec.empty()) return defaultValue;
		if (index < 0) return defaultValue;
		if (index >= vec.size()) return defaultValue;
		return vec[index];
	}
	
	ImVec4 getGroupColor(int groupIdx)
	{
		float hue = (groupIdx * 0.618033988749895f);
		hue = hue - floor(hue);
		
		float s = 0.8f;
		float v = 0.9f;
		int hi = (int)(hue * 6);
		float f = hue * 6 - hi;
		float p = v * (1 - s);
		float q = v * (1 - f * s);
		float t = v * (1 - (1 - f) * s);
		
		float r, g, b;
		switch(hi)
		{
			case 0: r = v; g = t; b = p; break;
			case 1: r = q; g = v; b = p; break;
			case 2: r = p; g = v; b = t; break;
			case 3: r = p; g = q; b = v; break;
			case 4: r = t; g = p; b = v; break;
			case 5: r = v; g = p; b = q; break;
			default: r = v; g = t; b = p; break;
		}
		
		return ImVec4(r, g, b, 1.0f);
	}
	
	ofParameter<vector<float>> pointsX, pointsY;
	ofParameter<vector<float>> start, end, opacity;
	ofParameter<vector<float>> red, green, blue;
	ofParameter<vector<float>> outX, outY, opacityOut;
	ofParameter<vector<float>> outR, outG, outB;
	ofParameter<bool> showWindow;
	ofParameter<bool> endpointDots;
	ofParameter<int> numGroups;
	ofEventListener listener;
	ofEventListeners listeners;
	
	vector<vector<int>> pathGroups;
	int focusedGroup;
};

#endif /* trimGroupPaths_h */
