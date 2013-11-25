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

#include "surfaceitem.h"

#include "camera.h"
#include "common.h"
#include "map.h"
#include "qwaylandsurface.h"

#include <QGuiApplication>
#include <QPainter>
#include <QOpenGLPaintDevice>
#include <QScreen>

QOpenGLShaderProgram *SurfaceItem::m_program = 0;

uint SurfaceItem::m_vertexAttr = 0;
uint SurfaceItem::m_texCoordAttr = 0;
uint SurfaceItem::m_matrixUniform = 0;
uint SurfaceItem::m_pixelSizeUniform = 0;
uint SurfaceItem::m_eyeUniform = 0;
uint SurfaceItem::m_focusColorUniform = 0;
uint SurfaceItem::m_normalUniform = 0;
uint SurfaceItem::m_lightsUniform = 0;
uint SurfaceItem::m_numLightsUniform = 0;

SurfaceItem::SurfaceItem(QWaylandSurface *surface)
    : m_surface(surface)
    , m_depthOffset(0)
    , m_opacity(0.55)
    , m_textureId(0)
    , m_height(maxHeight() * 0.99)
    , m_focus(false)
    , m_mipmap(true)
{
    m_time.start();
    connect(surface, SIGNAL(damaged(const QRect &)), this, SLOT(surfaceDamaged(const QRect &)));
    connect(surface, SIGNAL(sizeChanged()), this, SLOT(sizeChanged()));
    m_dirty = QRect(QPoint(), surface->size());

    qDebug() << "surface size:" << surface->size();
    m_opacityAnimation = new QPropertyAnimation(this, "opacity");
    m_opacityAnimation->setDuration(400);
    sizeChanged();
}

SurfaceItem::~SurfaceItem()
{
    if (!m_textureSize.isNull())
        glDeleteTextures(1, &m_textureId);
}

void SurfaceItem::setOpacity(qreal op)
{
    if (op != m_opacity) {
        m_opacity = op;
        emit opacityChanged();
    }
}

void SurfaceItem::setFocus(bool focus)
{
    if (focus != m_focus) {
        m_focus = focus;
        m_opacityAnimation->setEndValue(focus ? 1.0 : 0.55);
        m_opacityAnimation->start();
    }
}

void SurfaceItem::initialize(const Map &map, QObject *parent)
{
    QByteArray vsrc =
        "attribute highp vec4 vertexAttr;\n"
        "attribute highp vec2 texCoordAttr;\n"
        "uniform mediump mat4 matrix;\n"
        "varying highp vec2 texCoord;\n"
        "varying mediump vec3 p;\n"
        "void main(void)\n"
        "{\n"
        "    texCoord = texCoordAttr;\n"
        "    p = vertexAttr.xyz;\n"
        "    gl_Position = matrix * vertexAttr;\n"
        "}\n";

    QByteArray fsrc =
        useSimpleShading() ?
            "uniform sampler2D texture;\n"
            "varying highp vec2 texCoord;\n"
            "uniform lowp float focusColor;\n"
            "void main(void)\n"
            "{\n"
            "    lowp vec4 tex = texture2D(texture, texCoord);\n"
            "    gl_FragColor = tex * 0.9 * focusColor * tex.a;\n"
            "}\n"
        :
            "uniform sampler2D texture;\n"
            "uniform highp vec2 pixelSize;\n"
            "uniform lowp vec3 normal;\n"
            "varying highp vec2 texCoord;\n"
            "varying highp vec3 p;\n"
            "uniform int numLights;\n"
            "uniform highp vec3 lights[NUM_LIGHTS];\n"
            "uniform highp vec3 eye;\n"
            "uniform lowp float focusColor;\n"
            "void main(void)\n"
            "{\n"
            "    highp vec4 tex = texture2D(texture, texCoord);\n"
            "    highp vec2 dt = abs(texCoord - vec2(0.5));\n"
            "    highp vec3 toEyeN = normalize(eye - p);\n"
            "    highp vec4 result = tex * 0.9;\n" // light source
            "    for (int i = 0; i < NUM_LIGHTS; ++i) {\n"
            "        highp vec3 toLight = lights[i] - p;\n"
            "        highp vec3 toLightN = normalize(toLight);\n"
            "        highp float normalDotLight = dot(toLightN, normal);\n"
            "        highp float lightDistance = length(toLight);\n"
            "        highp float reflectionDotView = max(0.0, dot(normalize(((2.0 * normal) * normalDotLight) - toLightN), toEyeN));\n"
            "        highp vec3 specular = 0.5 * vec3(0.75 * pow(reflectionDotView, 8.0) / max(1.5, 0.8 * lightDistance));\n"
            "        if (i < numLights)\n"
            "            result += vec4(specular, 1.0);\n"
            "    }\n"
            "    highp vec4 blend = mix(vec4(0.0), result * tex.a, (1.0 - smoothstep(0.5 - pixelSize.x, 0.5, dt.x)) * (1.0 - smoothstep(0.5 - pixelSize.y, 0.5, dt.y)));\n"
            "    gl_FragColor = mix(min(blend, vec4(1.0)) * focusColor, tex, focusColor);\n"
            "}\n";

    fsrc.replace("NUM_LIGHTS", QByteArray::number(map.maxLights()));

    m_program = generateShaderProgram(parent, vsrc, fsrc);

    m_vertexAttr = m_program->attributeLocation("vertexAttr");
    m_texCoordAttr = m_program->attributeLocation("texCoordAttr");
    m_matrixUniform = m_program->uniformLocation("matrix");
    m_focusColorUniform = m_program->uniformLocation("focusColor");
    m_pixelSizeUniform = m_program->uniformLocation("pixelSize");
    m_eyeUniform = m_program->uniformLocation("eye");
    m_normalUniform = m_program->uniformLocation("normal");
    m_lightsUniform = m_program->uniformLocation("lights");
    m_numLightsUniform = m_program->uniformLocation("numLights");
}

void SurfaceItem::setHeight(qreal height)
{
    m_height = qBound(qreal(0.4), height, maxHeight());
}

qreal SurfaceItem::height() const
{
    return m_height;
}

void SurfaceItem::sizeChanged()
{
    setHeight(0.8 * m_surface->size().height() / qreal(QGuiApplication::primaryScreen()->size().height()));
}

qreal SurfaceItem::maxHeight() const
{
    return 0.8;
}

QVector<QVector3D> SurfaceItem::vertices() const
{
    QSize size = m_surface->size();

    qreal w = (m_height * size.width()) / size.height();
    qreal h = m_height;

    QVector3D pos = m_pos + m_normal * m_depthOffset;

    QVector2D center(pos.x(), pos.z());

    QVector3D perp = QVector3D::crossProduct(QVector3D(0, 1, 0), m_normal);
    QVector2D delta = w * QVector2D(perp.x(), perp.z()).normalized() / 2;

    qreal scale = qMin(1.0, m_time.elapsed() * 0.002);

    qreal top = m_pos.y() + h * 0.5 * scale;
    qreal bottom = m_pos.y() - h * 0.5 * scale;

    QVector2D left = center - delta * scale;
    QVector2D right = center + delta * scale;

    QVector3D va(left.x(), top, left.y());
    QVector3D vb(right.x(), top, right.y());
    QVector3D vc(right.x(), bottom, right.y());
    QVector3D vd(left.x(), bottom, left.y());

    QVector<QVector3D> result;
    result << va << vb << vc << vd;
    return result;
}

uint SurfaceItem::textureId() const
{
    uint id = 0;
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (m_surface->type() == QWaylandSurface::Texture) {
        id = m_surface->texture(ctx);
    } else {
        QImage image = m_surface->image();
        if (m_textureSize != image.size()) {
            if (!m_textureSize.isNull())
                glDeleteTextures(1, &m_textureId);
            const_cast<SurfaceItem *>(this)->m_textureId = generateTexture(image, true, false);
            const_cast<SurfaceItem *>(this)->m_textureSize = image.size();
        } else if (!m_dirty.isNull()) {
            updateSubImage(m_textureId, image, m_dirty, true);
        }
        const_cast<SurfaceItem *>(this)->m_dirty = QRect();
        id = m_textureId;
    }

    if (m_mipmap && canUseMipmaps(m_textureSize)) {
        glBindTexture(GL_TEXTURE_2D, id);
        ctx->functions()->glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        const_cast<bool &>(m_mipmap) = false;
    }

    return id;
}

void SurfaceItem::surfaceDamaged(const QRect &rect)
{
    m_dirty = m_dirty | rect;
}

QSize SurfaceItem::size() const
{
    return m_surface->size();
}

void SurfaceItem::render(const Map &map, const Camera &camera) const
{
    int zone = map.zone(vertices().at(0));

    GLuint tex = textureId();

    if (zone < 0)
        return;

    m_program->bind();
    m_program->setUniformValue(m_matrixUniform, camera.viewProjectionMatrix());

    QSize size = m_surface->size();
    m_program->setUniformValue(m_pixelSizeUniform, 5. / size.width(), 5. / size.height());
    m_program->setUniformValue(m_eyeUniform, camera.viewPos());
    m_program->setUniformValue(m_focusColorUniform, GLfloat(m_opacity));
    m_program->setUniformValueArray(m_lightsUniform, map.lights(zone).constData(), map.lights(zone).size());
    m_program->setUniformValue(m_numLightsUniform, map.lights(zone).size());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    QVector<QVector3D> v = vertices();

    QVector3D va = v[0];
    QVector3D vb = v[1];
    QVector3D vc = v[2];
    QVector3D vd = v[3];

    QVector<QVector3D> vertexBuffer;
    vertexBuffer << va << vb << vd << vd << vb << vc;

    qreal y1 = 0;
    qreal y2 = 1;

    if (m_surface->isYInverted())
        qSwap(y1, y2);

    QVector<QVector2D> texCoordBuffer;
    texCoordBuffer << QVector2D(0, y2) << QVector2D(1, y2)
                   << QVector2D(0, y1) << QVector2D(0, y1)
                   << QVector2D(1, y2) << QVector2D(1, y1);

    m_program->setUniformValue(m_normalUniform, -QVector3D::crossProduct(vb - va, vc - va).normalized());

    m_program->enableAttributeArray(m_vertexAttr);
    m_program->setAttributeArray(m_vertexAttr, vertexBuffer.constData());
    m_program->enableAttributeArray(m_texCoordAttr);
    m_program->setAttributeArray(m_texCoordAttr, texCoordBuffer.constData());

    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);

    m_program->disableAttributeArray(m_texCoordAttr);
    m_program->disableAttributeArray(m_vertexAttr);

#if 0
    QOpenGLPaintDevice device(camera.viewSize());
    QPainter p(&device);

    va = camera.viewProjectionMatrix().map(va);
    vb = camera.viewProjectionMatrix().map(vb);
    vc = camera.viewProjectionMatrix().map(vc);
    vd = camera.viewProjectionMatrix().map(vd);

    QVector3D c(camera.viewSize().width() * 0.5, camera.viewSize().height() * 0.5, 0);
    va = c + c * va * QVector3D(1, -1, 0);
    vb = c + c * vb * QVector3D(1, -1, 0);
    vc = c + c * vc * QVector3D(1, -1, 0);
    vd = c + c * vd * QVector3D(1, -1, 0);

    QPointF pa(va.x(), va.y());
    QPointF pb(vb.x(), vb.y());
    QPointF pc(vc.x(), vc.y());
    QPointF pd(vd.x(), vd.y());

    p.drawLine(pa, pb);
    p.drawLine(pb, pc);
    p.drawLine(pc, pd);
    p.drawLine(pd, pa);

    extern QVector3D debug;

    QVector3D d = camera.viewProjectionMatrix().map(debug);
    d = c + c * d * QVector3D(1, -1, 0);

    static QVector3D old;
    if (debug != old)
        old = debug;

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::red);
    p.drawEllipse(QRectF(d.x() - 2, d.y() - 2, 4, 4));

    p.end();
#endif
}
