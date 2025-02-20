/*
MIT License

Copyright (c) 2019 Ryan L. Guy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "meshobject.h"

#include "meshutils.h"

MeshObject::MeshObject() {
}

MeshObject::MeshObject(int i, int j, int k, double dx) :
        _isize(i), _jsize(j), _ksize(k), _dx(dx) {
}

MeshObject::~MeshObject() {
}

void MeshObject::getGridDimensions(int *i, int *j, int *k) { 
    *i = _isize; *j = _jsize; *k = _ksize; 
}

void MeshObject::updateMeshStatic(TriangleMesh meshCurrent) {
    _meshPrevious = meshCurrent;
    _meshCurrent = meshCurrent;
    _meshNext = meshCurrent;
    _vertexTranslationsCurrent = std::vector<vmath::vec3>(meshCurrent.vertices.size());
    _vertexTranslationsNext = std::vector<vmath::vec3>(meshCurrent.vertices.size());
    _isAnimated = false;
    _isChangingTopology = false;
}

void MeshObject::updateMeshAnimated(TriangleMesh meshPrevious, 
                                    TriangleMesh meshCurrent, 
                                    TriangleMesh meshNext) {
    _meshPrevious = meshPrevious;
    _meshCurrent = meshCurrent;
    _meshNext = meshNext;
    _isChangingTopology = false;

    _vertexTranslationsCurrent = std::vector<vmath::vec3>(meshCurrent.vertices.size());
    if (_meshPrevious.vertices.size() == _meshCurrent.vertices.size()) {
        for (size_t i = 0; i < meshCurrent.vertices.size(); i++) {
            _vertexTranslationsCurrent[i] = _meshCurrent.vertices[i] - _meshPrevious.vertices[i];
        }
    } else {
        _isChangingTopology = true;
    }

    _vertexTranslationsNext = std::vector<vmath::vec3>(meshNext.vertices.size());
    if (_meshNext.vertices.size() == _meshCurrent.vertices.size()) {
        for (size_t i = 0; i < meshCurrent.vertices.size(); i++) {
            _vertexTranslationsNext[i] = _meshNext.vertices[i] - _meshCurrent.vertices[i];
        }
    } else {
        _isChangingTopology = true;
    }

    _isAnimated = true;
}

void MeshObject::getCells(std::vector<GridIndex> &cells) {
        getCells(0.0f, cells);
}

void MeshObject::getCells(float frameInterpolation, std::vector<GridIndex> &cells) {
    if (_isInversed) {
        _getInversedCells(frameInterpolation, cells);
    }

    TriangleMesh m = getMesh(frameInterpolation);
    Array3d<bool> nodes(_isize + 1, _jsize + 1, _ksize + 1, false);
    MeshUtils::getGridNodesInsideTriangleMesh(m, _dx, nodes);

    Array3d<bool> cellGrid(_isize, _jsize, _ksize, false);
    GridIndex nodeCells[8];
    for (int k = 0; k < nodes.depth; k++) {
        for (int j = 0; j < nodes.height; j++) {
            for (int i = 0; i < nodes.width; i++) {
                if (!nodes(i, j, k)) {
                    continue;
                }

                Grid3d::getVertexGridIndexNeighbours(i, j, k, nodeCells);
                for (int nidx = 0; nidx < 8; nidx++) {
                    if (cellGrid.isIndexInRange(nodeCells[nidx])) {
                        cellGrid.set(nodeCells[nidx], true);
                    }
                }
            }
        }
    }

    for (int k = 0; k < _ksize; k++) {
        for (int j = 0; j < _jsize; j++) {
            for (int i = 0; i < _isize; i++) {
                if (cellGrid(i, j, k)) {
                    cells.push_back(GridIndex(i, j, k));
                }
            }
        }
    }

    cells.shrink_to_fit();
}

bool MeshObject::isAnimated() {
    return _isAnimated;
}

void MeshObject::clearObjectStatus() {
    _isObjectStateChanged = false;
}

TriangleMesh MeshObject::getMesh() {
    return _meshCurrent;
}

TriangleMesh MeshObject::getMesh(float frameInterpolation) {
    // TODO: update this method to improve/handle rigid, deformable, or
    // topology-changing meshes

    if (_isChangingTopology) {
        return getMesh();
    }

    frameInterpolation = fmax(0.0f, frameInterpolation);
    frameInterpolation = fmin(1.0f, frameInterpolation);

    TriangleMesh outmesh = _meshCurrent;
    for (size_t i = 0; i < _meshCurrent.vertices.size(); i++) {
        vmath::vec3 v1 = _meshCurrent.vertices[i];
        vmath::vec3 v2 = _meshNext.vertices[i];
        outmesh.vertices[i] = v1 + frameInterpolation * (v2 - v1);
    }

    return outmesh;
}

std::vector<vmath::vec3> MeshObject::getVertexTranslations() {
    return _vertexTranslationsCurrent;
}

std::vector<vmath::vec3> MeshObject::getVertexTranslations(float frameInterpolation) {
    if (_isChangingTopology) {
        return getVertexTranslations();
    }

    frameInterpolation = fmax(0.0f, frameInterpolation);
    frameInterpolation = fmin(1.0f, frameInterpolation);

    std::vector<vmath::vec3> transout(_vertexTranslationsCurrent.size(), vmath::vec3());
    for (size_t i = 0; i < _vertexTranslationsCurrent.size(); i++) {
        vmath::vec3 p1 = _vertexTranslationsCurrent[i];
        vmath::vec3 p2 = _vertexTranslationsNext[i];
        transout[i] = p1 + frameInterpolation * (p2 - p1);
    }

    return transout;
}

std::vector<vmath::vec3> MeshObject::getVertexVelocities(double dt) {
    return getVertexVelocities(dt, 0.0f);
}

std::vector<vmath::vec3> MeshObject::getVertexVelocities(double dt, float frameInterpolation) {
    std::vector<vmath::vec3> velocities = getVertexTranslations(frameInterpolation);

    double eps = 1e-10;
    if (dt < eps) {
        velocities = std::vector<vmath::vec3>(velocities.size());
        return velocities;
    }

    double invdt = 1.0 / dt;
    for (size_t i = 0; i < velocities.size(); i++) {
        velocities[i] *= invdt;
    }

    return velocities;
}

std::vector<vmath::vec3> MeshObject::getFrameVertexVelocities(int frameno, double dt) {
    std::vector<vmath::vec3> velocities = _vertexTranslationsCurrent;

    double eps = 1e-10;
    if (dt < eps) {
        velocities = std::vector<vmath::vec3>(velocities.size());
        return velocities;
    }

    double invdt = 1.0 / dt;
    for (size_t i = 0; i < velocities.size(); i++) {
        velocities[i] *= invdt;
    }

    return velocities;
}

void MeshObject::getMeshLevelSet(double dt, float frameInterpolation, int exactBand, 
                                 MeshLevelSet &levelset) {
    TriangleMesh m = getMesh(frameInterpolation);

    // Loose geometry will cause problems when splitting into mesh islands
    std::vector<int> removedVertices = m.removeExtraneousVertices();
    std::vector<vmath::vec3> vertexVelocities = getVertexVelocities(dt, frameInterpolation);
    for (int i = removedVertices.size() - 1; i >= 0; i--) {
        vertexVelocities.erase(vertexVelocities.begin() + removedVertices[i]);
    }

    std::vector<TriangleMesh> islands;
    std::vector<std::vector<vmath::vec3> > islandVertexVelocities;
    _getMeshIslands(m, vertexVelocities, levelset, islands, islandVertexVelocities);
    _expandMeshIslands(islands);

    if ((int)islands.size() < _numIslandsForFractureOptimizationTrigger) {
        _addMeshIslandsToLevelSet(islands, islandVertexVelocities, exactBand, levelset);
    } else {
        _addMeshIslandsToLevelSetFractureOptimization(
                                  islands, islandVertexVelocities, exactBand, levelset);
    }
}

void MeshObject::enable() {
    if (!_isEnabled) {
        _isObjectStateChanged = true;
    }
    _isEnabled = true;
}

void MeshObject::disable() {
    if (_isEnabled) {
        _isObjectStateChanged = true;
    }
    _isEnabled = false;
}

bool MeshObject::isEnabled() {
    return _isEnabled;
}

void MeshObject::inverse() {
    _isInversed = !_isInversed;
}

bool MeshObject::isInversed() {
    return _isInversed;
}

void MeshObject::setFriction(float f) {
    f = fmin(f, 1.0f);
    f = fmax(f, 0.0f);
    _friction = f;
}

float MeshObject::getFriction() {
    return _friction;
}

void MeshObject::setWhitewaterInfluence(float value) {
    value = fmax(value, 0.0f);
    _whitewaterInfluence = value;
}

float MeshObject::getWhitewaterInfluence() {
    return _whitewaterInfluence;
}

void MeshObject::setSheetingStrength(float value) {
    value = fmax(value, 0.0f);
    _sheetingStrength = value;
}

float MeshObject::getSheetingStrength() {
    return _sheetingStrength;
}

void MeshObject::setMeshExpansion(float ex) {
    _meshExpansion = ex;
}

float MeshObject::getMeshExpansion() {
    return _meshExpansion;
}

void MeshObject::enableAppendObjectVelocity() {
    _isAppendObjectVelocityEnabled = true;
}

void MeshObject::disableAppendObjectVelocity() {
     _isAppendObjectVelocityEnabled = false;
}

bool MeshObject::isAppendObjectVelocityEnabled() {
    return _isAppendObjectVelocityEnabled;
}

RigidBodyVelocity MeshObject::getRigidBodyVelocity(double framedt) {
    framedt = fmax(framedt, 1e-6);

    float vscale = _objectVelocityInfluence;
    float eps = 1e-5;
    RigidBodyVelocity rv;
    if (!_isAnimated || _isChangingTopology) {
        TriangleMesh m = getMesh();
        rv.centroid = m.getCentroid();
        rv.axis = vmath::vec3(1.0, 0.0, 0.0);
        return rv;
    }

    TriangleMesh m1 = _meshCurrent;
    TriangleMesh m2 = _meshNext;
    rv.centroid = m1.getCentroid();

    vmath::vec3 c1 = m1.getCentroid();
    vmath::vec3 c2 = m2.getCentroid();
    rv.linear = ((c2 - c1) / framedt) * vscale;

    vmath::vec3 vert1, vert2;
    bool referencePointFound = false;
    for (size_t i = 0; i < m1.vertices.size(); i++) {
        vert1 = m1.vertices[i];
        vert2 = m2.vertices[i];
        if (vmath::length(vert1 - rv.centroid) > eps && vmath::length(vert2 - rv.centroid)) {
            referencePointFound = true;
            break;
        }
    }

    if (!referencePointFound || vmath::length(vert1 - (vert2 - (c2 - c1))) < eps) {
        rv.axis = vmath::vec3(1.0, 0.0, 0.0);
        rv.angular = 0.0;
        return rv;
    }

    vmath::vec3 v1 = vert1 - rv.centroid;
    vmath::vec3 v2 = (vert2 - (c2 - c1)) - rv.centroid;
    if (vmath::length(v1) < eps || vmath::length(v2) < eps) {
        rv.axis = vmath::vec3(1.0, 0.0, 0.0);
        rv.angular = 0.0;
        return rv;
    }

    vmath::vec3 cross = vmath::cross(v1, v2);
    if (vmath::length(cross) < eps) {
        rv.axis = vmath::vec3(1.0, 0.0, 0.0);
        rv.angular = 0.0;
        return rv;
    }
    rv.axis = cross.normalize();

    v1 = v1.normalize();
    v2 = v2.normalize();
    double angle = acos(vmath::dot(v1, v2));
    rv.angular = (angle / framedt) * vscale;

    if (std::isinf(rv.axis.x) || std::isinf(rv.axis.y) || std::isinf(rv.axis.z) || std::isinf(rv.angular) || 
        std::isnan(rv.axis.x) || std::isnan(rv.axis.y) || std::isnan(rv.axis.z) || std::isnan(rv.angular)) {
        rv.axis = vmath::vec3(1.0, 0.0, 0.0);
        rv.angular = 0.0;
    }

    return rv;
}

void MeshObject::setObjectVelocityInfluence(float value) {
    _objectVelocityInfluence = value;
}

float MeshObject::getObjectVelocityInfluence() {
    return _objectVelocityInfluence;
}

MeshObjectStatus MeshObject::getStatus() {
    MeshObjectStatus s;
    s.isEnabled = isEnabled();
    s.isAnimated = isAnimated();
    s.isInversed = isInversed();
    s.isStateChanged = _isObjectStateChanged;
    s.isMeshChanged = _isMeshChanged();
    return s;
}

void MeshObject::_getInversedCells(float frameInterpolation, std::vector<GridIndex> &cells) {
    TriangleMesh m = getMesh(frameInterpolation);
    Array3d<bool> nodes(_isize + 1, _jsize + 1, _ksize + 1, false);
    MeshUtils::getGridNodesInsideTriangleMesh(m, _dx, nodes);

    Array3d<bool> cellGrid(_isize, _jsize, _ksize, false);
    GridIndex nodeCells[8];
    for (int k = 0; k < nodes.depth; k++) {
        for (int j = 0; j < nodes.height; j++) {
            for (int i = 0; i < nodes.width; i++) {
                if (nodes(i, j, k)) {
                    continue;
                }

                Grid3d::getVertexGridIndexNeighbours(i, j, k, nodeCells);
                for (int nidx = 0; nidx < 8; nidx++) {
                    if (cellGrid.isIndexInRange(nodeCells[nidx])) {
                        cellGrid.set(nodeCells[nidx], true);
                    }
                }
            }
        }
    }

    for (int k = 0; k < _ksize; k++) {
        for (int j = 0; j < _jsize; j++) {
            for (int i = 0; i < _isize; i++) {
                if (cellGrid(i, j, k)) {
                    cells.push_back(GridIndex(i, j, k));
                }
            }
        }
    }

    cells.shrink_to_fit();
}

void MeshObject::_getMeshIslands(TriangleMesh &m,
                                 std::vector<vmath::vec3> &vertexVelocities,
                                 MeshLevelSet &levelset, 
                                 std::vector<TriangleMesh> &islands,
                                 std::vector<std::vector<vmath::vec3> > &islandVertexVelocities) {

    std::vector<TriangleMesh> tempIslands;
    std::vector<std::vector<vmath::vec3> > tempIslandVertexVelocities;
    MeshUtils::splitIntoMeshIslands(m, vertexVelocities, tempIslands, tempIslandVertexVelocities);

    int isize, jsize, ksize;
    levelset.getGridDimensions(&isize, &jsize, &ksize);
    double dx = levelset.getCellSize();
    AABB gridAABB(0.0, 0.0, 0.0, isize * dx, jsize * dx, ksize * dx);

    for (size_t i = 0; i < tempIslands.size(); i++) {
        AABB meshAABB(tempIslands[i].vertices);
        vmath::vec3 minp = meshAABB.getMinPoint();
        vmath::vec3 maxp = meshAABB.getMaxPoint();

        if (gridAABB.isPointInside(minp) && gridAABB.isPointInside(maxp)) {
            islands.push_back(tempIslands[i]);
            islandVertexVelocities.push_back(tempIslandVertexVelocities[i]);
        } else {
            AABB inter = gridAABB.getIntersection(meshAABB);
            if (inter.width > 0.0 || inter.height > 0.0 || inter.depth > 0.0) {
                islands.push_back(tempIslands[i]);
                islandVertexVelocities.push_back(tempIslandVertexVelocities[i]);
            }
        }
    }
}


MeshLevelSet MeshObject::_getMeshIslandLevelSet(TriangleMesh &m, 
                                                std::vector<vmath::vec3> &velocities, 
                                                MeshLevelSet &domainLevelSet,
                                                int exactBand) {
    
    int isize, jsize, ksize;
    domainLevelSet.getGridDimensions(&isize, &jsize, &ksize);
    double dx = domainLevelSet.getCellSize();

    AABB islandAABB(m.vertices);
    GridIndex gmin = Grid3d::positionToGridIndex(islandAABB.getMinPoint(), dx);
    GridIndex gmax = Grid3d::positionToGridIndex(islandAABB.getMaxPoint(), dx);
    gmin.i = (int)fmax(gmin.i - exactBand, 0);
    gmin.j = (int)fmax(gmin.j - exactBand, 0);
    gmin.k = (int)fmax(gmin.k - exactBand, 0);
    gmax.i = (int)fmin(gmax.i + exactBand + 1, isize - 1);
    gmax.j = (int)fmin(gmax.j + exactBand + 1, jsize - 1);
    gmax.k = (int)fmin(gmax.k + exactBand + 1, ksize - 1);

    int gwidth = gmax.i - gmin.i;
    int gheight = gmax.j - gmin.j;
    int gdepth = gmax.k - gmin.k;

    MeshLevelSet islandLevelSet(gwidth, gheight, gdepth, dx, this);
    islandLevelSet.setGridOffset(gmin);
    islandLevelSet.fastCalculateSignedDistanceField(m, velocities, exactBand);

    return islandLevelSet;
}

void MeshObject::_expandMeshIslands(std::vector<TriangleMesh> &islands) {
    float eps = 1e-9f;
    if (fabs(_meshExpansion) < eps) {
        return;
    }

    for (size_t i = 0; i < islands.size(); i++) {
        _expandMeshIsland(islands[i]);
    }
}

void MeshObject::_expandMeshIsland(TriangleMesh &m) {
    if (m.vertices.empty()) {
        return;
    }

    vmath::vec3 vsum(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < m.vertices.size(); i++) {
        vsum += m.vertices[i];
    }

    vmath::vec3 centroid = vsum / (float)m.vertices.size();
    float expval = 0.5f * _meshExpansion;
    float eps = 1e-9f;
    for (size_t i = 0; i < m.vertices.size(); i++) {
        vmath::vec3 v = m.vertices[i] - centroid;
        if (fabs(v.x) < eps && fabs(v.y) < eps && fabs(v.z) < eps) {
            continue;
        }

        v = v.normalize();
        m.vertices[i] += expval * v;
    }
}

void MeshObject::_addMeshIslandsToLevelSet(std::vector<TriangleMesh> &islands,
                                           std::vector<std::vector<vmath::vec3> > &islandVertexVelocities,
                                           int exactBand,
                                           MeshLevelSet &levelset) {
    for (size_t i = 0; i < islands.size(); i++) {
        MeshLevelSet islandLevelSet = _getMeshIslandLevelSet(
                islands[i], islandVertexVelocities[i], levelset, exactBand
        );

        levelset.calculateUnion(islandLevelSet);
    }
}

void MeshObject::_addMeshIslandsToLevelSetFractureOptimization(
                                           std::vector<TriangleMesh> &islands,
                                           std::vector<std::vector<vmath::vec3> > &islandVertexVelocities,
                                           int exactBand,
                                           MeshLevelSet &levelset) {

    BoundedBuffer<MeshIslandWorkItem> workQueue(islands.size());
    for (size_t i = 0; i < islands.size(); i++) {
        MeshIslandWorkItem item(islands[i], islandVertexVelocities[i]);
        workQueue.push(item);
    }

    BoundedBuffer<MeshLevelSet*> finishedWorkQueue(_finishedWorkQueueSize);

    int numthreads = ThreadUtils::getMaxThreadCount();
    std::vector<std::thread> threads(numthreads);
    for (int i = 0; i < numthreads; i++) {
        threads[i] = std::thread(&MeshObject::_islandMeshLevelSetProducerThread, this,
                                 &workQueue, &finishedWorkQueue, &levelset, exactBand);
    }

    int numItemsProcessed = 0;
    while (numItemsProcessed < (int)islands.size()) {
        std::vector<MeshLevelSet*> finishedItems;
        finishedWorkQueue.popAll(finishedItems);

        for (size_t i = 0; i < finishedItems.size(); i++) {
            levelset.calculateUnion(*(finishedItems[i]));
            delete (finishedItems[i]);
        }
        numItemsProcessed += finishedItems.size();
    }

    workQueue.notifyFinished();
    for (size_t i = 0; i < threads.size(); i++) {
        workQueue.notifyFinished();
        threads[i].join();
    }
}

void MeshObject::_islandMeshLevelSetProducerThread(BoundedBuffer<MeshIslandWorkItem> *workQueue,
                                                   BoundedBuffer<MeshLevelSet*> *finishedWorkQueue,
                                                   MeshLevelSet *domainLevelSet,
                                                   int exactBand) {
    int isize, jsize, ksize;
    domainLevelSet->getGridDimensions(&isize, &jsize, &ksize);
    double dx = domainLevelSet->getCellSize();

    while (workQueue->size() > 0) {
        std::vector<MeshIslandWorkItem> items;
        int numItems = workQueue->pop(1, items);
        if (numItems == 0) {
            continue;
        }
        MeshIslandWorkItem w = items[0];

        AABB islandAABB(w.mesh.vertices);
        GridIndex gmin = Grid3d::positionToGridIndex(islandAABB.getMinPoint(), dx);
        GridIndex gmax = Grid3d::positionToGridIndex(islandAABB.getMaxPoint(), dx);
        gmin.i = (int)fmax(gmin.i - exactBand, 0);
        gmin.j = (int)fmax(gmin.j - exactBand, 0);
        gmin.k = (int)fmax(gmin.k - exactBand, 0);
        gmax.i = (int)fmin(gmax.i + exactBand + 1, isize - 1);
        gmax.j = (int)fmin(gmax.j + exactBand + 1, jsize - 1);
        gmax.k = (int)fmin(gmax.k + exactBand + 1, ksize - 1);

        int gwidth = gmax.i - gmin.i;
        int gheight = gmax.j - gmin.j;
        int gdepth = gmax.k - gmin.k;

        MeshLevelSet *islandLevelSet = new MeshLevelSet(gwidth, gheight, gdepth, dx, this);
        islandLevelSet->setGridOffset(gmin);
        islandLevelSet->disableMultiThreading();
        islandLevelSet->fastCalculateSignedDistanceField(w.mesh, w.vertexVelocities, exactBand);

        finishedWorkQueue->push(islandLevelSet);
    }
}

bool MeshObject::_isMeshChanged() {
    if (!isAnimated()) {
        return false;
    }

    if (_meshPrevious.vertices.size() != _meshCurrent.vertices.size()) {
        return true;
    }

    float eps = 1e-5;
    bool isMeshChanged = false;
    for (size_t i = 0; i < _meshPrevious.vertices.size(); i++) {
        if (vmath::length(_meshPrevious.vertices[i] - _meshCurrent.vertices[i]) > eps) {
            isMeshChanged = true;
            break;
        }
    }

    return isMeshChanged;
}