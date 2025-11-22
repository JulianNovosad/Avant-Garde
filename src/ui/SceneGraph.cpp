#include "SceneGraph.h"

SceneGraph::SceneGraph(){}

std::shared_ptr<Node3D> SceneGraph::addNode(const std::string &id, const glm::vec3 &pos){
    auto n = std::make_shared<Node3D>(id, pos);
    nodeList.push_back(n);
    nodeMap[id] = n;
    return n;
}

void SceneGraph::addEdge(const std::string &aId, const std::string &bId){
    auto a = getNode(aId);
    auto b = getNode(bId);
    if (!a || !b) return;
    auto e = std::make_shared<Edge3D>(aId, bId);
    e->setPositions(a->getPosition(), b->getPosition());
    edgeList.push_back(e);
}

std::shared_ptr<Node3D> SceneGraph::getNode(const std::string &id){
    auto it = nodeMap.find(id);
    if (it==nodeMap.end()) return nullptr;
    return it->second;
}

void SceneGraph::update(float dt){
    for (auto &n: nodeList) n->update(dt);
    for (auto &e: edgeList){
        // update positions from nodes
        auto a = getNode(e->aId());
        auto b = getNode(e->bId());
        if (a && b) e->setPositions(a->getPosition(), b->getPosition());
        e->update(dt);
    }
}

void SceneGraph::setEdgePulse(float v){ for (auto &e: edgeList) e->setPulse(v); }
