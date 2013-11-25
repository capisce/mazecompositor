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

#include "view.h"

#include "common.h"
#include "entity.h"
#include "light.h"
#include "mesh.h"
#include "surfaceitem.h"

#include "qwaylandinput.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineF>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QTimer>

#include <qopengl.h>
#include <qmath.h>
#include <float.h>

static void frameRendered()
{
    if (!fpsDebug())
        return;

    static int frameCount = 0;
    static QTime lastTime = QTime::currentTime();

    ++frameCount;

    const QTime currentTime = QTime::currentTime();

    const int interval = 2500;

    const int delta = lastTime.msecsTo(currentTime);

    if (delta > interval) {
        qreal fps = 1000.0 * frameCount / delta;
        qDebug() << "FPS:" << fps;

        frameCount = 0;
        lastTime = currentTime;
    }
}

QSurfaceFormat createFormat()
{
    QSurfaceFormat format;
    format.setSamples(4);
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(1);
    return format;
}

QOpenGLWindow::QOpenGLWindow(const QRect &geometry)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setGeometry(geometry);
    setFormat(createFormat());
    setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    create();

    m_context = new QOpenGLContext(this);
    m_context->setFormat(createFormat());
    m_context->create();
}

QOpenGLContext *QOpenGLWindow::context() const
{
    return m_context;
}

View::View(const QRect &geometry)
    : QOpenGLWindow(geometry)
    , QWaylandCompositor(this)
    , m_walkingVelocity(0)
    , m_strafingVelocity(0)
    , m_turningSpeed(0)
    , m_pitchSpeed(0)
    , m_targetYaw(0)
    , m_targetPitch(0)
    , m_simulationTime(0)
    , m_walkTime(0)
    , m_jumping(false)
    , m_jumpVelocity(0)
    , m_map()
    , m_input(defaultInputDevice())
    , m_focus(0)
    , m_dragItem(0)
    , m_wireframe(false)
    , m_mouseLook(false)
    , m_mouseWalk(false)
    , m_dragAccepted(false)
    , m_touchMoveId(-1)
    , m_touchLookId(-1)
    , m_showInfo(false)
    , m_pressingInfo(false)
    , m_fullscreen(false)
    , m_entity(new Entity(this))
{
    m_camera.setPos(QVector3D(2.5, 0, 2.5));
    m_camera.setYaw(0.1);

    m_camera.setViewSize(size());

    m_context = context();
    m_context->makeCurrent(this);
    glViewport(0, 0, width(), height());

    generateScene();

    m_gl.initializeOpenGLFunctions();

    QByteArray vsrc =
        "attribute highp vec4 vertex;\n"
        "attribute highp vec3 normal;\n"
        "attribute highp vec2 texCoord;\n"
        "uniform mediump mat4 matrix;\n"
        "varying lowp vec3 n;\n"
        "varying highp vec3 p;\n"
        "varying mediump vec2 t;\n"
        "void main(void)\n"
        "{\n"
        "    p = vertex.xyz;\n"
        "    t = texCoord;\n"
        "    n = normalize(normal);\n"
        "    gl_Position = matrix * vertex;\n"
        "}\n";

    QByteArray fsrc =
        useSimpleShading() ?
            "uniform sampler2D texture;\n"
            "uniform int numLights;\n"
            "varying lowp vec3 n;\n"
            "varying highp vec3 p;\n"
            "uniform highp vec3 lights[NUM_LIGHTS];\n"
            "uniform highp vec3 eye;\n"
            "varying mediump vec2 t;\n"
            "varying highp float light;\n"
            "void main(void)\n"
            "{\n"
            "    lowp vec3 tex = texture2D(texture, t).rgb;\n"
            "    highp vec3 normal = normalize(n);\n"
            "    highp float diffuseCoeff = 0.0;\n"
            "    for (int i = 0; i < NUM_LIGHTS; ++i) {\n"
            "        highp vec3 toLight = lights[i] - p;\n"
            "        highp float toLightSqr = dot(toLight, toLight);\n"
            "        highp float lightDistanceInv = 1.0 / sqrt(toLightSqr);\n"
            "        highp vec3 toLightN = toLight * lightDistanceInv;\n"
            "        highp float normalDotLight = dot(toLightN, normal);\n"
            "        if (i < numLights)\n"
            "            diffuseCoeff += max(normalDotLight, 0.0) / max(1.5, toLightSqr);\n"
            "    }\n"
            "    gl_FragColor = vec4((1.0 * diffuseCoeff + 0.2) * tex, 1.0);\n"
            "}\n"
        :
            "uniform sampler2D texture;\n"
            "uniform int numLights;\n"
            "varying lowp vec3 n;\n"
            "varying highp vec3 p;\n"
            "uniform highp vec3 lights[NUM_LIGHTS];\n"
            "uniform highp vec3 eye;\n"
            "varying mediump vec2 t;\n"
            "varying highp float light;\n"
            "void main(void)\n"
            "{\n"
            "    lowp vec3 tex = texture2D(texture, t).rgb;\n"
            "    highp vec3 normal = normalize(n);\n"
            "    highp vec3 viewN = normalize(p - eye);\n"
            "    highp float specularFactor = pow(2.0, 10.0 * tex.r);\n"
            "    highp float specular = 0.0;\n"
            "    highp float diffuseCoeff = 0.0;\n"
            "    for (int i = 0; i < NUM_LIGHTS; ++i) {\n"
            "        highp vec3 toLight = lights[i] - p;\n"
            "        highp float toLightSqr = dot(toLight, toLight);\n"
            "        highp float lightDistanceInv = 1.0 / sqrt(toLightSqr);\n"
            "        highp vec3 toLightN = toLight * lightDistanceInv;\n"
            "        highp float normalDotLight = dot(toLightN, normal);\n"
            "        highp float reflectionDotView = max(0.0, dot(reflect(toLightN, normal), viewN));\n"
            "        highp float lightScale = min(1.0, lightDistanceInv);\n"
            "        if (i < numLights) {\n"
            "            diffuseCoeff += max(normalDotLight, 0.0) / max(1.5, toLightSqr);\n"
            "            specular += pow(reflectionDotView, specularFactor) * lightScale;\n"
            "        }\n"
            "    }\n"
            "    gl_FragColor = vec4((0.8 * diffuseCoeff + 0.2 + 0.6 * specular) * tex, 1.0);\n"
            "}\n";

    fsrc.replace("NUM_LIGHTS", QByteArray::number(m_map.maxLights()));

    m_program = generateShaderProgram(this, vsrc, fsrc);

    m_vertexAttr = m_program->attributeLocation("vertex");
    m_normalAttr = m_program->attributeLocation("normal");
    m_textureAttr = m_program->attributeLocation("texCoord");
    m_matrixUniform = m_program->uniformLocation("matrix");
    m_eyeUniform = m_program->uniformLocation("eye");
    m_lightsUniform = m_program->uniformLocation("lights");
    m_numLightsUniform = m_program->uniformLocation("numLights");

    QImage textureImage("boiler_plate.jpg");
    textureImage = textureImage.scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage rustImage("boiler_plate_rust.jpg");
    rustImage = rustImage.scaled(512, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage noiseImage("noise.jpg");
    noiseImage = noiseImage.scaled(512, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    noiseImage = noiseImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < noiseImage.height(); ++y) {
        QRgb *p= (QRgb *)noiseImage.scanLine(y);
        for (int x = 0; x < noiseImage.width(); ++x) {
            uint val = qRed(p[x]);
            p[x] = qRgba(val, val, val, val);
        }
    }

    QImage noisyRust(noiseImage);

    QPainter p(&noisyRust);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.drawImage(0, 0, rustImage);
    p.end();

    p.begin(&textureImage);
    p.setOpacity(0.4);
    p.drawImage(0, 0, noisyRust);
    p.end();

    m_textureId = generateTexture(textureImage);

    QPainterPath m;
    m.moveTo(-2.0,  0.0);
    m.lineTo(-1.5,  1.0);
    m.lineTo(-1.5,  0.5);
    m.lineTo(-0.5,  0.5);
    m.lineTo(-0.5,  1.5);
    m.lineTo(-1.0,  1.5);
    m.lineTo( 0.0,  2.0);
    m.lineTo( 1.0,  1.5);
    m.lineTo( 0.5,  1.5);
    m.lineTo( 0.5,  0.5);
    m.lineTo( 1.5,  0.5);
    m.lineTo( 1.5,  1.0);
    m.lineTo( 2.0,  0.0);
    m.lineTo( 1.5, -1.0);
    m.lineTo( 1.5, -0.5);
    m.lineTo( 0.5, -0.5);
    m.lineTo( 0.5, -1.5);
    m.lineTo( 1.0, -1.5);
    m.lineTo( 0.0, -2.0);
    m.lineTo(-1.0, -1.5);
    m.lineTo(-0.5, -1.5);
    m.lineTo(-0.5, -0.5);
    m.lineTo(-1.5, -0.5);
    m.lineTo(-1.5, -1.0);

    QImage arrowImage(256, 256, QImage::Format_ARGB32_Premultiplied);
    arrowImage.fill(0);

    p.begin(&arrowImage);
    p.translate(arrowImage.width() / 2, arrowImage.height() / 2);
    QTransform transform;
    transform.rotate(75, Qt::XAxis);
    p.setTransform(transform, true);
    p.rotate(5);
    p.scale(60, 60);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(m, Qt::white);
    p.end();

    QImage eyeImage(256, 256, QImage::Format_ARGB32_Premultiplied);
    eyeImage.fill(0);

    QPainterPath e;
    e.moveTo(-1.5, 0);
    e.quadTo(0, 1.1, 1.5, 0);
    e.quadTo(0, -1.1, -1.5, 0);
    e.addEllipse(-0.5, -0.5, 1, 1);

    p.begin(&eyeImage);
    p.translate(eyeImage.width() / 2, eyeImage.height() / 2);
    p.scale(70, 70);
    p.scale(1, -1);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(e, Qt::white);
    p.end();

    QImage infoImage(128, 128, QImage::Format_ARGB32_Premultiplied);
    infoImage.fill(0);

    QFont font;
    font.setFamily("Times");
    font.setItalic(true);

    QPainterPath i;
    i.addText(QPointF(), font, "i");
    QRectF r = i.boundingRect();

    i = i.translated(-r.center().x(), -r.center().y());
    i = QTransform::fromScale(1.0 / r.width(), 1.0 / r.height()).map(i);

    QPainterPath i2;
    i2.addEllipse(-1, -1, 2, 2);
    i2.addPath(i);

    p.begin(&infoImage);
    p.translate(infoImage.width() / 2, infoImage.height() / 2);
    p.scale(40, 40);
    p.fillPath(i2, Qt::white);
    p.end();

    m_eyeTextureId = generateTexture(eyeImage);
    m_arrowsTextureId = generateTexture(arrowImage);
    m_infoTextureId = generateTexture(infoImage);

    int offs[] = { 0, 2,
                   3, 1 };
    for (int i = 0; i < 4; ++i) {
        QImage ditherImage(2, 2, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                uint mask = offs[(i + 2 * y + x) % 4];
                mask |= mask << 16;
                mask |= mask << 8;
                ditherImage.setPixel(x, y, mask);
            }
        }

        m_ditherId[i] = generateTexture(ditherImage, false);

        glBindTexture(GL_TEXTURE_2D, m_ditherId[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    m_program->bind();

    GLint value;
    m_gl.glGetUniformiv(m_program->programId(), m_program->uniformLocation("texture"), &value);
    m_textureUniform = value;

    SurfaceItem::initialize(m_map, this);
    Light::initialize(this);
    m_entity->initialize();

    QPainterPath portalPath;
    portalPath.moveTo(-0.25, 0);
    portalPath.lineTo(-0.25, 0.6);
    portalPath.cubicTo(-0.25, 0.8, 0.25, 0.8, 0.25, 0.6);
    portalPath.lineTo(0.25, 0);
    portalPath.lineTo(-0.25, 0);

    QMatrix matrix;
    matrix.scale(300, 300);
    m_portalPoly = matrix.inverted().map(portalPath.toFillPolygon(matrix));
    m_portalRect = m_portalPoly.boundingRect();

    m_time.start();

    m_focusTimer = new QTimer(this);
    m_focusTimer->setSingleShot(true);
    m_focusTimer->setInterval(400);

    m_fullscreenTimer = new QTimer(this);
    m_fullscreenTimer->setSingleShot(true);
    m_fullscreenTimer->setInterval(400);

    connect(m_focusTimer, SIGNAL(timeout()), this, SLOT(onLongPress()));

    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(0);
    m_animationTimer->setSingleShot(true);
    m_animationTimer->start();
    connect(m_animationTimer, SIGNAL(timeout()), this, SLOT(render()));
}

View::~View()
{
}

void View::surfaceDestroyed(QObject *object)
{
    QWaylandSurface *surface = static_cast<QWaylandSurface *>(object);

    SurfaceHash::iterator it = m_surfaces.find(surface);
    if (it == m_surfaces.end())
        return;

    m_dockedSurfaces.removeOne(*it);
    m_mappedSurfaces.removeOne(*it);

    if (m_focus == *it) {
        m_fullscreen = false;
        m_focus = 0;
    }
    if (m_dragItem == *it)
        m_dragItem = 0;

    delete *it;
    m_surfaces.remove(surface);

    m_animationTimer->start();
}

void View::surfaceDamaged(const QRect &)
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    if (!m_surfaces.contains(surface)) {
        SurfaceItem *item = new SurfaceItem(surface);
        m_surfaces.insert(surface, item);

        connect(item, SIGNAL(opacityChanged()), m_animationTimer, SLOT(start()));

        m_dockedSurfaces << item;
    }

    m_animationTimer->start();
}

void View::surfaceCreated(QWaylandSurface *surface)
{
    connect(surface, SIGNAL(destroyed(QObject *)), this, SLOT(surfaceDestroyed(QObject *)));
    connect(surface, SIGNAL(damaged(const QRect &)), this, SLOT(surfaceDamaged(const QRect &)));
}

bool View::tryMove(QVector3D &pos, const QVector3D &delta) const
{
    QVector3D old = pos;

    if (delta.x() != 0 && !blocked(pos + QVector3D(delta.x(), 0, 0)))
        pos.setX(pos.x() + delta.x());

    if (delta.z() != 0 && !blocked(pos + QVector3D(0, 0, delta.z())))
        pos.setZ(pos.z() + delta.z());

    return pos != old;
}

static inline QRectF rectFromPoint(const QPointF &point, qreal size)
{
    return QRectF(point, point).adjusted(-size/2, -size/2, size/2, size/2);
}

bool View::blocked(const QVector3D &pos) const
{
    const QRectF rect = rectFromPoint(QPointF(pos.x(), pos.z()), 0.4);

    for (int y = 0; y < m_map.dimY(); ++y) {
        for (int x = 0; x < m_map.dimX(); ++x) {
            if (m_map(x, y))
                continue;

            QRectF r(x, y, 1, 1);

            if (r.intersects(rect))
                return true;
        }
    }

    return false;
}

Camera View::portalize(const Camera &camera, int portal, bool clip) const
{
    const Portal *portalA = m_map.portal(portal);
    const Portal *portalB = portalA->target();

    QVector3D portalUp = QVector3D(0, 1, 0);

    QVector3D portalRightA = QVector3D::crossProduct(portalUp, portalA->normal());
    QVector3D portalRightB = QVector3D::crossProduct(portalUp, portalB->normal());

    QVector3D deltaA = camera.pos() - portalA->pos();

    qreal dx = QVector3D::dotProduct(deltaA, portalRightA);
    qreal dy = -QVector3D::dotProduct(deltaA, portalUp);
    qreal dz = QVector3D::dotProduct(deltaA, portalA->normal());

    QVector3D deltaVA = camera.viewPos() - portalA->pos();

    qreal vdx = QVector3D::dotProduct(deltaVA, portalRightA);
    qreal vdy = -QVector3D::dotProduct(deltaVA, portalUp);
    qreal vdz = QVector3D::dotProduct(deltaVA, portalA->normal());

    qreal relativeScale = portalB->scale() / portalA->scale();

    QVector3D pos = portalB->pos() - (dx * portalRightB + dy * portalUp + dz * portalB->normal()) * relativeScale;
    QVector3D viewPos = portalB->pos() - (vdx * portalRightB + vdy * portalUp + vdz * portalB->normal()) * relativeScale;

    QLineF lineA(QPointF(), -QPointF(portalA->normal().x(), portalA->normal().z()));
    QLineF lineB(QPointF(), QPointF(portalB->normal().x(), portalB->normal().z()));

    Camera result = camera;
    result.setHeight((viewPos.y() - pos.y()) / (0.4 + camera.viewBob()));
    result.setPos(pos);
    result.setYaw(camera.yaw() + lineA.angleTo(lineB));

    if (clip) {
        QVector4D clip = result.viewMatrix().inverted().transposed().map(QVector4D(portalB->normal(), -QVector3D::dotProduct(portalB->pos(), portalB->normal())));
        result.setNearClipPlane(clip);
    }

    return result;
}

void View::move(Camera &camera, const QVector3D &pos)
{
    QVector3D old = camera.pos();
    QVector3D viewDir = camera.direction();

    for (int i = 0; i < m_map.numPortals(); ++i) {
        const Portal *portalA = m_map.portal(i);

        QVector3D portalUp = QVector3D(0, 1, 0);
        QVector3D portalRightA = QVector3D::crossProduct(portalUp, portalA->normal());

        qreal scale = portalA->scale();

        QVector3D portalEntry = portalA->pos() - portalA->normal() * 0.015 * scale * QVector3D::dotProduct(portalA->normal(), viewDir);

        if (QVector3D::dotProduct(old - portalEntry, portalA->normal()) >= 0
            && QVector3D::dotProduct(pos - portalEntry, portalA->normal()) <= 0)
        {
            qreal dist = QVector3D::dotProduct(pos - portalEntry, portalRightA);
            if (dist >= m_portalRect.left() * scale && dist <= m_portalRect.right() * scale) {
                // are we too big to fit?
                if (scale * m_portalRect.bottom() < camera.viewPos().y() * 1.2)
                    return;

                // through the rabbit hole we go
                camera.setPos(pos);
                camera = portalize(camera, i, false);
                m_targetYaw = camera.yaw();
                return;
            }
        }
    }

    camera.setPos(pos);
}

void View::render()
{
    m_context->makeCurrent(this);

    QSizeF viewport(width(), height());

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width(), height());

    glEnable(GL_STENCIL_TEST);
    glClearStencil(0);
    glStencilMask(~0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);

    if (m_fullscreen) {
        drawTexture(QRectF(0, 0, width(), height()), viewport, m_focus->textureId(), 1.0);

        m_context->swapBuffers(this);
        QWaylandCompositor::frameFinished(m_focus->surface());

        frameRendered();

        return;
    }

    int elapsed = m_time.elapsed();

    const int stepSize = 8;
    int steps = qMin(50L, (elapsed - m_simulationTime) / stepSize);

    for (int i = 0; i < steps; ++i) {
        m_targetYaw += m_turningSpeed;
        m_targetPitch += m_pitchSpeed;

        m_targetPitch = qBound(qreal(-30), m_targetPitch, qreal(30));

        if (m_targetYaw != m_camera.yaw())
            m_camera.setYaw(m_camera.yaw() + 0.25 * (m_targetYaw - m_camera.yaw()));
        if (m_targetPitch != m_camera.pitch())
            m_camera.setPitch(m_camera.pitch() + 0.25 * (m_targetPitch - m_camera.pitch()));

        bool walking = false;
        if (m_walkingVelocity != 0) {
            QPointF delta = QLineF::fromPolar(m_walkingVelocity, m_camera.yaw() - 90).p2();
            QVector3D walkingDelta(delta.x(), 0, delta.y());
            QVector3D pos = m_camera.pos();
            if (tryMove(pos, walkingDelta * m_camera.height())) {
                walking = true;
                move(m_camera, pos);
            }
        }

        if (m_strafingVelocity != 0) {
            QPointF delta = QLineF::fromPolar(m_strafingVelocity, m_camera.yaw()).p2();
            QVector3D walkingDelta(delta.x(), 0, delta.y());
            QVector3D pos = m_camera.pos();
            if (tryMove(pos, walkingDelta * m_camera.height())) {
                walking = true;
                move(m_camera, pos);
            }
        }

        if (m_camera.pos().y() > 0 || m_jumpVelocity > 0) {
            QVector3D pos = m_camera.pos();

            qreal targetY = qMax(0.0, pos.y() + m_camera.height() * m_jumpVelocity * stepSize * 0.001);
            qreal viewBob = m_camera.viewBob();

            if (targetY + viewBob >= 0.2) {
                targetY = 0.2 - viewBob;
                m_jumpVelocity = 0;
            }

            pos.setY(targetY);
            m_camera.setPos(pos);

            m_jumpVelocity -= 9.81 * stepSize * 0.001;

            walking = false;
            if ((m_walkTime + stepSize) * 0.001 < m_camera.bobResetTime())
                m_walkTime += stepSize;
        } else if (m_jumping) {
            m_jumpVelocity = 2;
        }

        if (walking)
            m_walkTime += stepSize;

        m_simulationTime += stepSize;
        m_entity->setWalking(walking);
    }

    m_camera.setTime(m_walkTime * 0.001);

    m_entity->setPosition(m_camera.pos());
    m_entity->setDirection(m_camera.direction());
    m_entity->setScale(m_camera.height());

    for (int i = 0; i < m_mappedSurfaces.size(); ++i)
        m_mappedSurfaces[i]->setDepthOffset(i * 0.0001);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glStencilFunc(GL_EQUAL, 0, ~0);
    glStencilMask(0);

    render(m_camera, QRect(0, 0, width(), height()), m_map.zone(m_camera.pos()));

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < m_dockedSurfaces.size(); ++i) {
        SurfaceItem *item = m_dockedSurfaces.at(i);
        if (item == m_dragItem) {
            if (!m_dragAccepted) {
                drawTexture(dockItemRect(i).translated(m_dragItemDelta), viewport, item->textureId(), 0.5);
            }
        } else {
            drawTexture(dockItemRect(i), viewport, item->textureId(), 0.5);
        }
    }

    if (m_showInfo) {
        drawTexture(QRectF(3 * width() / 4 - 64, 2 * height() / 3 - 64, 128, 128), viewport, m_eyeTextureId, m_touchLookId == -1 ? 0.5 : 0.8);
        drawTexture(QRectF(width() / 4 - 64, 2 * height() / 3 - 64, 128, 128), viewport, m_arrowsTextureId, m_touchMoveId == -1 ? 0.5 : 0.8);
    }

    drawTexture(QRectF(width() - 70, 10, 60, 60), viewport, m_infoTextureId, m_showInfo ? 0.8 : 0.5);

#if 0
    static int frame = 0;
    glBlendFunc(GL_ONE, GL_ONE);
    drawTexture(QRectF(0, 0, width(), height()), viewport, m_ditherId[frame % 4], 1.0, QRectF(0, 0, width() / 2, height() / 2));
    ++frame;
#endif

    glDisable(GL_BLEND);

    m_context->swapBuffers(this);
    QWaylandCompositor::frameFinished();

    frameRendered();
}

void split(const QRectF &rect, int depth, QRectF *left, QRectF *right)
{
    QPointF center = rect.center();

    if (depth & 1) {
        *left = QRectF(rect.topLeft(), QPointF(center.x(), rect.bottom()));
        *right = QRectF(QPointF(center.x(), rect.top()), rect.bottomRight());
    } else {
        *left = QRectF(rect.topLeft(), QPointF(rect.right(), center.y()));
        *right = QRectF(QPointF(rect.left(), center.y()), rect.bottomRight());
    }
}

bool visibleFrom(const Camera &camera, const QRectF &r)
{
    QVector2D tl(r.topLeft());
    QVector2D tr(r.topRight());
    QVector2D bl(r.bottomLeft());
    QVector2D br(r.bottomRight());

    QVector2D c(camera.pos().x(), camera.pos().z());
    QVector2D d(camera.direction().x(), camera.direction().z());

    c = c + d * camera.zNear();

#if 0
    return QVector2D::dotProduct(tl - c, d) > 0
        || QVector2D::dotProduct(tr - c, d) > 0
        || QVector2D::dotProduct(br - c, d) > 0
        || QVector2D::dotProduct(bl - c, d) > 0;
#else
    QVector<QVector3D> coordinates;
    coordinates << QVector3D(tl.x(), 0, tl.y());
    coordinates << QVector3D(tr.x(), 0, tr.y());
    coordinates << QVector3D(br.x(), 0, br.y());
    coordinates << QVector3D(bl.x(), 0, bl.y());

    coordinates << QVector3D(tl.x(), 1, tl.y());
    coordinates << QVector3D(tr.x(), 1, tr.y());
    coordinates << QVector3D(br.x(), 1, br.y());
    coordinates << QVector3D(bl.x(), 1, bl.y());

    return !camera.toScreenRect(coordinates).isNull();
#endif
}

void View::render(const Camera &camera, const QRect &currentBounds, int zone, int depth)
{
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

#ifndef QT_OPENGL_ES_2
    if (m_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

    m_program->bind();
    m_program->setUniformValue(m_matrixUniform, camera.viewProjectionMatrix());
    m_program->setUniformValue(m_eyeUniform, camera.viewPos());
    m_program->setUniformValueArray(m_lightsUniform, m_map.lights(zone).constData(), m_map.lights(zone).size());
    m_program->setUniformValue(m_numLightsUniform, m_map.lights(zone).size());

    glActiveTexture(GL_TEXTURE0 + m_textureUniform);
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    m_vertexData.bind();

    int stride = (3 + 3 + 2) * 4;
    m_program->enableAttributeArray(m_vertexAttr);
    m_program->setAttributeBuffer(m_vertexAttr, GL_FLOAT, 0, 3, stride);
    m_program->enableAttributeArray(m_normalAttr);
    m_program->setAttributeBuffer(m_normalAttr, GL_FLOAT, 3 * 4, 3, stride);
    m_program->enableAttributeArray(m_textureAttr);
    m_program->setAttributeBuffer(m_textureAttr, GL_FLOAT, (3 + 3) * 4, 2, stride);

    m_indexData.bind();
    int offset = m_indexBufferOffsets.at(zone).first;
    int size = m_indexBufferOffsets.at(zone).second;

    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_SHORT, reinterpret_cast<GLvoid *>(offset * 2));
    m_indexData.release();

    m_vertexData.release();

    m_program->disableAttributeArray(m_textureAttr);
    m_program->disableAttributeArray(m_normalAttr);
    m_program->disableAttributeArray(m_vertexAttr);

    for (int i = 0; i < m_mappedSurfaces.size(); ++i) {
        m_mappedSurfaces.at(i)->render(m_map, camera);
    }

    glCullFace(GL_FRONT);

    for (int i = 0; i < m_map.lights(zone).size(); ++i)
        Light(zone, i).render(m_map, camera);

#ifndef QT_OPENGL_ES_2
    if (m_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    if (depth < 3) {
        for (int i = 0; i < m_map.numPortals(); ++i) {
            const Portal *portalA = m_map.portal(i);
            const Portal *portalB = portalA->target();

            if (!portalB || m_map.zone(portalA->pos()) != zone)
                continue;

            QVector3D portalUp = QVector3D(0, 1, 0);

            QVector3D portalRightA = QVector3D::crossProduct(portalUp, portalA->normal());

            qreal dist = QVector3D::dotProduct(camera.pos() - portalA->pos(), portalA->normal());

            QVector3D portalEdgeLeft = camera.viewMatrix().map(portalA->pos() + portalRightA * m_portalRect.left());
            QVector3D portalEdgeRight = camera.viewMatrix().map(portalA->pos() + portalRightA * m_portalRect.right());

            if (dist > 0 &&
                (QVector4D::dotProduct(camera.nearClipPlane(), QVector4D(portalEdgeLeft, 1)) > camera.zNear()
                 || QVector4D::dotProduct(camera.nearClipPlane(), QVector4D(portalEdgeRight, 1)) > camera.zNear()))
            {
                qreal scale = portalA->scale();

                QVector<QVector3D> portal;
                for (int j = 0; j < m_portalPoly.size(); ++j) {
                    portal << QVector3D(portalA->pos().x(), scale * m_portalPoly.at(j).y(), portalA->pos().z()) - scale * m_portalPoly.at(j).x() * portalRightA;
                }

                QRect newBounds = camera.toScreenRect(portal).toAlignedRect() & currentBounds;

                if (newBounds.isNull())
                    continue;

                QRect oldScissor = QRectF(currentBounds.x(), height() - (currentBounds.y() + currentBounds.height()), currentBounds.width(), currentBounds.height()).toAlignedRect();
                QRect newScissor = QRectF(newBounds.x(), height() - (newBounds.y() + newBounds.height()), newBounds.width(), newBounds.height()).toAlignedRect();

                glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
                glStencilMask(~0);

                glColorMask(false, false, false, false);
                drawConvexSolid(camera, portal, Qt::red);

                glStencilMask(0);

                glScissor(newScissor.x(), newScissor.y(), newScissor.width(), newScissor.height());
                glStencilFunc(GL_EQUAL, depth + 1, ~0);

                glDepthFunc(GL_ALWAYS);
                drawRect(QRectF(0, 0, width(), height()), QSizeF(width(), height()), Qt::black, 1.0);
                glColorMask(true, true, true, true);
                glDepthFunc(GL_LEQUAL);

                render(portalize(camera, i), newBounds, m_map.zone(portalB->pos()), depth + 1);

                glStencilFunc(GL_EQUAL, depth + 1, ~0);
                glStencilOp(GL_KEEP, GL_DECR, GL_DECR);
                glStencilMask(~0);

                glDepthFunc(GL_ALWAYS);
                glColorMask(false, false, false, false);
                drawConvexSolid(camera, portal, Qt::red);
                glColorMask(true, true, true, true);
                glDepthFunc(GL_LEQUAL);

                glScissor(oldScissor.x(), oldScissor.y(), oldScissor.width(), oldScissor.height());
                glStencilFunc(GL_EQUAL, depth, ~0);
            }
        }
    }

    if (m_camera.pos() != camera.pos() && zone == m_map.zone(m_camera.pos())) {
        glDisable(GL_CULL_FACE);
        glDepthMask(false);
        m_entity->updateTransform(camera);
        m_entity->render(m_map, camera);
        glDepthMask(true);
        glEnable(GL_CULL_FACE);
    }
}

void View::exposeEvent(QExposeEvent *)
{
    m_animationTimer->start();
}

void View::resizeEvent(QResizeEvent *)
{
    glViewport(0, 0, width(), height());
    m_camera.setViewSize(size());
    m_animationTimer->start();
}

void View::generateScene()
{
    for (int i = 0; i < m_map.numZones(); ++i) {
        Mesh mesh;

        foreach (const QVector<QVector3D> &tile, m_map.tiles(i))
            mesh.addFace(tile);

        mesh.verify();
        mesh.borderize(0.25);
//        mesh.catmullClarkSubdivide();
        mesh.catmullClarkSubdivide();

        int vertexOffset = m_vertexBuffer.size();
        int indexOffset = m_indexBuffer.size();

        QHash<int, int> replacement;

        QVector<QVector3D> meshVertexBuffer = mesh.vertexBuffer();
        QVector<QVector3D> meshNormalBuffer = mesh.normalBuffer();
        QVector<QVector2D> meshTexCoordBuffer(meshVertexBuffer.size());

        QVector<uint> meshIndexBuffer = mesh.indexBuffer();
        for (int turn = 0; turn < 2; ++turn) {
            for (int i = 0; i < meshIndexBuffer.size(); i += 3) {
                int index[3];
                for (int j = 0; j < 3; ++j)
                    index[j] = meshIndexBuffer.at(i + j);

                QVector3D v1 = mesh.vertexBuffer().at(index[0]);
                QVector3D v2 = mesh.vertexBuffer().at(index[1]);
                QVector3D v3 = mesh.vertexBuffer().at(index[2]);

                QVector3D n = QVector3D::crossProduct(v2 - v1, v3 - v1).normalized();
                if (qAbs(n.y()) <= 0.5) { // wall ?
                    if (turn == 1)
                        continue;
                    for (int j = 0; j < 3; ++j) {
                        replacement[index[j]] = index[j];
                        m_indexBuffer << index[j];
                        QVector3D v = meshVertexBuffer.at(index[j]);
                        meshTexCoordBuffer[index[j]] = QVector2D(v.x() + v.z(), v.y()) * 8;
                    }
                } else {
                    if (turn == 0)
                        continue;
                    for (int j = 0; j < 3; ++j) {
                        QVector3D v = meshVertexBuffer.at(index[j]);
                        if (replacement.value(index[j]) != 0) {
                            if (replacement.value(index[j]) == index[j]) {
                                replacement[index[j]] = meshVertexBuffer.size();
                                meshVertexBuffer << v;
                                meshNormalBuffer << meshNormalBuffer.at(index[j]);
                                meshTexCoordBuffer << QVector2D();
                            }
                            index[j] = replacement.value(index[j]);
                        }
                        meshTexCoordBuffer[index[j]] = QVector2D(v.x(), v.z()) * 8;
                        m_indexBuffer << index[j];
                    }
                }
            }
        }

        m_normalBuffer << meshNormalBuffer;
        m_vertexBuffer << meshVertexBuffer;
        m_texCoordBuffer << meshTexCoordBuffer;

        QVector<QVector2D> texCoordBuffer(mesh.vertexBuffer().size());

        for (int i = indexOffset; i < m_indexBuffer.size(); ++i)
            m_indexBuffer[i] += vertexOffset;

        m_indexBufferOffsets << qMakePair(indexOffset, m_indexBuffer.size() - indexOffset);
    }

    QVector<float> interleaved;
    for (int i = 0; i < m_vertexBuffer.size(); ++i) {
        interleaved << m_vertexBuffer.at(i).x();
        interleaved << m_vertexBuffer.at(i).y();
        interleaved << m_vertexBuffer.at(i).z();
        interleaved << m_normalBuffer.at(i).x();
        interleaved << m_normalBuffer.at(i).y();
        interleaved << m_normalBuffer.at(i).z();
        interleaved << m_texCoordBuffer.at(i).x();
        interleaved << m_texCoordBuffer.at(i).y();
    }

    int totalSize = interleaved.size() * 4;

    m_vertexData = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_vertexData.create();
    m_vertexData.bind();
    m_vertexData.allocate(totalSize);
    m_vertexData.write(0, interleaved.constData(), totalSize);
    m_vertexData.release();

    m_indexData = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    m_indexData.create();
    m_indexData.bind();
    m_indexData.allocate(2 * m_indexBuffer.size());
    m_indexData.write(0, m_indexBuffer.constData(), 2 * m_indexBuffer.size());
    m_indexData.release();

    printf("Vertex count: %d\n", m_vertexBuffer.size());
    printf("Map triangle count: %d\n", m_indexBuffer.size() / 3);
}

void View::resizeTo(const QVector2D &local)
{
    Q_ASSERT(m_focus);

    QSize sz = m_focus->surface()->size();

    QVector2D size = QVector2D(sz.width(), sz.height());
    QVector2D center = size / 2;

    qreal currentHeight = m_focus->height();
    qreal desiredGrowth = (local - center).y() / (m_resizeGrip - center).y();

    qreal desiredHeight = currentHeight * desiredGrowth;

    m_focus->setHeight(desiredHeight);
    m_animationTimer->start();
}

QRectF View::dockItemRect(int i) const
{
    SurfaceItem *item = m_dockedSurfaces.at(i);

    qreal rw = width() / 16.;

    qreal w;
    qreal h;

    QSize size = item->size();
    if (size.width() > size.height()) {
        w = rw;
        h = size.height() * w / size.width();
    } else {
        h = rw;
        w = size.width() * h / size.width();
    }

    qreal x = 6 + (i + 0.5) * rw - w * 0.5;
    qreal y = 40 - h * 0.5;

    return QRectF(x, y, w, h);
}

SurfaceItem *View::dockItemAt(const QPoint &pos)
{
    for (int i = 0; i < m_dockedSurfaces.size(); ++i) {
        QRectF r = dockItemRect(i);
        if (r.contains(pos))
            return m_dockedSurfaces.at(i);
    }
    return 0;
}

bool View::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        handleTouchEvent((QTouchEvent *)event);
        break;
    default:
        return QWindow::event(event);
    }
    return true;
}

void View::handleTouchEvent(QTouchEvent *event)
{
    if (event->type() == QEvent::TouchBegin)
        handleTouchBegin(event);
    if (event->type() == QEvent::TouchUpdate)
        handleTouchUpdate(event);
    if (event->type() == QEvent::TouchEnd)
        handleTouchEnd(event);
#if 0
    if (event->type() == QEvent::TouchBegin)
        printf("TouchBegin: ");
    if (event->type() == QEvent::TouchUpdate)
        printf("TouchUpdate: ");
    if (event->type() == QEvent::TouchEnd)
        printf("TouchEnd: ");
    QList<QTouchEvent::TouchPoint> points = event->touchPoints();
    printf("points: %d, ", points.size());
    for (int i = 0; i < points.size(); ++i) {
        printf("(id %d, %fx %fy) ", points.at(i).id(), points.at(i).pos().x(), points.at(i).pos().y());
    }
    printf("\n");
#endif
}

void View::onLongPress()
{
    m_dragItem = m_focus;
}

void View::handleTouchBegin(QTouchEvent *event)
{
    QPoint primaryPos = event->touchPoints().at(0).pos().toPoint();
    if (m_fullscreen) {
        QPointF relative(primaryPos.x() * m_focus->surface()->size().width() / qreal(width()), primaryPos.y() * m_focus->surface()->size().height() / qreal(height()));
        m_input->sendMousePressEvent(Qt::LeftButton, relative);
        return;
    }

    QRect infoRect(width() - 70, 10, 60, 60);

    SurfaceItem *oldFocus = m_focus;

    m_focus = 0;

    TraceResult result;
    if (infoRect.contains(primaryPos)) {
        m_pressingInfo = true;
    } else {
        m_dragItem = dockItemAt(primaryPos);
        if (m_dragItem) {
            m_mousePos = primaryPos;
            m_dragItemDelta = QPoint();
            m_dragAccepted = false;
            m_animationTimer->start();
            return;
        }

        result = trace(primaryPos);
        m_focus = result.item;
    }

    if (oldFocus && m_focus != oldFocus)
        oldFocus->setFocus(false);

    if (m_focus) {
        QRect rect = QRect(m_focus->surface()->pos().toPoint(), m_focus->surface()->size());

        QVector2D size = QVector2D(rect.bottomRight());
        QVector2D local = QVector2D(result.u, result.v) * size;

        qreal gripDistanceSquared = 400;

        if ((QVector2D(rect.topLeft()) - local).lengthSquared() < gripDistanceSquared
            || (QVector2D(rect.topRight()) - local).lengthSquared() < gripDistanceSquared
            || (QVector2D(rect.bottomRight()) - local).lengthSquared() < gripDistanceSquared
            || (QVector2D(rect.bottomLeft()) - local).lengthSquared() < gripDistanceSquared)
        {
            m_resizeGrip = local;
        } else if (m_focus != oldFocus) {
            int last = m_dockedSurfaces.size();
            m_dockedSurfaces << m_focus;
            m_mousePos = dockItemRect(last).center().toPoint();
            m_dockedSurfaces.removeAt(last);
            m_focusTimer->start();
        } else {
            m_mousePos = local.toPoint();
            startFocus();
            m_input->sendMousePressEvent(Qt::LeftButton, m_mousePos);
        }
    } else if (!m_pressingInfo) {
        if (m_animationTimer->isSingleShot()) {
            m_simulationTime = m_time.elapsed();
            m_animationTimer->setSingleShot(false);
            m_animationTimer->start();
        }

        m_mouseLook = true;
        handleCamera(event);
    }
}

void View::startFocus()
{
    m_focusTimer->stop();
    m_input->setKeyboardFocus(m_focus->surface());
    m_input->setMouseFocus(m_focus->surface(), QPoint());

    m_focus->setFocus(true);

    m_mappedSurfaces.removeOne(m_focus);
    m_mappedSurfaces << m_focus;
}

void View::updateWalking(const QPoint &touch)
{
    QVector2D delta = QVector2D(touch) - QVector2D(width() / 4, 2 * height() / 3);
    qreal length = delta.length();
    delta = delta.normalized();

    qreal scale = 1.0 - qExp(-length * length * 0.0005);

    if (scale < 0.4)
        scale = 0;

    delta *= scale;

    m_walkingVelocity = -0.005 * delta.y();
    m_strafingVelocity = -0.005 * delta.x();
}

void View::handleCamera(QTouchEvent *event)
{
    QList<QTouchEvent::TouchPoint> points = event->touchPoints();
    int touchCount = points.size();
    for (int i = 0; i < touchCount; ++i) {
        int id = points.at(i).id();

        QPoint pos = points.at(i).pos().toPoint();
        Qt::TouchPointState state = points.at(i).state();
        if (state == Qt::TouchPointPressed) {
            if (m_touchMoveId < 0 && (pos.x() < width() / 2 || m_touchLookId >= 0)) {
                m_touchMoveId = id;
                m_mouseWalk = true;
                updateWalking(pos);
            } else if (m_touchLookId < 0 && (pos.x() > width() / 2 || m_touchMoveId >= 0)) {
                m_touchLookId = id;
                m_mousePos = pos;
            }
        } else if (state == Qt::TouchPointMoved) {
            if (id == m_touchMoveId) {
                updateWalking(pos);
            } else if (id == m_touchLookId) {
                QPoint delta = m_mousePos - pos;
                m_mousePos = pos;

                m_targetYaw += delta.x() * 0.4;
                m_targetPitch -= delta.y() * 0.4;
            }
        } else if (state == Qt::TouchPointReleased) {
            if (id == m_touchMoveId) {
                m_touchMoveId = -1;
                m_mouseWalk = false;
                m_walkingVelocity = 0;
                m_strafingVelocity = 0;
            } else if (id == m_touchLookId) {
                m_touchLookId = -1;
            }
        }
    }

    if (event->type() == QEvent::TouchEnd) {
        m_touchLookId = -1;
        m_touchMoveId = -1;
        m_mouseWalk = false;
        m_walkingVelocity = 0;
        m_strafingVelocity = 0;
    }
}

void View::updateDrag(const QPoint &pos)
{
    TraceResult result = trace(pos, TraceIgnoreSurfaces);

    const QVector<QVector3D> &tile = m_map.tiles(result.zone).at(result.tile);

    QVector3D tileDeltaU = tile.at(1) - tile.at(0);
    QVector3D tileDeltaV = tile.at(3) - tile.at(0);

    QVector3D tileCenter = (tile.at(2) + tile.at(0)) * 0.5;
    QVector3D tileNormal =  -QVector3D::crossProduct(tile.at(1) - tile.at(0),
            tile.at(2) - tile.at(1)).normalized();

    m_dragAccepted = qAbs(tileNormal.y()) < 1e-4 && !m_map.occupied(result.pos + 0.5 * tileNormal);

    if (m_dragAccepted) {
        m_dockedSurfaces.removeOne(m_dragItem);
        if (m_mappedSurfaces.indexOf(m_dragItem) == -1)
            m_mappedSurfaces << m_dragItem;

        // snap to grid

        result.u = qFloor(result.u * 20) / 20.;
        result.v = qFloor(result.v * 20) / 20.;

        m_dragItem->setPos(tileCenter + tileNormal * 0.04 + tileDeltaU * (result.u - 0.5) + tileDeltaV * (result.v - 0.5));
        m_dragItem->setNormal(tileNormal);
    } else {
        m_mappedSurfaces.removeOne(m_dragItem);
        if (m_dockedSurfaces.indexOf(m_dragItem) == -1)
            m_dockedSurfaces << m_dragItem;
    }

    m_animationTimer->start();
}

void View::handleTouchEnd(QTouchEvent *event)
{
    QPoint primaryPos = event->touchPoints().at(0).pos().toPoint();

    if (m_fullscreen) {
        QPointF relative(primaryPos.x() * m_focus->surface()->size().width() / qreal(width()), primaryPos.y() * m_focus->surface()->size().height() / qreal(height()));
        m_input->sendMouseReleaseEvent(Qt::LeftButton, relative);

        if (QRect(0, 0, 2, 2).contains(primaryPos)) {
            m_fullscreen = false;
            m_animationTimer->start();
        }

        return;
    }

    if (m_pressingInfo) {
        QRect infoRect(width() - 70, 10, 60, 60);
        if (infoRect.contains(primaryPos)) {
            m_showInfo = !m_showInfo;
            m_animationTimer->start();
        }
        m_pressingInfo = false;
        return;
    }

    bool active = !qFuzzyIsNull(m_strafingVelocity) || !qFuzzyIsNull(m_walkingVelocity) || !qFuzzyIsNull(m_pitchSpeed) || !qFuzzyIsNull(m_turningSpeed) || m_jumping;
    if (!m_animationTimer->isSingleShot() && !active)
        m_animationTimer->setSingleShot(true);

    if (m_dragItem) {
        updateDrag(primaryPos);
        m_dragItem = 0;
        m_focus = 0;
        return;
    }

    if (!m_focus) {
        if (m_mouseLook) {
            handleCamera(event);
            m_mouseLook = false;
        }
        return;
    }

    TraceResult result = trace(primaryPos, TraceKeepFocus);
    Q_ASSERT(result.item == m_focus);

    QRect rect = QRect(m_focus->surface()->pos().toPoint(), m_focus->surface()->size());
    QVector2D size = QVector2D(rect.bottomRight());
    QVector2D local = QVector2D(result.u, result.v) * size;

    m_mousePos = local.toPoint();

    if (!m_resizeGrip.isNull()) {
        resizeTo(local);
        m_resizeGrip = QVector2D();
        return;
    }

    if (m_focusTimer->isActive()) {
        startFocus();
	m_fullscreenTimer->start();
    } else if (0 && m_focus && m_fullscreenTimer->isActive()) {
        m_fullscreen = true;
        m_animationTimer->setSingleShot(true);
        m_animationTimer->start();
    } else {
        m_input->sendMouseReleaseEvent(Qt::LeftButton, m_mousePos);
    }
}

void View::handleTouchUpdate(QTouchEvent *event)
{
    if (m_pressingInfo)
        return;

    QPoint primaryPos = event->touchPoints().at(0).pos().toPoint();

    if (m_fullscreen) {
        QPointF relative(primaryPos.x() * m_focus->surface()->size().width() / qreal(width()), primaryPos.y() * m_focus->surface()->size().height() / qreal(height()));
        m_input->sendMouseMoveEvent(relative);
        return;
    }

    if (m_dragItem) {
        m_dragItemDelta = primaryPos - m_mousePos;
        updateDrag(primaryPos);
    }

    if (m_mouseLook) {
        handleCamera(event);
        return;
    }

    if (!m_focus || m_focusTimer->isActive())
        return;

    TraceResult result = trace(primaryPos, TraceKeepFocus);
    Q_ASSERT(result.item == m_focus);

    QRect rect = QRect(m_focus->surface()->pos().toPoint(), m_focus->surface()->size());
    QVector2D size = QVector2D(rect.bottomRight());
    QVector2D local = QVector2D(result.u, result.v) * size;

    if (!m_resizeGrip.isNull()) {
        resizeTo(local);
        return;
    }

    m_input->sendMouseMoveEvent(local.toPoint());
}

void View::keyPressEvent(QKeyEvent *event)
{
    if (m_focus) {
        m_input->sendFullKeyEvent(event);
    } else {
        handleKey(event->key(), true);

        bool active = !qFuzzyIsNull(m_strafingVelocity) || !qFuzzyIsNull(m_walkingVelocity) || !qFuzzyIsNull(m_pitchSpeed) || !qFuzzyIsNull(m_turningSpeed) || m_jumping;

        if (m_animationTimer->isSingleShot() && active) {
            m_simulationTime = m_time.elapsed();
            m_animationTimer->setSingleShot(false);
            m_animationTimer->start();
        }
    }
}

void View::keyReleaseEvent(QKeyEvent *event)
{
    if (m_focus) {
        m_input->sendFullKeyEvent(event);
    } else if (!event->isAutoRepeat()) {
        handleKey(event->key(), false);

        bool active = !qFuzzyIsNull(m_strafingVelocity) || !qFuzzyIsNull(m_walkingVelocity) || !qFuzzyIsNull(m_pitchSpeed) || !qFuzzyIsNull(m_turningSpeed) || m_jumping || m_mouseLook || m_dragItem || m_camera.pos().y() > 0;

        if (!m_animationTimer->isSingleShot() && !active)
            m_animationTimer->setSingleShot(true);
   }
}

bool View::handleKey(int key, bool pressed)
{
    switch (key) {
    case Qt::Key_Space:
        m_jumping = pressed;
        return true;
    case Qt::Key_Left:
    case Qt::Key_Q:
        m_turningSpeed = (pressed ? 0.75 : 0.0);
        return true;
    case Qt::Key_Right:
    case Qt::Key_E:
        m_turningSpeed = (pressed ? -0.75 : 0.0);
        return true;
    case Qt::Key_Down:
        m_pitchSpeed = (pressed ? -0.75 : 0.0);
        return true;
    case Qt::Key_Up:
        m_pitchSpeed = (pressed ? 0.75 : 0.0);
        return true;
    case Qt::Key_S:
        m_walkingVelocity = (pressed ? -0.01 : 0.0);
        return true;
    case Qt::Key_W:
        m_walkingVelocity = (pressed ? 0.01 : 0.0);
        return true;
    case Qt::Key_A:
        m_strafingVelocity = (pressed ? 0.01 : 0.0);
        return true;
    case Qt::Key_D:
        m_strafingVelocity = (pressed ? -0.01 : 0.0);
        return true;
    case Qt::Key_T:
        if (pressed)
            m_wireframe = !m_wireframe;
        return true;
    }

    return false;
}

bool intersectRay(const QVector3D &ro, const QVector3D &rd, const QVector<QVector3D> &vertices,
                  qreal *u, qreal *v, qreal *t, bool ignoreBounds = false)
{
    QVector3D va = vertices[0];
    QVector3D du = vertices[1] - va;
    QVector3D dv = vertices[3] - va;

    QVector3D normal = QVector3D::crossProduct(du, dv);

    qreal dot = QVector3D::dotProduct(normal, rd);
    if (dot == 0)
        return false;

    qreal ct = QVector3D::dotProduct(va - ro, normal) / dot;

    QVector3D deltaHit = ro + rd * ct - va;

    qreal cu = QVector3D::dotProduct(deltaHit, du) / du.lengthSquared();
    qreal cv = QVector3D::dotProduct(deltaHit, dv) / dv.lengthSquared();

    if (!ignoreBounds && (cu < 0 || cu >= 1 || cv < 0 || cv >= 1))
        return false;

    if (!ignoreBounds && (ct <= 0 || ct >= *t))
        return false;

    *t = ct;
    *u = cu;
    *v = cv;
    return true;
}

View::TraceResult View::trace(const QPointF &pos, TraceFlags flags)
{
    TraceResult result;
    result.item = 0;

    QVector2D c(m_camera.viewSize().width() * 0.5, m_camera.viewSize().height() * 0.5);
    QVector2D mapped = ((QVector2D(pos) - c) * QVector2D(1, -1)) * QVector2D(1 / c.x(), 1 / c.y());

    QMatrix4x4 proj = m_camera.projectionMatrix();
    QMatrix4x4 viewInv = m_camera.viewMatrix().inverted();

    qreal fov = proj(1, 1);

    qreal zh = -1.0;
    qreal wh = -1.0 * zh;

    QVector4D rd_eyeSpace(mapped.x() * wh, mapped.y() * wh / fov, zh, 0);

    QVector3D ro = viewInv.map(QVector3D()); // ray origin
    QVector3D rd = viewInv.map(rd_eyeSpace).toVector3D(); // ray direction

    qreal rt_min = FLT_MAX; // min distance along ray to intersection

    if (m_focus && (flags & TraceKeepFocus)) {
        intersectRay(ro, rd, m_focus->vertices(), &result.u, &result.v, &rt_min, true);
        result.item = m_focus;
        result.pos = ro + rd * rt_min;
        return result;
    }

    int zone = m_map.zone(m_camera.pos());

    for (int i = 0; i < m_map.tiles(zone).size(); ++i) {
        if (intersectRay(ro, rd, m_map.tiles(zone).at(i), &result.u, &result.v, &rt_min)) {
            result.zone = zone;
            result.tile = i;
        }
    }

    if (!(flags & TraceIgnoreSurfaces)) {
        for (int i = 0; i < m_mappedSurfaces.size(); ++i) {
            SurfaceItem *item = m_mappedSurfaces.at(i);
            if (intersectRay(ro, rd, item->vertices(), &result.u, &result.v, &rt_min)) {
                result.item = item;
            }
        }
    }

    result.pos = ro + rd * rt_min;

    return result;
}
