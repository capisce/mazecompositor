/*
 * Copyright (c) 2012 Samuel Rødal
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef VIEW_H
#define VIEW_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QPair>
#include <QPolygonF>
#include <QTime>
#include <QTouchEvent>
#include <QWindow>
#include <QVector>
#include <QVector3D>

#include "camera.h"
#include "map.h"

#include "qwaylandcompositor.h"
#include "qwaylandsurface.h"

class Entity;
class Light;
class SurfaceItem;
class QOpenGLFramebufferObject;

class QOpenGLWindow : public QWindow
{
public:
    QOpenGLWindow(const QRect &geometry);
    QOpenGLContext *context() const;

private:
    QOpenGLContext *m_context;
};

class View : public QOpenGLWindow, public QWaylandCompositor
{
    Q_OBJECT
public:
    View(const QRect &geometry);
    ~View();

public slots:
    void render();
    void onLongPress();

private slots:
    void surfaceDestroyed(QObject *surface);
    void surfaceDamaged(const QRect &rect);

protected:
    void surfaceCreated(QWaylandSurface *surface);

private:
    void resizeEvent(QResizeEvent *event);
    void exposeEvent(QExposeEvent *event);
    void generateScene();

    void render(const Camera &camera, const QRect &currentBounds, int zone = 0, int depth = 0);

    void updateDrag(const QPoint &pos);
    void handleTouchEvent(QTouchEvent *event);

#if 1
    void handleTouchBegin(QTouchEvent *event);
    void handleTouchUpdate(QTouchEvent *event);
    void handleTouchEnd(QTouchEvent *event);
    void handleCamera(QTouchEvent *event);
    void updateWalking(const QPoint &touch);
#endif
    bool event(QEvent *event);

    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);

    bool handleKey(int code, bool pressed);

    bool blocked(const QVector3D &pos) const;
    bool tryMove(QVector3D &pos, const QVector3D &delta) const;
    void move(Camera &camera, const QVector3D &pos);

    QRectF dockItemRect(int i) const;
    SurfaceItem *dockItemAt(const QPoint &pos);

    void startFocus();

    struct TraceResult {
        int zone;
        int tile;
        SurfaceItem *item;
        qreal u;
        qreal v;
        QVector3D pos;
    };

    enum TraceFlag {
        NoTraceFlag = 0,
        TraceKeepFocus = 1,
        TraceIgnoreSurfaces = 2
    };

    Q_DECLARE_FLAGS(TraceFlags, TraceFlag)

    TraceResult trace(const QPointF &pos, TraceFlags flags = NoTraceFlag);
    void resizeTo(const QVector2D &local);

    Camera portalize(const Camera &camera, int portal, bool clip = true) const;

    uint m_vertexAttr;
    uint m_normalAttr;
    uint m_textureAttr;
    uint m_matrixUniform;
    uint m_textureUniform;
    uint m_rustUniform;
    uint m_wallUniform;
    uint m_noiseUniform;
    uint m_eyeUniform;
    uint m_lightsUniform;
    uint m_numLightsUniform;

    uint m_eyeTextureId;
    uint m_arrowsTextureId;
    uint m_infoTextureId;
    uint m_rustId;
    uint m_textureId;
    uint m_ditherId[4];

    QOpenGLContext *m_context;
    QOpenGLFunctions m_gl;
    QOpenGLShaderProgram *m_program;

    Camera m_camera;

    QVector<QVector3D> m_normalBuffer;
    QVector<QVector3D> m_vertexBuffer;
    QVector<QVector2D> m_texCoordBuffer;
    QVector<ushort> m_indexBuffer;
    QVector<QPair<int, int> > m_indexBufferOffsets;

    qreal m_walkingVelocity;
    qreal m_strafingVelocity;
    qreal m_turningSpeed;
    qreal m_pitchSpeed;
    qreal m_targetYaw;
    qreal m_targetPitch;

    long m_simulationTime;
    long m_walkTime;

    bool m_jumping;
    qreal m_jumpVelocity;

    QTime m_time;

    typedef QHash<QWaylandSurface *, SurfaceItem *> SurfaceHash;
    SurfaceHash m_surfaces;

    QList<SurfaceItem *> m_mappedSurfaces;
    QList<SurfaceItem *> m_dockedSurfaces;

    Map m_map;
    QWaylandInputDevice *m_input;
    SurfaceItem *m_focus;
    QVector2D m_resizeGrip;
    QPolygonF m_portalPoly;
    QRectF m_portalRect;

    QOpenGLBuffer m_vertexData;
    QOpenGLBuffer m_indexData;

    SurfaceItem *m_dragItem;

    bool m_wireframe;
    bool m_mouseLook;
    bool m_mouseWalk;

    QPoint m_dragItemDelta;
    bool m_dragAccepted;

    int m_touchMoveId;
    int m_touchLookId;
    bool m_showInfo;
    bool m_pressingInfo;
    bool m_fullscreen;

    QPoint m_mousePos;

    QHash<int, bool> m_occupiedTiles;
    QTimer *m_focusTimer;
    QTimer *m_fullscreenTimer;
    QTimer *m_animationTimer;
    Entity *m_entity;
};

#endif
