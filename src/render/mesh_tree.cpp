#include "mesh_tree.h"
#include <iostream>
#include <glm/gtx/quaternion.hpp>
DISABLE_WARNINGS_PUSH()
#include <glm/gtx/transform.hpp>
DISABLE_WARNINGS_POP()
std::unordered_map<MeshTree*, std::shared_ptr<MeshTree>> MemoryManager::objs;
static HitBox makeHitBox(Mesh& cpuMesh, bool allowCollision) {
    std::array<std::array<float, 3>, 2> minMax{};

    for (Vertex vertex : cpuMesh.vertices) {
        if (vertex.position.x < minMax[0][0])
            minMax[0][0] = vertex.position.x;
        if (vertex.position.x > minMax[1][0])
            minMax[1][0] = vertex.position.x;

        if (vertex.position.y < minMax[0][1])
            minMax[0][1] = vertex.position.y;
        if (vertex.position.y > minMax[1][1])
            minMax[1][1] = vertex.position.y;

        if (vertex.position.z < minMax[0][2])
            minMax[0][2] = vertex.position.z;
        if (vertex.position.z > minMax[1][2])
            minMax[1][2] = vertex.position.z;
    }

    std::array<glm::vec3, 8> points{};

    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                points[x + y * 2 + z * 4] = glm::vec3(minMax[x][0], minMax[y][1], minMax[z][2]);
            }
        }
    }

    return {allowCollision, points};
}

MeshTree::MeshTree(std::string tag, Mesh* msh, glm::vec3 off, glm::vec4 rots, glm::vec4 rotp, glm::vec3 scl, bool allowCollision) {
    this->mesh = new GPUMesh(*msh);
    this->transform = {off, rots, rotp, scl};
    // this->selfRotate= std::shared_ptr<glm::vec4>(&(transform.selfRotate));
    // this->rotateParent= std::shared_ptr<glm::vec4>(&(transform.rotateParent));
    // this->translate= std::shared_ptr<glm::vec3>(&(transform.translate));
    // this->scale= std::shared_ptr<glm::vec3>(&(transform.scale));
    this->hitBox = makeHitBox(*msh, allowCollision);
    
    this->tag = tag;
}

MeshTree::MeshTree(std::string tag){
    this->transform = {glm::vec3(0.f),
                       glm::vec4(0.f,1.f,0.f,0.f),
                       glm::vec4(0.f,1.f,0.f,0.f),
                       glm::vec3(1.f)};
    // this->selfRotate= std::shared_ptr<glm::vec4>(&(transform.selfRotate));
    // this->rotateParent= std::shared_ptr<glm::vec4>(&(transform.rotateParent));
    // this->translate= std::shared_ptr<glm::vec3>(&(transform.translate));
    // this->scale= std::shared_ptr<glm::vec3>(&(transform.scale));
    this->mesh = nullptr;
    
    this->tag = tag;
}

MeshTree::MeshTree(std::string tag, Mesh* msh, bool allowCollision) {
    this->transform = {glm::vec3(0.f),
                       glm::vec4(0.f,1.f,0.f,0.f),
                       glm::vec4(0.f,1.f,0.f,0.f),
                       glm::vec3(1.f)};
    this->mesh = new GPUMesh(*msh);
    // this->selfRotate= std::shared_ptr<glm::vec4>(&(transform.selfRotate));
    // this->rotateParent= std::shared_ptr<glm::vec4>(&(transform.rotateParent));
    // this->translate= std::shared_ptr<glm::vec3>(&(transform.translate));
    // this->scale= std::shared_ptr<glm::vec3>(&(transform.scale));
    this->hitBox = makeHitBox(*msh, allowCollision);
    
    this->tag = tag;
}

void MeshTree::addChild(std::shared_ptr<MeshTree> child){
    child.get()->parent = std::weak_ptr<MeshTree>(shared_from_this());
    this->children.push_back(child);
}

glm::mat4 MeshTree::modelMatrix() const {
    glm::mat4 currTransform;

    if (is_root)
        currTransform = glm::identity<glm::mat4>();
    else{
        if(parent.expired()){
            std::cerr << "WARNING! EXPIRED PARENT!!" << std::endl;
            return glm::identity<glm::mat4>();
        }
        std::shared_ptr parentPtr = parent.lock();
        currTransform = parentPtr.get()->modelMatrix();
    }

    // Translate
    

    // Rotate
    // glm::vec3 axis = glm::normalize(glm::vec3(transform.rotateParent.x, transform.rotateParent.y, transform.rotateParent.z));
    currTransform = glm::rotate(currTransform, glm::radians(transform.rotateParent.w), glm::vec3(transform.rotateParent.x, transform.rotateParent.y, transform.rotateParent.z));


    currTransform = glm::translate(currTransform, transform.translate);
    if(al != nullptr){
        al->position = currTransform*glm::vec4(0.f, 0.f, 0.f, 1.f);
        al->externalForward = glm::normalize(currTransform*glm::vec4(-1.f, 0.f, 0.f, 1.f));
    }
    currTransform = glm::rotate(currTransform, glm::radians(transform.selfRotate.w), glm::vec3(transform.selfRotate.x, transform.selfRotate.y, transform.selfRotate.z));


    
    
    // Scale
    return glm::scale(currTransform, transform.scale);
}

HitBox MeshTree::getTransformedHitBox() {
    glm::mat4 modelMatrix = this->modelMatrix();

    HitBox transformed = this->hitBox;
    for (glm::vec3& point : transformed.points) {
        point = modelMatrix * glm::vec4(point, 1);
    }
    return transformed;
}

glm::vec3 MeshTree::getTransformedHitBoxMiddle() {
    return modelMatrix() * glm::vec4(this->hitBox.getMiddle(), 1);
}

bool MeshTree::collide(MeshTree *other) {
    return (!this->hitBox.allowCollision || !other->hitBox.allowCollision)
            && this->getTransformedHitBox().collides(other->getTransformedHitBox());
}

static MeshTree* collidesWith(MeshTree* root, MeshTree* toCheck) {
    if (root->collide(toCheck))
        return root;

    for ( size_t i = 0; i < root->children.size(); i++) {
        std::weak_ptr<MeshTree> child = root->children.at(i);
        if(child.expired()){
            root->children.erase(root->children.begin()+i);
            i--;
            continue;
        }
        MeshTree* childp = child.lock().get();
        if (collidesWith(childp, toCheck) != nullptr)
            return childp;
    }

    return nullptr;
}

// Returns true if translation succeeded
bool MeshTree::tryTranslation(glm::vec3 translation, MeshTree* root) {
    this->transform.translate += translation;

    MeshTree* other = collidesWith(root, this);

    if (other == nullptr)
        return true;

    float newDist = glm::distance(this->getTransformedHitBoxMiddle(), other->getTransformedHitBoxMiddle());

    this->transform.translate -= translation;

    float oldDist = glm::distance(this->getTransformedHitBoxMiddle(), other->getTransformedHitBoxMiddle());

    if (newDist > oldDist) {
        this->transform.translate += translation;
        return true;
    }

    return false;
}

MeshTree::~MeshTree() { std::cout<<this->tag<<std::endl; }

void MeshTree::clean(LightManager& lmngr){
    for(size_t i = 0; i < children.size(); i++){
        if(!children[i].expired()){
            MeshTree* child = children[i].lock().get();
            MemoryManager::removeEl(child);
            
        }
    }
    if(al != nullptr){
        lmngr.removeByReference(al);
    }
}