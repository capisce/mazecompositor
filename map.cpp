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

#include "map.h"

#include <qmath.h>
#include <limits.h>

#include <QQueue>

#include "common.h"

Map::Map()
{
    QVector<QVector3D> lights;

    Portal *pa = new Portal(QVector3D(3.5, 0.0, 3.5), QVector3D(0.0, 0.0, -1.0));
    Portal *pb = new Portal(QVector3D(1.5, 0.0, 2.5), QVector3D(1.0, 0.0, 0.0));

    pa->setScale(1.2);
    pb->setScale(0.8);

    pa->setTarget(pb);
    pb->setTarget(pa);

    Portal *pc = new Portal(QVector3D(3.5, 0.0, 9.5), QVector3D(0.0, 0.0, 1.0));
    Portal *pd = new Portal(QVector3D(3.5, 0.0, 6.5), QVector3D(-1.0, 0.0, 0.0));

    pc->setTarget(pd);
    pd->setTarget(pc);

    Portal *pe = new Portal(QVector3D(3.5, 0.0, 15.5), QVector3D(0.0, 0.0, 1.0));

    Portal *pf = new Portal(QVector3D(7.5, 0.0, 1.5), QVector3D(0.0, 0.0, 1.0));
    Portal *pg = new Portal(QVector3D(7.5, 0.0, 15.5), QVector3D(0.0, 0.0, -1.0));

    pe->setTarget(pf);
    pf->setTarget(pe);
    pg->setTarget(pd);

    m_portals << pa << pb << pc << pd << pe << pf << pg;

    m_map =
        "###?#.###"
        "#     # #"
        "= o o & #"
        "=     & #"
        "# ### #o#"
        "#   # # #"
        "& o # # #"
        "*     & #"
        "#######o#"
        "##   ## #"
        "#  o  # #"
        "##   ## #"
        "## # ##o#"
        "## o ## #"
        "####### #"
        "##   ## #"
        "#  o  ###"
        "##   ####"
        "## # ####"
        "## o ####"
        "#########";

    Portal *ca = new Portal(QVector3D(3.5, 0.0, 13.5), QVector3D(1.0, 0.0, 0.0));
    Portal *cb = new Portal(QVector3D(3.5, 0.0, 19.5), QVector3D(-1.0, 0.0, 0.0));

    ca->setType(Portal::Corridor);
    cb->setType(Portal::Corridor);

    ca->setScale(2.5);
    cb->setScale(2.5);

    ca->setTarget(cb);
    cb->setTarget(ca);

    m_portals << ca << cb;

    m_dimX = 9;
    m_dimY = 21;

    m_occupied.resize(m_dimX * m_dimY);
    m_zones.fill(false);
    for (int i = 0; i < m_portals.size(); ++i)
    {
        int x = qFloor(m_portals.at(i)->pos().x());
        int y = qFloor(m_portals.at(i)->pos().z());

        m_occupied[y * m_dimX + x] = true;
    }

    m_zones.resize(m_dimX * m_dimY);
    m_zones.fill(INT_MAX);

    int nextZone = 0;
    for (int y = 0; y < m_dimY; ++y) {
        for (int x = 0; x < m_dimX; ++x) {
            if (type(x, y) == Light)
                lights << QVector3D(x + 0.5, 0.96, y + 0.5);
            if (!empty(x, y))
                continue;
            if (zone(x, y) != INT_MAX)
                continue;
            m_zones[y * m_dimX + x] = nextZone;
            QPoint pos(x, y);
            QQueue<QPoint> queue;
            queue.enqueue(pos);
            while (!queue.isEmpty()) {
                QPoint deltas[] = { QPoint(-1, 0), QPoint(1, 0), QPoint(0, -1), QPoint(0, 1) };
                QPoint pos = queue.dequeue();
                for (int i = 0; i < 4; ++i) {
                    QPoint next = pos + deltas[i];
                    if (!empty(next.x(), next.y()))
                        continue;
                    if (m_zones[next.y() * m_dimX + next.x()] == nextZone)
                        continue;
                    m_zones[next.y() * m_dimX + next.x()] = nextZone;
                    queue.enqueue(next);
                }
            }
            ++nextZone;
        }
    }

    m_lights.resize(nextZone);
    m_maxLights = 0;

    for (int i = 0; i < lights.size(); ++i) {
        int z = zone(lights.at(i));
        m_lights[z] << lights.at(i);
        m_maxLights = qMax(m_maxLights, m_lights[z].size());
    }

    m_tiles.resize(nextZone);

    for (int z = 0; z < numZones(); ++z) {
        QSet<int> xGrid;
        QSet<int> yGrid;

        for (int y = 0; y < dimY(); ++y) {
            for (int x = 0; x < dimX(); ++x) {
                if (!empty(x, y) || zone(x, y) != z)
                    continue;
                for (int d = -1; d <= 1; d += 2) {
                    if (!empty(x, y + d)) {
                        if (!empty(x - 1, y))
                            xGrid << x;
                        if (!empty(x + 1, y))
                            xGrid << x + 1;
                        if (empty(x - 1, y + d))
                            xGrid << x;
                        if (empty(x + 1, y + d))
                            xGrid << x + 1;
                    }
                    if (!empty(x + d, y)) {
                        if (!empty(x, y - 1))
                            yGrid << y;
                        if (!empty(x, y + 1))
                            yGrid << y + 1;
                        if (empty(x + d, y - 1))
                            yGrid << y;
                        if (empty(x + d, y + 1))
                            yGrid << y + 1;
                    }
                }
            }
        }

        QVector3D scale(1, 1, 1);

        QList<int> xList = xGrid.toList();
        QList<int> yList = yGrid.toList();

        qSort(xList);
        qSort(yList);

        for (QList<int>::const_iterator yit = yList.constBegin(); (yit + 1) != yList.constEnd(); ++yit) {
            for (QList<int>::const_iterator xit = xList.constBegin(); (xit + 1) != xList.constEnd(); ++xit) {
                int x1 = *xit;
                int y1 = *yit;
                int x2 = *(xit + 1);
                int y2 = *(yit + 1);

                if (!empty(x1, y1))
                    continue;

                QVector3D dim(x2 - x1, 1, y2 - y1);

                m_tiles[z] << tile(x1, y1, Ceiling, scale, dim);
                m_tiles[z] << tile(x1, y1, Floor, scale, dim);

                if (!empty(x1 - 1, y1))
                    m_tiles[z] << tile(x1, y1, West, scale, dim);
                if (!empty(x2, y1))
                    m_tiles[z] << tile(x1, y1, East, scale, dim);
                if (!empty(x1, y1 - 1))
                    m_tiles[z] << tile(x1, y1, North, scale, dim);
                if (!empty(x1, y2))
                    m_tiles[z] << tile(x1, y1, South, scale, dim);
            }
        }
    }

    m_time.start();
};

QVector<QVector3D> Map::lights(int z) const
{
    if (z < 0 || z >= m_lights.size())
        return QVector<QVector3D>();
    return m_lights.at(z);
}

