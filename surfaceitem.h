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

#ifndef SURFACEITEM_H
#define SURFACEITEM_H

#include <QImage>
#include <QTime>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

#include <QPropertyAnimation>

class Camera;
class Map;
class QWaylandSurface;

class QOpenGLShaderProgram;

class SurfaceItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
public:
    SurfaceItem(QWaylandSurface *surface);
    ~SurfaceItem();

    QVector<QVector3D> vertices() const;

    QWaylandSurface *surface() const
    {
        return m_surface;
    }

    void setPos(const QVector3D &pos) { m_pos = pos; }
    void setNormal(const QVector3D &normal) { m_normal = normal; }

    void setDepthOffset(qreal offset) { m_depthOffset = offset; }
    qreal depthOffset() const { return m_depthOffset; }

    qreal height() const;
    qreal maxHeight() const;

    void setFocus(bool focus);
    void setHeight(qreal height);

    void setMipmap(bool mipmap) { m_mipmap = mipmap; }

    uint textureId() const;
    QSize size() const;

    void render(const Map &map, const Camera &camera) const;
    void setOpacity(qreal op);
    qreal opacity() const { return m_opacity; }

    static void initialize(const Map &map, QObject *parent);

private slots:
    void surfaceDamaged(const QRect &rect);
    void sizeChanged();

signals:
    void opacityChanged();

private:
    QWaylandSurface *m_surface;

    QVector3D m_pos;
    QVector3D m_normal;
    qreal m_depthOffset;
    qreal m_opacity;

    static QOpenGLShaderProgram *m_program;

    static uint m_vertexAttr;
    static uint m_texCoordAttr;
    static uint m_matrixUniform;
    static uint m_pixelSizeUniform;
    static uint m_eyeUniform;
    static uint m_normalUniform;
    static uint m_lightsUniform;
    static uint m_numLightsUniform;
    static uint m_focusColorUniform;

    uint m_textureId;
    QRect m_dirty;
    QSize m_textureSize;

    QTime m_time;
    qreal m_height;
    bool m_focus;
    bool m_mipmap;

    QPropertyAnimation *m_opacityAnimation;
};

#endif
