#ifndef padXY_h
#define padXY_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <deque>

class padXY : public ofxOceanodeNodeModel {
public:
    padXY() : ofxOceanodeNodeModel("Pad XY") {
        isControlPointActive = false;
    }
    
    struct Point {
        float x, y;
        float size;
        std::deque<std::pair<float, float>> history; // Stores past positions
    };

    void setup() override {
        description = "XY pad that displays input points with trails and allows interactive control of an output point.";
        
        // Add parameters
        addParameter(xIn.set("X In", {0}, {-1}, {1}));
        addParameter(yIn.set("Y In", {0}, {-1}, {1}));
        addParameter(pointSizes.set("Point Sizes", {0.5}, {0}, {1}));
        addParameter(trail.set("Trail", 0.5, 0, 1));
        addParameter(size.set("Size", 240, 100, 500));
        addOutputParameter(xOut.set("X Out", 0, -1, 1));
        addOutputParameter(yOut.set("Y Out", 0, -1, 1));
        
        // Initialize controlled point
        controlPoint.x = 0;
        controlPoint.y = 0;
        controlPoint.size = 0.25;

        // Add display region
        addCustomRegion(padRegion.set("Pad Region", [this](){
            drawPad();
        }), [this](){
            drawPad();
        });

        // Add listeners for parameter changes
        listeners.push(xIn.newListener([this](vector<float> &){
            updatePoints();
        }));
        
        listeners.push(yIn.newListener([this](vector<float> &){
            updatePoints();
        }));
        
        listeners.push(pointSizes.newListener([this](vector<float> &){
            updatePoints();
        }));
    }

private:
    // Member variables
    ofParameter<vector<float>> xIn;
    ofParameter<vector<float>> yIn;
    ofParameter<vector<float>> pointSizes;
    ofParameter<float> trail;
    ofParameter<int> size;
    ofParameter<float> xOut;
    ofParameter<float> yOut;
    customGuiRegion padRegion;
    ofEventListeners listeners;
    
    std::vector<Point> points;
    Point controlPoint;
    bool isControlPointActive;

    void updatePoints() {
        // Get the input vectors
        const vector<float>& x = xIn.get();
        const vector<float>& y = yIn.get();
        vector<float> sizes = pointSizes.get();
        
        // Determine how many actual points we have
        size_t numPoints = std::min(x.size(), y.size());
        if(numPoints == 0) {
            points.clear();
            return;
        }
        
        // Ensure we have enough sizes
        while(sizes.size() < numPoints) {
            sizes.push_back(sizes.empty() ? 0.5f : sizes.back());
        }
        
        // Update points vector
        points.resize(numPoints);
        
        // Update each point
        for(size_t i = 0; i < numPoints; i++) {
            points[i].x = x[i];
            points[i].y = y[i];
            points[i].size = sizes[i];
        }
    }
    
    void updateTrails() {
        const size_t MAX_TRAIL = 50;
        
        // Update trail for each point
        for(auto& p : points) {
            // Only add new position if it's different from the last one
            if(p.history.empty() ||
               p.x != p.history.back().first ||
               p.y != p.history.back().second) {
                
                if(p.history.size() >= MAX_TRAIL) {
                    p.history.pop_front();
                }
                p.history.push_back({p.x, p.y});
            }
        }
        
        // Update control point trail only if active
        if(isControlPointActive) {
            if(controlPoint.history.empty() ||
               controlPoint.x != controlPoint.history.back().first ||
               controlPoint.y != controlPoint.history.back().second) {
                
                if(controlPoint.history.size() >= MAX_TRAIL) {
                    controlPoint.history.pop_front();
                }
                controlPoint.history.push_back({controlPoint.x, controlPoint.y});
            }
        }
    }

    void drawPad() {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float padSize = size.get();
        
        // Create invisible button for interaction
        ImGui::InvisibleButton("PadArea", ImVec2(padSize, padSize));
        
        // Handle mouse interaction
        if(ImGui::IsItemActive() && ImGui::IsItemHovered()) {
            isControlPointActive = true;
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            // Convert mouse position to -1 to 1 range
            controlPoint.x = (2.0f * (mousePos.x - pos.x) / padSize) - 1.0f;
            controlPoint.y = (2.0f * (mousePos.y - pos.y) / padSize) - 1.0f;
            // Clamp values
            controlPoint.x = std::max(-1.0f, std::min(1.0f, controlPoint.x));
            controlPoint.y = std::max(-1.0f, std::min(1.0f, controlPoint.y));
            // Update outputs
            xOut = controlPoint.x;
            yOut = controlPoint.y;
        }
        else if(!ImGui::IsItemActive() && isControlPointActive) {
            // Reset control point when released
            isControlPointActive = false;
            controlPoint.history.clear();
        }

        // Draw background
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + padSize, pos.y + padSize),
            IM_COL32(0, 0, 0, 255)
        );
        
        // Draw grid
        float centerX = pos.x + padSize/2;
        float centerY = pos.y + padSize/2;
        drawList->AddLine(
            ImVec2(centerX, pos.y),
            ImVec2(centerX, pos.y + padSize),
            IM_COL32(40, 40, 40, 255)
        );
        drawList->AddLine(
            ImVec2(pos.x, centerY),
            ImVec2(pos.x + padSize, centerY),
            IM_COL32(40, 40, 40, 255)
        );

        updateTrails();
        
        // Draw input points with trails
        float trailValue = trail.get();
        for(const auto& point : points) {
            float pointRadius = (point.size * padSize) / 16.0f;
            
            // Draw trail first (if trail value > 0)
            if(!point.history.empty() && trailValue > 0) {
                for(size_t i = 0; i < point.history.size() - 1; i++) {  // -1 because current pos is drawn separately
                    float alpha = (float(i) / point.history.size()) * trailValue * 255;
                    ImVec2 pointPos(
                        pos.x + (point.history[i].first + 1) * padSize/2,
                        pos.y + (point.history[i].second + 1) * padSize/2
                    );
                    drawList->AddCircleFilled(
                        pointPos,
                        pointRadius,
                        IM_COL32(255, 255, 255, (int)alpha)
                    );
                }
            }
            
            // Always draw current position at full opacity
            ImVec2 currentPos(
                pos.x + (point.x + 1) * padSize/2,
                pos.y + (point.y + 1) * padSize/2
            );
            drawList->AddCircleFilled(
                currentPos,
                pointRadius,
                IM_COL32(255, 255, 255, 255)
            );
        }
        
        // Draw control point with trail only if active
        if(isControlPointActive) {
            float pointRadius = (controlPoint.size * padSize) / 16.0f;
            
            // Draw trail first (if trail value > 0)
            if(!controlPoint.history.empty() && trailValue > 0) {
                for(size_t i = 0; i < controlPoint.history.size() - 1; i++) {  // -1 because current pos is drawn separately
                    float alpha = (float(i) / controlPoint.history.size()) * trailValue * 255;
                    ImVec2 pointPos(
                        pos.x + (controlPoint.history[i].first + 1) * padSize/2,
                        pos.y + (controlPoint.history[i].second + 1) * padSize/2
                    );
                    drawList->AddCircleFilled(
                        pointPos,
                        pointRadius,
                        IM_COL32(255, 0, 0, (int)alpha)
                    );
                }
            }
            
            // Always draw current position at full opacity
            ImVec2 currentPos(
                pos.x + (controlPoint.x + 1) * padSize/2,
                pos.y + (controlPoint.y + 1) * padSize/2
            );
            drawList->AddCircleFilled(
                currentPos,
                pointRadius,
                IM_COL32(255, 0, 0, 255)
            );
        }
    }
};

#endif /* padXY_h */
