#include "CollisionTree.h"

void CollisionTree::freeTree(treeNode_t *node)
{
    if (!node)
        return;
    CollisionTree::freeTree(node->nxt1);
    CollisionTree::freeTree(node->nxt2);
    delete node->bBox;
    delete node;
}

boundingBox_t *CollisionTree::createBoundingBox(const boundingBox_t *bBox1, const boundingBox_t *bBox2)
{
    boundingBox_t *bBox = nullptr;

    if (!bBox1 || !bBox2)
        return (nullptr);
    bBox = new boundingBox_t;
    bBox->topRight.x = std::max(bBox1->topRight.x, bBox2->topRight.x);
    bBox->topRight.y = std::max(bBox1->topRight.y, bBox2->topRight.y);
    bBox->topRight.z = std::max(bBox1->topRight.z, bBox2->topRight.z);

    bBox->bottomLeft.x = std::min(bBox1->bottomLeft.x, bBox2->bottomLeft.x);
    bBox->bottomLeft.y = std::min(bBox1->bottomLeft.y, bBox2->bottomLeft.y);
    bBox->bottomLeft.z = std::min(bBox1->bottomLeft.z, bBox2->bottomLeft.z);

    bBox->bounds[0] = bBox->bottomLeft;
    bBox->bounds[1] = bBox->topRight;
    return (bBox);
}

boundingBox_t *CollisionTree::createBoundingBox(const face3_t &face, const std::vector<Vector3d> &vertices)
{
    boundingBox_t *bBox = new boundingBox_t;

    if (face.indexV[0] - 1 < 0 || (size_t)face.indexV[0] - 1 >= vertices.size() ||
            face.indexV[1] - 1 < 0 || (size_t)face.indexV[1] - 1 >= vertices.size() ||
            face.indexV[2] - 1 < 0 || (size_t)face.indexV[2] - 1 >= vertices.size())
        return (nullptr);

    bBox->topRight.x = std::max(vertices[face.indexV[0] - 1].x,
            std::max(vertices[face.indexV[1] - 1].x,
            vertices[face.indexV[2] - 1].x));
    bBox->topRight.y = std::max(vertices[face.indexV[0] - 1].y,
            std::max(vertices[face.indexV[1] - 1].y,
            vertices[face.indexV[2] - 1].y));
    bBox->topRight.z = std::max(vertices[face.indexV[0] - 1].z,
            std::max(vertices[face.indexV[1] - 1].z,
            vertices[face.indexV[2] - 1].z));

    bBox->bottomLeft.x = std::min(vertices[face.indexV[0] - 1].x,
            std::min(vertices[face.indexV[1] - 1].x,
            vertices[face.indexV[2] - 1].x));
    bBox->bottomLeft.y = std::min(vertices[face.indexV[0] - 1].y,
            std::min(vertices[face.indexV[1] - 1].y,
            vertices[face.indexV[2] - 1].y));
    bBox->bottomLeft.z = std::min(vertices[face.indexV[0] - 1].z,
            std::min(vertices[face.indexV[1] - 1].z,
            vertices[face.indexV[2] - 1].z));

    bBox->bounds[0] = bBox->bottomLeft;
    bBox->bounds[1] = bBox->topRight;
    return (bBox);
}

treeNode_t *CollisionTree::buildTreeRoots(const std::vector<treeNode_t *> &leafs)
{
    std::vector<treeNode_t *> upLeafs;
    treeNode_t *node = nullptr;

    if (leafs.size() == 1)
        return (leafs[0]);
    if (!leafs.size())
        return (nullptr);
    for (size_t i = 0; i < leafs.size(); i += 2)
    {
        if (i+1 < leafs.size())
        {
            node = new treeNode_t;
            node->nxt1 = leafs[i];
            node->nxt2 = leafs[i+1];
            node->bBox = CollisionTree::createBoundingBox(node->nxt1->bBox, node->nxt2->bBox);
        }
        else
            node = leafs[i];
        upLeafs.push_back(node);
    }
    return (CollisionTree::buildTreeRoots(upLeafs));
}

treeNode_t *CollisionTree::buildTreeLeafs(Obj3d *obj3d)
{
    std::vector<treeNode_t *> leafs;
    treeNode_t *node = nullptr;

    if (!obj3d) return (nullptr);
    for (size_t i = 0; i < obj3d->_faces3.size(); ++i)
    {
        node = new treeNode_t;
        node->obj3d = obj3d;
        node->faceIndex = i;
        node->bBox = CollisionTree::createBoundingBox(obj3d->_faces3[i], obj3d->_vertices->v);
        leafs.push_back(node);
    }
    return (CollisionTree::buildTreeRoots(leafs));
}

static std::mutex index_mutex;

void CollisionTree::buildMultiThreads(std::vector<Obj3d *> &obj3ds, std::vector<treeNode_t *> &trees, size_t *index)
{
    size_t i = 0;

    while (true)
    {
        index_mutex.lock();
        if (obj3ds[i]->_collisionTree)
            trees.push_back(static_cast<treeNode_t *>(obj3ds[i]->_collisionTree));
        if (*index >= obj3ds.size())
        {
            index_mutex.unlock();
            break;
        }
        i = *index;
        (*index)++;
        index_mutex.unlock();
        obj3ds[i]->_collisionTree = CollisionTree::buildTreeLeafs(obj3ds[i]);
    }
}

treeNode_t *CollisionTree::build(std::vector<Obj3d *> &obj3ds, int threads)
{
    std::vector<treeNode_t *> trees;
    std::vector<std::thread> build_threads;
    size_t index = 0;

    if (!obj3ds.size())
        return (nullptr);
    for (int i = 0; i < threads; ++i)
    {
        build_threads.emplace_back(std::thread(&CollisionTree::buildMultiThreads, std::ref(obj3ds), std::ref(trees), &index));
    }
    for (int i = 0; i < threads; ++i)
    {
        build_threads[i].join();
    }
    return (CollisionTree::buildTreeRoots(trees));
}
