#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "Node3D.h"
#include "Edge3D.h"

class SceneGraph {
public:
    SceneGraph();
    std::shared_ptr<Node3D> addNode(const std::string &id, const glm::vec3 &pos);
    void addEdge(const std::string &aId, const std::string &bId);
    std::shared_ptr<Node3D> getNode(const std::string &id);
    void update(float dt);
    const std::vector<std::shared_ptr<Node3D>>& nodes() const { return nodeList; }
    const std::vector<std::shared_ptr<Edge3D>>& edges() const { return edgeList; }
    void setEdgePulse(float v);
private:
    std::vector<std::shared_ptr<Node3D>> nodeList;
    std::vector<std::shared_ptr<Edge3D>> edgeList;
    std::unordered_map<std::string, std::shared_ptr<Node3D>> nodeMap;
};
