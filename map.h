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

#ifndef MAP_H
#define MAP_H

#include <QByteArray>
#include <QTime>
#include <QVector>
#include <QVector3D>

#include <qmath.h>

class Portal
{
public:
    enum Type
    {
        Corridor,
        Gate
    };

    Portal(const QVector3D &pos, const QVector3D &normal, Type type = Gate, qreal scale = 1)
        : m_pos(pos)
        , m_normal(normal)
        , m_type(type)
        , m_scale(scale)
        , m_target(0)
    {
    }

    void setType(Type type) { m_type = type; }
    void setScale(qreal scale) { m_scale = scale; }

    Portal *target() const { return m_target; }
    void setTarget(Portal *target) { m_target = target; }

    QVector3D pos() const { return m_pos; }
    QVector3D normal() const { return m_normal; }
    Type type() const { return m_type; }
    qreal scale() const { return m_scale; }

private:
    QVector3D m_pos;
    QVector3D m_normal;
    Type m_type;
    qreal m_scale;
    Portal *m_target;
};

class Map
{
public:
    enum CellType
    {
        Wall,
        Light,
        Empty
    };

    Map();

    int dimX() const { return m_dimX; }
    int dimY() const { return m_dimY; }

    bool contains(int x, int y) const
    {
        return x >= 0 && x < m_dimX && y >= 0 && y < m_dimY;
    }

    CellType type(int x, int y) const
    {
        switch (m_map.at(y * m_dimX + x)) {
        case ' ':
            return Empty;
        case 'o':
            return Light;
        default:
            return Wall;
        }
    }

    bool occupied(int x, int y) const
    {
        return !empty(x, y) || m_occupied.at(y * m_dimX + x);
    }

    bool occupied(const QVector3D &pos) const
    {
        return occupied(qFloor(pos.x()), qFloor(pos.z()));
    }

    bool empty(int x, int y) const
    {
        return type(x, y) != Wall;
    }

    bool operator()(int x, int y) const
    {
        return empty(x, y);
    }

    int zone(const QVector3D &pos) const
    {
        return zone(qFloor(pos.x()), qFloor(pos.z()));
    }

    int zone(int x, int y) const
    {
        return m_zones.value(y * m_dimX + x, -1);
    }

    QVector<QVector3D> lights(int zone) const;

    int maxLights() const
    {
        return m_maxLights;
    }

    QList<QVector<QVector3D> > tiles(int zone) const
    {
        return m_tiles.at(zone);
    }

    int numZones() const
    {
        return m_lights.size();
    }

    int numPortals() const
    {
        return m_portals.size();
    }

    const Portal *portal(int i) const
    {
        return m_portals.at(i);
    }

private:
    QByteArray m_map;
    QVector<int> m_zones;
    QVector<bool> m_occupied;

    int m_dimX;
    int m_dimY;

    QTime m_time;

    QVector<QVector<QVector3D> > m_lights;
    QVector<QList<QVector<QVector3D > > > m_tiles;
    QVector<Portal *> m_portals;

    int m_maxLights;
};

#endif
