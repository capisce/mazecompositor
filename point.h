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

#ifndef POINT_H
#define POINT_H

#include <qglobal.h>

#include <QVector3D>

static const double scaling = (1 << 16);

class Point
{
private:
    qint64 m_x;
    qint64 m_y;
    qint64 m_z;

public:
    Point() : m_x(0), m_y(0), m_z(0) {}
    Point(qint64 x, qint64 y, qint64 z) : m_x(x), m_y(y), m_z(z) {}

    static Point fromVector3D(const QVector3D &v)
    {
        return Point(qRound64(v.x() * scaling),
                     qRound64(v.y() * scaling),
                     qRound64(v.z() * scaling));
    }

    QVector3D toVector3D() const
    {
        return QVector3D(m_x / scaling, m_y / scaling, m_z / scaling);
    }

    bool operator==(const Point &o) const
    {
        return m_x == o.m_x && m_y == o.m_y && m_z == o.m_z;
    }

    bool operator!=(const Point &o) const
    {
        return m_x != o.m_x || m_y != o.m_y || m_z != o.m_z;
    }

    qint64 x() const { return m_x; }
    qint64 y() const { return m_y; }
    qint64 z() const { return m_z; }

    Point &operator+=(const Point &o)
    {
        m_x += o.m_x;
        m_y += o.m_y;
        m_z += o.m_z;
        return *this;
    }

    Point operator+(const Point &o) const
    {
        Point p(*this);
        p += o;
        return p;
    }

    Point &operator-=(const Point &o)
    {
        m_x -= o.m_x;
        m_y -= o.m_y;
        m_z -= o.m_z;
        return *this;
    }

    Point operator-(const Point &o) const
    {
        Point p(*this);
        p -= o;
        return p;
    }

    Point &operator/=(double v)
    {
        m_x = qRound(m_x / v);
        m_y = qRound(m_y / v);
        m_z = qRound(m_z / v);
        return *this;
    }

    Point normalized() const
    {
        return *this / toVector3D().length();
    }

    Point operator/(double v) const
    {
        Point p(*this);
        p /= v;
        return p;
    }

    static QVector3D crossProductF(const Point &a, const Point &b)
    {
        return QVector3D::crossProduct(a.toVector3D(), b.toVector3D());
    }

    static Point crossProduct(const Point &a, const Point &b)
    {
        return fromVector3D(crossProductF(a, b));
    }

    static qreal dotProduct(const Point &a, const Point &b)
    {
        return QVector3D::dotProduct(a.toVector3D(), b.toVector3D());
    }
};

inline uint qHash(const Point &point)
{
    qint64 x = point.x();
    qint64 y = point.y();
    qint64 z = point.z();

    qint64 ax = qAbs(x);
    qint64 ay = qAbs(y);
    qint64 az = qAbs(z);

    uint result = (ax >> 12) & 0x3ff;
    result |= (ay >> 2) &    0xffc00;
    result |= (az << 6) & 0x3ff00000;
    result |= (x ^ y ^ z) >> 32;

    return result;
}

#endif
