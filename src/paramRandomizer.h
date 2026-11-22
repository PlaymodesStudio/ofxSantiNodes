//
//  paramRandomizer.h
//  ofxSantiNodes
//
//  Param Randomizer: on every Randomize, sets its target parameters to random
//  values within each one's own range. The values are written into the
//  parameters themselves, not sent through cables, so snapshots and presets
//  keep recalling them.
//
//  Targets are edited in the Inspector. Add one parameter, or at once: all
//  parameters of a node, the published parameters of a macro (the inputs its
//  routers give it), every parameter inside a macro (its nodes, nested macros
//  included), the routers of the canvas the randomizer sits in, or every
//  parameter of this canvas. Float, int, bool and float-vector parameters are
//  taken; outputs, and settings that change a node's structure (N Chan,
//  Server, Seed, router Min / Max...), are left out of the bulk adds.
//  A target inside a macro is kept as the path through its macro nodes
//  ("Macro_1/Router_f_3/Value").
//
//  Group: targets with the same group number (> 0) get the same random draw,
//  each scaled into its own range (like Reaktor knobs sharing a MIDI CC);
//  group 0 draws on its own (every element of a vector too).
//  Amount: 1 jumps to the random value, less moves that far toward it.
//  Seed: 0 draws fresh values each time, > 0 a reproducible sequence that
//  restarts when Seed changes.
//

#ifndef paramRandomizer_h
#define paramRandomizer_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeMacro.h"
#include "imgui.h"
#include <random>
#include <map>
#include <set>
#include <climits>

class paramRandomizer : public ofxOceanodeNodeModel {
public:
    paramRandomizer() : ofxOceanodeNodeModel("Param Randomizer") {
        description = "Sets its target parameters to random values within their ranges on every Randomize. Targets are edited "
                      "in the Inspector: single parameters, a node, a macro's published parameters, everything inside a macro "
                      "or the whole canvas. Targets with the same Group share one draw; Amount blends toward the random "
                      "values; Seed 0 is fresh, > 0 reproducible.";
    }

    void setup() override {
        addParameter(randomize.set("Randomize"));
        addParameter(amount.set("Amount", 1.f, 0.f, 1.f));
        addParameter(seed.set("Seed", 0, 0, INT_MAX));
        addCustomRegion(summaryRegion.set("Targets", [this](){ drawSummary(); }), [this](){ drawSummary(); });
        addInspectorParameter(editorRegion.set("Target Editor", [this](){ drawEditor(); }));

        reseed();
        listeners.push(randomize.newListener([this](){ apply(); }));
        listeners.push(seed.newListener([this](int&){ reseed(); }));
    }

    void presetSave(ofJson& json) override {
        ofJson list = ofJson::array();
        for(const auto& t : targets) list.push_back({{"path", t.path}, {"group", t.group}});
        json["RandomizerTargets"] = list;
    }

    void presetRecallAfterSettingParameters(ofJson& json) override {
        targets.clear();
        if(json.contains("RandomizerTargets") && json["RandomizerTargets"].is_array()){
            for(const auto& t : json["RandomizerTargets"]){
                if(!t.contains("path")) continue;
                targets.push_back({t["path"].get<std::string>(), t.value("group", 0)});
            }
        }
        reseed();
    }

private:
    struct Target { std::string path; int group = 0; };

    ofParameter<void> randomize;
    ofParameter<float> amount;
    ofParameter<int> seed;
    customGuiRegion summaryRegion;
    ofParameter<std::function<void()>> editorRegion;
    ofEventListeners listeners;
    std::vector<Target> targets;
    std::mt19937 rng;
    char filter[64] = {0};

    void reseed() {
        if(seed.get() > 0) rng.seed((unsigned)seed.get());
        else rng.seed(std::random_device{}());
    }

    float draw() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }

    // ── what can be a target ──────────────────────────────────────────────
    static bool supported(ofxOceanodeAbstractParameter& p) {
        const std::string t = p.valueType();
        return t == typeid(float).name() || t == typeid(int).name() || t == typeid(bool).name()
            || t == typeid(std::vector<float>).name();
    }
    static bool isRouter(ofxOceanodeNodeModel& m) { return m.nodeName().rfind("Router", 0) == 0; }
    // settings that rebuild a node rather than play it: never bulk-added
    static bool structural(const std::string& name) {
        static const std::set<std::string> names = {"N Chan", "Server", "Seed", "Active", "Snapshot", "Unison",
            "Min", "Max", "Name", "Exclude From Snapshots", "nInputs", "Channels", "Voices"};
        return names.count(name) > 0;
    }
    static std::string groupOf(ofxOceanodeNodeModel& m) { return m.getParameterGroup().getEscapedName(); }

    // the parameters of one node a bulk add takes (a router: its Value only)
    void collectNode(ofxOceanodeNodeModel& model, const std::string& prefix, std::vector<std::string>& out) {
        if(&model == this) return;
        auto& group = model.getParameterGroup();
        for(auto& abs : group){
            auto* p = dynamic_cast<ofxOceanodeAbstractParameter*>(abs.get());
            if(p == nullptr || !supported(*p)) continue;
            if(p->getFlags() & ofxOceanodeParameterFlags_DisableInConnection) continue;   // outputs
            if(isRouter(model) ? (p->getName() != "Value") : structural(p->getName())) continue;
            out.push_back(prefix + groupOf(model) + "/" + p->getEscapedName());
        }
    }
    // every node of a canvas, descending into its macros
    void collectCanvas(ofxOceanodeContainer& container, const std::string& prefix, bool intoMacros, std::vector<std::string>& out) {
        for(auto* node : container.getAllModules()){
            auto& model = node->getNodeModel();
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
                if(intoMacros && macro->getContainer()) collectCanvas(*macro->getContainer(), prefix + groupOf(model) + "/", true, out);
                else collectNode(model, prefix, out);          // its published parameters
            }else collectNode(model, prefix, out);
        }
    }
    void add(const std::vector<std::string>& paths) {
        for(const auto& path : paths){
            if(std::none_of(targets.begin(), targets.end(), [&](const Target& t){ return t.path == path; }))
                targets.push_back({path, 0});
        }
    }

    // "Macro_1/Macro_2/Node_1/Param": walk the macro nodes, then find Node_1/Param in the last canvas
    ofxOceanodeAbstractParameter* resolve(const std::string& path) {
        ofxOceanodeContainer* container = getHostContainer();
        if(container == nullptr) return nullptr;
        auto parts = ofSplitString(path, "/");
        if(parts.size() < 2) return nullptr;
        for(size_t i = 0; i + 2 < parts.size(); ++i){
            ofxOceanodeContainer* next = nullptr;
            for(auto* node : container->getAllModules()){
                auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel());
                if(macro && groupOf(*macro) == parts[i] && macro->getContainer()){ next = macro->getContainer().get(); break; }
            }
            if(next == nullptr) return nullptr;
            container = next;
        }
        return container->findParameter(parts[parts.size() - 2] + "/" + parts.back());
    }

    void apply() {
        if(seed.get() == 0) reseed();
        std::map<int, float> shared;
        auto groupDraw = [&](int group) {
            if(group <= 0) return draw();
            auto it = shared.find(group);
            if(it != shared.end()) return it->second;
            const float r = draw();
            shared[group] = r;
            return r;
        };
        const float a = ofClamp(amount.get(), 0.f, 1.f);
        for(const auto& t : targets){
            auto* p = resolve(t.path);
            if(p == nullptr || p->getNodeModel() == this || p->hasInConnection()) continue;
            const std::string type = p->valueType();
            if(type == typeid(float).name()){
                auto& f = p->cast<float>().getParameter();
                const float target = ofLerp(f.getMin(), f.getMax(), groupDraw(t.group));
                f = ofLerp(f.get(), target, a);
            }else if(type == typeid(int).name()){
                auto& i = p->cast<int>().getParameter();
                const float target = ofLerp(i.getMin(), i.getMax(), groupDraw(t.group));
                i = (int)std::round(ofLerp((float)i.get(), target, a));
            }else if(type == typeid(bool).name()){
                auto& b = p->cast<bool>().getParameter();
                if(draw() < a) b = groupDraw(t.group) >= 0.5f;
            }else if(type == typeid(std::vector<float>).name()){
                auto& v = p->cast<std::vector<float>>().getParameter();
                std::vector<float> value = v.get();
                const float lo = v.getMin().empty() ? 0.f : v.getMin()[0];
                const float hi = v.getMax().empty() ? 1.f : v.getMax()[0];
                const float r = t.group > 0 ? groupDraw(t.group) : 0.f;
                for(size_t k = 0; k < value.size(); ++k){
                    const float mn = k < v.getMin().size() ? v.getMin()[k] : lo;
                    const float mx = k < v.getMax().size() ? v.getMax()[k] : hi;
                    value[k] = ofLerp(value[k], ofLerp(mn, mx, t.group > 0 ? r : draw()), a);
                }
                v = value;
            }
        }
    }

    // ── node: a one-line summary ─────────────────────────────────────────
    void drawSummary() {
        int missing = 0;
        for(const auto& t : targets) if(resolve(t.path) == nullptr) ++missing;
        ImGui::TextDisabled("%d target%s%s", (int)targets.size(), targets.size() == 1 ? "" : "s",
                            missing ? "" : "  (Inspector)");
        if(missing){
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%d missing", missing);
        }
    }

    // ── inspector: the target list and the add menu ──────────────────────
    void drawEditor() {
        auto* container = getHostContainer();
        ImGui::PushID(this);
        ImGui::TextUnformatted("Targets");
        ImGui::SameLine();
        if(ImGui::SmallButton("+ Add")) ImGui::OpenPopup("##addTargets");
        ImGui::SameLine();
        if(ImGui::SmallButton("Clear")) targets.clear();
        if(ImGui::BeginPopup("##addTargets")){
            if(container) drawAddMenu(*container);
            ImGui::EndPopup();
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "filter", filter, sizeof(filter));
        const std::string needle = ofToLower(filter);
        int remove = -1;
        if(ImGui::BeginChild("##targetList", ImVec2(0, std::min(320.f, 24.f + 22.f * (float)targets.size())), true)){
            for(int i = 0; i < (int)targets.size(); ++i){
                if(!needle.empty() && ofToLower(targets[i].path).find(needle) == std::string::npos) continue;
                ImGui::PushID(i);
                const bool found = resolve(targets[i].path) != nullptr;
                ImGui::SetNextItemWidth(36);
                ImGui::DragInt("##group", &targets[i].group, 0.1f, 0, 128);
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Group: targets with the same number share one draw; 0 = its own");
                ImGui::SameLine();
                if(ImGui::SmallButton("x")) remove = i;
                ImGui::SameLine();
                if(found) ImGui::TextUnformatted(targets[i].path.c_str());
                else ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s (missing)", targets[i].path.c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        if(remove >= 0) targets.erase(targets.begin() + remove);
        ImGui::PopID();
    }

    void drawAddMenu(ofxOceanodeContainer& container) {
        std::vector<ofxOceanodeNodeMacro*> macros;
        for(auto* node : container.getAllModules())
            if(auto* m = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) macros.push_back(m);

        if(ImGui::BeginMenu("Parameter")){
            for(auto* node : container.getAllModules()){
                auto& model = node->getNodeModel();
                if(&model == this || !ImGui::BeginMenu(model.getParameterGroup().getName().c_str())) continue;
                std::vector<std::string> paths;
                for(auto& abs : model.getParameterGroup()){
                    auto* p = dynamic_cast<ofxOceanodeAbstractParameter*>(abs.get());
                    if(p == nullptr || !supported(*p) || (p->getFlags() & ofxOceanodeParameterFlags_DisableInConnection)) continue;
                    const std::string path = groupOf(model) + "/" + p->getEscapedName();
                    const bool marked = std::any_of(targets.begin(), targets.end(), [&](const Target& t){ return t.path == path; });
                    if(ImGui::MenuItem(p->getName().c_str(), nullptr, marked)){
                        if(marked) targets.erase(std::remove_if(targets.begin(), targets.end(), [&](const Target& t){ return t.path == path; }), targets.end());
                        else targets.push_back({path, 0});
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("All parameters of a node")){
            for(auto* node : container.getAllModules()){
                auto& model = node->getNodeModel();
                if(&model == this) continue;
                if(ImGui::MenuItem(model.getParameterGroup().getName().c_str())){
                    std::vector<std::string> paths; collectNode(model, "", paths); add(paths);
                }
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Published parameters of a macro", !macros.empty())){
            for(auto* m : macros){
                if(ImGui::MenuItem(m->getParameterGroup().getName().c_str())){
                    std::vector<std::string> paths; collectNode(*m, "", paths); add(paths);
                }
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("All parameters inside a macro", !macros.empty())){
            for(auto* m : macros){
                if(ImGui::MenuItem(m->getParameterGroup().getName().c_str()) && m->getContainer()){
                    std::vector<std::string> paths; collectCanvas(*m->getContainer(), groupOf(*m) + "/", true, paths); add(paths);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Routers of this canvas")){
            std::vector<std::string> paths;
            for(auto* node : container.getAllModules())
                if(isRouter(node->getNodeModel())) collectNode(node->getNodeModel(), "", paths);
            add(paths);
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Inside a macro: the parameters it publishes, set from within");
        if(ImGui::MenuItem("All parameters of this canvas")){
            std::vector<std::string> paths; collectCanvas(container, "", false, paths); add(paths);
        }
        if(ImGui::MenuItem("All parameters of this canvas and its macros")){
            std::vector<std::string> paths; collectCanvas(container, "", true, paths); add(paths);
        }
    }
};

#endif /* paramRandomizer_h */
