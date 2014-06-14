#include <stdlib.h>
#include <stdio.h>
#include "scene.h"

typedef struct HPGmat4{
    float _11, _12, _13, _14,
	_21, _22, _23, _24,
	_31, _32, _33, _34,
	_41, _42, _43, _44;
} HPGmat4;

typedef enum {
    RIGHT, LEFT, TOP, BOTTOM, NEAR, FAR
} Faces;

static HPGvector cameraList, activeCameras, renderQueue, alphaQueue;
static HPGwindowSizeFun windowSizefun;

static HPGcamera currentCamera;

float *hpgCurrentCameraPosition(){
    return (float *) &currentCamera.position;
}

float *hpgCurrentCameraProjection(){
    return currentCamera.projection;
}

float *hpgCurrentCameraModelView(){
    return currentCamera.viewProjection;
}

float *hpgCurrentCameraModelViewProjection(){
    return currentCamera.modelViewProjection;
}

void hpgSetWindowSizeFun(HPGwindowSizeFun fun){ windowSizefun = fun; }

static void addToQueue(Node *node){
    HPGnode *n = (HPGnode *) node->data;
    if(!n->pipeline) return;
    if (n->pipeline->hasAlpha)
	hpgPush(&alphaQueue, n);
    else
	hpgPush(&renderQueue, n);
}

static void renderNode(HPGnode *node, HPGcamera *camera){
    float trans[16];
    hpgMultMat4(camera->viewProjection, node->transform, 
                camera->modelViewProjection);
    node->pipeline->render(node->data);
}

static void clearQueues(){
    renderQueue.size = 0;
    alphaQueue.size = 0;
}

static int xNegative(const void *a, const void *b){
    BoundingSphere *ba = ((HPGnode *) a)->partitionData.boundingSphere;
    BoundingSphere *bb = ((HPGnode *) b)->partitionData.boundingSphere;
    float xa = ba->x + ba->r;
    float xb = bb->x + bb->r;
    if (xa < xb) return 1;
    else if (xa > xb) return -1;
    return 0;
}
static int xPositive(const void *a, const void *b){
    BoundingSphere *ba = ((HPGnode *) a)->partitionData.boundingSphere;
    BoundingSphere *bb = ((HPGnode *) b)->partitionData.boundingSphere;
    float xa = ba->x + ba->r;
    float xb = bb->x + bb->r;
    if (xa > xb) return 1;
    else if (xa < xb) return -1;
    return 0;
}
static int yNegative(const void *a, const void *b){
    BoundingSphere *ba = ((HPGnode *) a)->partitionData.boundingSphere;
    BoundingSphere *bb = ((HPGnode *) b)->partitionData.boundingSphere;
    float ya = ba->y + ba->r;
    float yb = bb->y + bb->r;
    if (ya < yb) return 1;
    else if (ya > yb) return -1;
    return 0;
}
static int yPositive(const void *a, const void *b){
    BoundingSphere *ba = ((HPGnode *) a)->partitionData.boundingSphere;
    BoundingSphere *bb = ((HPGnode *) b)->partitionData.boundingSphere;
    float ya = ba->y + ba->r;
    float yb = bb->y + bb->r;
    if (ya > yb) return 1;
    else if (ya < yb) return -1;
    return 0;
}
static int zNegative(const void *a, const void *b){
    BoundingSphere *ba = ((HPGnode *) a)->partitionData.boundingSphere;
    BoundingSphere *bb = ((HPGnode *) b)->partitionData.boundingSphere;
    float za = ba->z + ba->r;
    float zb = bb->z + bb->r;
    if (za < zb) return 1;
    else if (za > zb) return -1;
    return 0;
}
static int zPositive(const void *a, const void *b){
    BoundingSphere *ba = ((HPGnode *) a)->partitionData.boundingSphere;
    BoundingSphere *bb = ((HPGnode *) b)->partitionData.boundingSphere;
    float za = ba->z + ba->r;
    float zb = bb->z + bb->r;
    if (za > zb) return 1;
    else if (za < zb) return -1;
    return 0;
}

static int xGreaterThan(const void *a, const void *b){
    float xa = ((HPGnode *) a)->partitionData.boundingSphere->x;
    float xb = ((HPGnode *) b)->partitionData.boundingSphere->x;
    if (xa < xb) return 1;
    else if (xa > xb) return -1;
    return 0;
}
static int xLessThan(const void *a, const void *b){
    float xa = ((HPGnode *) a)->partitionData.boundingSphere->x;
    float xb = ((HPGnode *) b)->partitionData.boundingSphere->x;
    if (xa > xb) return 1;
    else if (xa < xb) return -1;
    return 0;
}
static int yGreaterThan(const void *a, const void *b){
    float ya = ((HPGnode *) a)->partitionData.boundingSphere->y;
    float yb = ((HPGnode *) b)->partitionData.boundingSphere->y;
    if (ya < yb) return 1;
    else if (ya > yb) return -1;
    return 0;
}
static int yLessThan(const void *a, const void *b){
    float ya = ((HPGnode *) a)->partitionData.boundingSphere->y;
    float yb = ((HPGnode *) b)->partitionData.boundingSphere->y;
    if (ya > yb) return 1;
    else if (ya < yb) return -1;
    return 0;
}
static int zGreaterThan(const void *a, const void *b){
    float za = ((HPGnode *) a)->partitionData.boundingSphere->z;
    float zb = ((HPGnode *) b)->partitionData.boundingSphere->z;
    if (za < zb) return 1;
    else if (za > zb) return -1;
    return 0;
}
static int zLessThan(const void *a, const void *b){
    float za = ((HPGnode *) a)->partitionData.boundingSphere->z;
    float zb = ((HPGnode *) b)->partitionData.boundingSphere->z;
    if (za > zb) return 1;
    else if (za < zb) return -1;
    return 0;
}

static int programSort(const void *a, const void *b){
    unsigned int pa = (int) (((HPGnode *) a)->pipeline);
    unsigned int pb = (int) (((HPGnode *) b)->pipeline);
    if (pa < pb) return -1;
    else if (pa > pb) return 1;
    return 0;
}

static void setSortFuns(Plane *plane, 
                        int (*alphaSort)(const void*, const void*),
                        int (*renderSort)(const void*, const void*)){
    float aa, ab, ac;
    aa = abs(plane->a); 
    ab = abs(plane->b); 
    ac = abs(plane->c); 

    if ((aa > ab) && (aa > ac)){
        if (plane->a < 0.0) { alphaSort = xLessThan; renderSort = xNegative; } 
        else                { alphaSort = xGreaterThan; renderSort = xPositive; }
    } else if ((ab > ac)){
        if (plane->b < 0.0) { alphaSort = yLessThan; renderSort = yNegative; } 
        else                { alphaSort = yGreaterThan; renderSort = yPositive; }
    } else {
        if (plane->c < 0.0) { alphaSort = zLessThan; renderSort = zNegative; } 
        else                { alphaSort = zGreaterThan; renderSort = zPositive; }
    }
}

static void renderQueues(HPGcamera *camera){
    int i, j, count;
    struct pipeline *p = NULL;
    int (*alphaSort)(const void*, const void*);
    int (*renderSort)(const void*, const void*);
    setSortFuns(&camera->planes[NEAR], alphaSort, renderSort);
    qsort(renderQueue.data, alphaQueue.size, sizeof(void *), &programSort);
    qsort(renderQueue.data, renderQueue.size, sizeof(void *), &programSort);
    for (i = 0; i < alphaQueue.size;){
        HPGnode *n = (HPGnode *) hpgVectorValue(&alphaQueue, i);
        p = n->pipeline;
        p->preRender(n);
        for (count = 1; count < (alphaQueue.size - i);){
            HPGnode *m = (HPGnode *) hpgVectorValue(&alphaQueue, i + count);
            if (m->pipeline == p)
                count++;
            else
                qsort(alphaQueue.data, count, sizeof(void *), alphaSort);
        }
        for (j = 0; j < count; j++)
            renderNode((HPGnode *) hpgVectorValue(&alphaQueue, i + j), camera);
        p->postRender();
        i += count;
    }
    for (i = 0; i < renderQueue.size;){
        HPGnode *n = (HPGnode *) hpgVectorValue(&renderQueue, i);
        p = n->pipeline;
        p->preRender(n);
        for (count = 1; count < (renderQueue.size - i);){
            HPGnode *m = (HPGnode *) hpgVectorValue(&renderQueue, i + count);
            if (m->pipeline == p)
                count++;
            else
                qsort(renderQueue.data, count, sizeof(void *), renderSort);
        }
        for (j = 0; j < count; j++)
            renderNode((HPGnode *) hpgVectorValue(&renderQueue, i + j), camera);
        p->postRender();
        i += count;
    }
}

/*
http://web.archive.org/web/20120531231005/http://crazyjoke.free.fr/doc/3D/plane%20extraction.pdf
*/
/* Normals (pointing in) are (a, b, c) */
static void computePlanes(HPGcamera *camera){
    Plane *ps = camera->planes;
    HPGmat4 *m = (HPGmat4 *) camera->viewProjection;
    ps[LEFT].a   = m->_41 + m->_11; ps[LEFT].b   = m->_42 + m->_12;
    ps[LEFT].c   = m->_43 + m->_13; ps[LEFT].d   = m->_44 + m->_14;
    ps[RIGHT].a  = m->_41 - m->_11; ps[RIGHT].b  = m->_42 - m->_12;
    ps[RIGHT].c  = m->_43 - m->_13; ps[RIGHT].d  = m->_44 - m->_14;
    ps[BOTTOM].a = m->_41 + m->_21; ps[BOTTOM].b = m->_42 + m->_22;
    ps[BOTTOM].c = m->_43 + m->_23; ps[BOTTOM].d = m->_44 + m->_24;
    ps[TOP].a    = m->_41 - m->_21; ps[TOP].b    = m->_42 - m->_22;
    ps[TOP].c    = m->_43 - m->_23; ps[TOP].d    = m->_44 - m->_24;
    ps[NEAR].a   = m->_41 + m->_31; ps[NEAR].b   = m->_42 + m->_32;
    ps[NEAR].c   = m->_43 + m->_33; ps[NEAR].d   = m->_44 + m->_34;
    ps[FAR].a    = m->_41 - m->_31; ps[FAR].b    = m->_42 - m->_32;
    ps[FAR].c    = m->_43 - m->_33; ps[FAR].d    = m->_44 - m->_34;
}

void hpgRenderCamera(HPGcamera *camera){
    currentCamera = *camera;
    HPGcamera *c = &currentCamera;
    float cameraMat[16], view[16];
    clearQueues();
    if (c->isLookAt){
        hpgTranslateRotateScale(c->position.x, c->position.y, 
                                c->position.z,
                                c->up.x, c->up.y, c->up.z, c->angle,
                                1.0, cameraMat);
        hpgCameraInverse(cameraMat, view);
    } else {
        hpgLookAt(c->position.x, c->position.y, c->position.z,
                  c->object.x, c->object.y, c->object.z, 
                  c->up.x, c->up.y, c->up.z,
                  view);
    }
    hpgMultMat4(c->projection, view, c->viewProjection);
    computePlanes(c);
    camera->scene->partitionInterface->doVisible(c->scene->partitionStruct,
                                                 c->planes, &addToQueue);
    renderQueues(c);
    *camera = currentCamera;
}

static void hpgOrthoCamera(int width, int height, HPGcamera *camera){
    hpgOrtho(width, height, camera->n, camera->f, camera->projection);
}

static void hpgPerspectiveCamera(int width, int height, HPGcamera *camera){
    hpgPerspective(width, height, camera->n, camera->f, camera->viewAngle,
		   camera->projection);
}

HPGcamera *hpgMakeCamera(HPGcameraType type, HPGscene *scene){
    HPGcamera *camera = malloc(sizeof(struct camera));
    camera->n = HPG_DEFAULT_NEAR_PLANE;
    camera->f = HPG_DEFAULT_FAR_PLANE;
    camera->viewAngle = HPG_DEFAULT_VIEW_ANGLE;
    camera->position.x = 0.0;
    camera->position.y = 0.0;
    camera->position.z = 0.0;
    camera->up.x = 0.0;
    camera->up.y = 1.0;
    camera->up.z = 0.0;
    camera->object.x = 0.0;
    camera->object.y = 0.0;
    camera->object.z = 0.0;
    camera->angle = 0.0;
    if (type == HPG_ORTHO)
	camera->update = &hpgOrthoCamera;
    else
	camera->update = &hpgPerspectiveCamera;
    camera->isLookAt = false;
    camera->scene = scene;
    hpgPush(&cameraList, (void *) camera);
    hpgPush(&activeCameras, (void *) camera);
    
    int w, h;
    windowSizefun(&w, &h);
    camera->update(w, h, camera);
    return camera;
}

void hpgSetCameraClipPlanes(HPGcamera *camera, float near, float far){
    camera->n = near;
    camera->f = far;
    int w, h;
    windowSizefun(&w, &h);
    camera->update(w, h, camera);
}

void hpgSetCameraViewAngle(HPGcamera *camera, float angle){
    camera->viewAngle = angle;
    int w, h;
    windowSizefun(&w, &h);
    camera->update(w, h, camera);
}

void hpgDeleteCamera(HPGcamera *camera){
    hpgRemove(&cameraList, (void *) camera);
    free(camera);
}

void hpgMoveCamera(HPGcamera *camera, float x, float y, float z){
    camera->position.x += x;
    camera->position.y += y;
    camera->position.z += z;
}

void hpgSetCameraPosition(HPGcamera *camera, float x, float y, float z){
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = z;
}

void hpgRotateCamera(HPGcamera *camera, float angle){
    camera->isLookAt = false;
    camera->angle += angle;
}

void hpgSetCameraRotation(HPGcamera *camera, float x, float y, float z, float angle){
    camera->isLookAt = false;
    camera->up.x = x;
    camera->up.y = y;
    camera->up.z = z;
    camera->angle = angle;
}

void hpgCameraLookAt(HPGcamera *camera, float x, float y, float z){
    camera->isLookAt = true;
    camera->object.x = x;
    camera->object.y = y;
    camera->object.z = z;
}

void hpgResizeCameras(int width, int height){
    int i;
    for (i = 0; i < cameraList.size; i++){
	HPGcamera *camera = (HPGcamera *) hpgVectorValue(&cameraList, i);
	camera->update(width, height, camera);
    }
}

void hpgRenderCameras(){
    int i;
    for (i = 0; i < activeCameras.size; i++)
	hpgRenderCamera((HPGcamera *) hpgVectorValue(&activeCameras, i));
}

void hpgActiveateCamera(HPGcamera *c){
    hpgPush(&activeCameras, (void *) c);
}

void hpgDeactiveateCamera(HPGcamera *c){
    hpgRemove(&activeCameras, (void *) c);
}

void hpgInitCameras(){
    hpgInitVector(&cameraList, 16);
    hpgInitVector(&activeCameras, 16);
    hpgInitVector(&renderQueue, 4096);
    hpgInitVector(&alphaQueue, 1024);
}
