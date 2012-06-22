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

#ifndef CAMERA_H
#define CAMERA_H

#include <QVector>
#include <QVector2D>
#include <QMatrix4x4>

class Camera
{
public:
    Camera()
        : m_yaw(0)
        , m_pitch(0)
        , m_fov(60)
        , m_height(1.0)
        , m_zNear(0.01)
        , m_zFar(1000.0)
        , m_time(0)
        , m_view(100, 100)
        , m_nearClipPlane(QVector4D(0, 0, -1, -m_zNear))
        , m_matrixDirty(true)
    {
    }

    void setViewSize(const QSize &size);
    QSize viewSize() const { return m_view; }

    qreal yaw() const;
    qreal pitch() const;
    qreal fov() const;

    QVector3D pos() const { return m_pos; }
    QVector3D direction() const;

    QVector3D viewPos() const;

    qreal zNear() const { return m_zNear; }
    qreal zFar() const { return m_zFar; }

    qreal height() const { return m_height; }
    void setHeight(qreal height) { m_height = height; m_matrixDirty = true; }

    QVector4D nearClipPlane() const { return m_nearClipPlane; }
    void setNearClipPlane(const QVector4D &clipPlane) { m_nearClipPlane = clipPlane; m_matrixDirty = true; }

    qreal viewBob() const;
    qreal bobResetTime() const;

    void setZNear(qreal zNear) { m_zNear = zNear; m_matrixDirty = true; }
    void setZFar(qreal zFar) { m_zFar = zFar; m_matrixDirty = true; }

    void setYaw(qreal yaw);
    void setPitch(qreal pitch);
    void setPos(const QVector3D &pos);
    void setFov(qreal fov);
    void setTime(qreal time);

    const QMatrix4x4 &viewProjectionMatrix() const;
    const QMatrix4x4 &viewMatrix() const;
    const QMatrix4x4 &projectionMatrix() const;

    QVector2D toScreen(const QVector3D &coordinate) const;

    QRectF toScreenRect(const QVector<QVector3D> &coordinates) const;

private:
    void updateMatrix() const;

    qreal m_yaw;
    qreal m_pitch;
    qreal m_fov;
    qreal m_height;
    qreal m_zNear;
    qreal m_zFar;
    qreal m_time;

    QSize m_view;

    QVector3D m_pos;
    QVector4D m_nearClipPlane;

    mutable bool m_matrixDirty;
    mutable QMatrix4x4 m_viewMatrix;
    mutable QMatrix4x4 m_viewProjectionMatrix;
    mutable QMatrix4x4 m_projectionMatrix;
    mutable QVector3D m_direction;
};

#endif
