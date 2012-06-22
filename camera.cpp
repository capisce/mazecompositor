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

#include "camera.h"

#include <QLineF>

#include <qmath.h>

void Camera::setPitch(qreal pitch)
{
    m_pitch = qBound(qreal(-30), pitch, qreal(30));
    m_matrixDirty = true;
}

QVector3D Camera::direction() const
{
    if (m_matrixDirty)
        updateMatrix();
    return m_direction;
}

qreal Camera::yaw() const { return m_yaw; }
qreal Camera::pitch() const { return m_pitch; }
qreal Camera::fov() const { return m_fov; }

void Camera::setYaw(qreal yaw)
{
    m_yaw = yaw;
    m_matrixDirty = true;
}

void Camera::setPos(const QVector3D &pos)
{
    m_pos = pos;
    m_matrixDirty = true;
}

void Camera::setFov(qreal fov)
{
    m_fov = fov;
    m_matrixDirty = true;
}

void Camera::setTime(qreal time)
{
    m_time = time;
    m_matrixDirty = true;
}

const QMatrix4x4 &Camera::viewMatrix() const
{
    updateMatrix();
    return m_viewMatrix;
}

const QMatrix4x4 &Camera::viewProjectionMatrix() const
{
    updateMatrix();
    return m_viewProjectionMatrix;
}

const QMatrix4x4 &Camera::projectionMatrix() const
{
    updateMatrix();
    return m_projectionMatrix;
}

QMatrix4x4 fromRotation(float angle, Qt::Axis axis)
{
    QMatrix4x4 m;
    if (axis == Qt::XAxis)
        m.rotate(angle, QVector3D(1, 0, 0));
    else if (axis == Qt::YAxis)
        m.rotate(-angle, QVector3D(0, 1, 0));
    else if (axis == Qt::ZAxis)
        m.rotate(angle, QVector3D(0, 0, 1));
    return m;
}

void Camera::setViewSize(const QSize &size)
{
    m_view = size;
}

inline float sign(float x)
{
    return x >= 0 ? 1 : -1;
}

void Camera::updateMatrix() const
{
    if (!m_matrixDirty)
        return;

    m_matrixDirty = false;

    QMatrix4x4 m;
    m *= fromRotation(m_yaw - 180, Qt::YAxis);
    m.translate(-m_pos.x(), -viewPos().y(), -m_pos.z());
    m = fromRotation(m_pitch, Qt::XAxis) * m;
    m_viewMatrix = m;

    float fovAngle = m_fov * 2 * 3.14159 / 360.0;

    float angle = qCos(fovAngle / 2) / qSin(fovAngle / 2);
    float fov = m_view.width() / (float(m_view.height()) * angle);

    float zn = zNear();
    float zf = zFar();

    float m33 = -(zn + zf) / (zf - zn);
    float m34 = -(2 * zn * zf) / (zf - zn);

    qreal data[] =
    {
        1.0, 0.0, 0, 0,
        0, fov, 0, 0,
        0, 0, m33, m34,
        0, 0, -1, 0
    };

    m_projectionMatrix = QMatrix4x4(data);

    // Technique from "Oblique View Frustum Depth Projection and Clipping" by Eric Lengyel

    QVector4D viewClipPlane = m_nearClipPlane;

    QVector4D q((sign(viewClipPlane.x())),
                (sign(viewClipPlane.y())) / fov,
                -1.0,
                (1.0 + m33) / m34);

    QVector4D c = viewClipPlane * (2.0 / QVector4D::dotProduct(viewClipPlane, q)) + QVector4D(0, 0, 1, 0);

    m_projectionMatrix.setRow(2, c);
    m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;

    QPointF p = QLineF::fromPolar(1, m_yaw - 90).p2();
    m_direction = QVector3D(p.x(), 0, p.y());
}

QVector3D Camera::viewPos() const
{
    return m_pos + QVector3D(0, m_height * (0.4 + viewBob()), 0);
}

qreal Camera::viewBob() const
{
    return 0.03 * qSin(10 * m_time);
}

qreal Camera::bobResetTime() const
{
    qreal pi = 3.14159265;
    qreal next = qCeil(10 * m_time / pi);
    return pi * next / 10;
}

QVector2D Camera::toScreen(const QVector3D &coordinate) const
{
    QVector3D projected = m_viewProjectionMatrix.map(coordinate);
    QVector2D c(viewSize().width() * 0.5, viewSize().height() * 0.5);

    return c + c * projected.toVector2D() * QVector2D(1, -1);
}

QRectF Camera::toScreenRect(const QVector<QVector3D> &coordinates) const
{
    QVector<QVector3D> mapped;
    for (int i = 0; i < coordinates.size(); ++i)
        mapped << m_viewMatrix.map(coordinates.at(i));

    qreal zClip = -m_zNear;

    QVector<QVector3D> clipped;
    for (int i = 0; i < mapped.size(); ++i) {
        const QVector3D &a = mapped.at(i);
        const QVector3D &b = mapped.at((i + 1) % mapped.size());

        bool aOut = a.z() > zClip;
        bool bOut = b.z() > zClip;

        if (aOut && bOut)
            continue;

        if (!aOut && !bOut) {
            clipped << b;
            continue;
        }

        qreal t = (zClip - a.z()) / (b.z() - a.z());
        QVector3D intersection = a + t * (b - a);

        clipped << intersection;

        if (!bOut)
            clipped << b;
    }

    QRectF bounds;

    QVector2D c(viewSize().width() * 0.5, viewSize().height() * 0.5);

    for (int i = 0; i < clipped.size(); ++i) {
        QVector2D projected = c + c * m_projectionMatrix.map(clipped.at(i)).toVector2D() * QVector2D(1, -1);
        bounds = bounds.united(QRectF(projected.toPointF(), QSizeF(0.01, 0.01)));
    }

    bounds = bounds.intersected(QRectF(0, 0, viewSize().width(), viewSize().height()));

    return bounds;
}
