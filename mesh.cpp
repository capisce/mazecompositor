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

#include "mesh.h"

#include <QRectF>

void Mesh::addFace(const QVector<Point> &face)
{
    Face *result = new Face();
    for (int i = 0; i < face.size(); ++i) {
        result->addEdge(
            allocate(
                face[i],
                face[(i+1) % face.size()],
                result));
    }

    m_faces << result;
}

void Mesh::addFace(const QVector<QVector3D> &face)
{
    Face *result = new Face();
    for (int i = 0; i < face.size(); ++i) {
        result->addEdge(
            allocate(
                Point::fromVector3D(face[i]),
                Point::fromVector3D(face[(i+1) % face.size()]),
                result));
    }

    m_faces << result;
}

Mesh::Mesh()
    : allocate(this)
{
}

Mesh::~Mesh()
{
    qDeleteAll(m_faces);
    qDeleteAll(m_vertices);
    qDeleteAll(m_edges);
}

void Mesh::Face::addEdge(Edge *edge)
{
    m_edges << edge;
}

Mesh::Vertex *Mesh::Face::vertexAt(int i) const
{
    int next = (i + 1) % m_edges.size();

    Edge *ea = m_edges.at(i);
    Edge *eb = m_edges.at(next);

    return Edge::sharedVertex(ea, eb);
}

Mesh::Allocator::Allocator(Mesh *mesh)
    : m_mesh(mesh)
{
}

Mesh::Allocator::~Allocator()
{
}

Mesh::Vertex *Mesh::Allocator::findVertex(const Point &p)
{
    VertexHash::iterator it = m_vertexHash.find(p);

    if (it == m_vertexHash.end()) {
        Vertex *v = new Vertex(p, m_mesh->m_vertices.size());

        m_mesh->m_vertices << v;
        it = m_vertexHash.insert(p, v);
    }

    return *it;
}

Mesh::Edge *Mesh::Allocator::operator()(const Point &a, const Point &b, Face *face)
{
    Vertex *va = findVertex(a);
    Vertex *vb = findVertex(b);

    Edge *edge = 0;

    foreach (Edge *e, va->m_edges) {
        if (e->pa() == vb || e->pb() == vb) {
            edge = e;
            break;
        }
    }

    if (edge) {
        Q_ASSERT(!edge->fb());
        edge->m_fb = face;
    } else {
        edge = new Edge(va, vb, face);
        va->m_edges << edge;
        vb->m_edges << edge;
        m_mesh->m_edges << edge;
    }

    va->m_faces << face;
    va->m_faceList = va->m_faces.toList();
    vb->m_faces << face;
    vb->m_faceList = vb->m_faces.toList();

    return edge;
}

void Mesh::makeBuffers() const
{
    for (int i = 0; i < m_vertices.size(); ++i) {
        m_vertexBuffer << m_vertices.at(i)->point().toVector3D();
        m_normalBuffer << -m_vertices.at(i)->normal().toVector3D().normalized();
    }

    for (int i = 0; i < m_faces.size(); ++i) {
        const Face *f = m_faces.at(i);
        if (f->vertexCount() == 3) {
            for (int j = 0; j < 3; ++j)
                m_indexBuffer << f->vertexAt(j)->index();
        } else {
            Q_ASSERT(f->vertexCount() == 4);
            int i0 = f->vertexAt(0)->index();
            int i1 = f->vertexAt(1)->index();
            int i2 = f->vertexAt(2)->index();
            int i3 = f->vertexAt(3)->index();
            m_indexBuffer << i0 << i1 << i3;
            m_indexBuffer << i3 << i1 << i2;
        }
    }
}

QVector<QVector3D> Mesh::normalBuffer() const
{
    if (m_normalBuffer.isEmpty())
        makeBuffers();
    return m_normalBuffer;
}

QVector<QVector3D> Mesh::vertexBuffer() const
{
    if (m_vertexBuffer.isEmpty())
        makeBuffers();
    return m_vertexBuffer;
}

QVector<uint> Mesh::indexBuffer() const
{
    if (m_indexBuffer.isEmpty())
        makeBuffers();
    return m_indexBuffer;
}

QVector<QVector3D> generateFace(const QVector3D *v, const QRectF &r)
{
    QPointF weights[4] = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };

    QVector<QVector3D> result;
    for (int i = 0; i < 4; ++i) {
        QPointF w = weights[i];
        QPointF iw = QPointF(1, 1) - w;
        result <<
            ((v[0] * iw.x() * iw.y()) +
             (v[1] * w.x() * iw.y()) +
             (v[2] * w.x() * w.y()) +
             (v[3] * iw.x() * w.y()));
    }

    return result;
};

void Mesh::addConvexOutline(const QVector<Point> &o)
{
    if (o.size() < 3)
        return;

    QVector<Point> outline = o;

    while (outline.size() > 3) {
        for (int j = 0; j < outline.size(); ++j) {
            Point last = outline.at((j - 1 + outline.size()) % outline.size());
            Point current = outline.at(j);
            Point next = outline.at((j + 1) % outline.size());

            Point d1 = current - last;
            Point d2 = next - current;

            if (Point::crossProduct(d1, d2) != Point()) {
                QVector<Point> face;
                face << last << current << next;
                addFace(face);
                outline.remove(j);
                break;
            }
        }
    }

    addFace(outline);
}

void Mesh::verify()
{
    for (int i = 0; i < m_vertices.size(); ++i) {
        Vertex *v = m_vertices.at(i);
        if (v->faceSet().size() < 3)
            qDebug() << "Vertex with less than three faces: " << v->point().toVector3D();
    }

    for (int i = 0; i < m_edges.size(); ++i) {
        Edge *e = m_edges.at(i);
        if (!e->pa() || !e->pb())
            printf("Edge with missing vertex\n");

        if (!e->fa() || !e->fb()) {
            qDebug() << "Edge with missing face: " << e->pa()->point().toVector3D() << e->pb()->point().toVector3D();
        }
    }
}

void Mesh::borderize(qreal factor)
{
    Mesh result;
    for (int i = 0; i < m_faces.size(); ++i) {
        Face *f = m_faces.at(i);

        if (f->vertexCount() == 3) {
            result.addFace(f->points());
            continue;
        }

        Q_ASSERT(f->vertexCount() == 4);

        bool allPlanar = f->planarNeighborhood();

        QVector3D p[4] = {
            f->vertexAt(0)->point().toVector3D(),
            f->vertexAt(1)->point().toVector3D(),
            f->vertexAt(2)->point().toVector3D(),
            f->vertexAt(3)->point().toVector3D()
        };

        if (allPlanar) {
            QVector<Point> planar;
            for (int i = 0; i < 4; ++i) {
                QVector3D pa = p[i];
                QVector3D pb = p[(i + 1) % 4];

                Edge *edge = f->edgeAt((i + 1) % 4);

                planar << Point::fromVector3D(pa);

                if (!edge->planarNeighborhood()) {
                    planar << Point::fromVector3D((pa * (1 - factor) + pb * factor))
                           << Point::fromVector3D((pa * factor + pb * (1 - factor)));
                }
            }

            result.addConvexOutline(planar);
        } else {
            result.addFace(generateFace(p, QRectF(QPointF(0, 0), QPointF(factor, factor))));
            result.addFace(generateFace(p, QRectF(QPointF(factor, 0), QPointF(1 - factor, factor))));
            result.addFace(generateFace(p, QRectF(QPointF(1 - factor, 0), QPointF(1, factor))));

            result.addFace(generateFace(p, QRectF(QPointF(0, factor), QPointF(factor, 1 - factor))));
            result.addFace(generateFace(p, QRectF(QPointF(factor, factor), QPointF(1 - factor, 1 - factor))));
            result.addFace(generateFace(p, QRectF(QPointF(1 - factor, factor), QPointF(1, 1 - factor))));

            result.addFace(generateFace(p, QRectF(QPointF(0, 1 - factor), QPointF(factor, 1))));
            result.addFace(generateFace(p, QRectF(QPointF(factor, 1 - factor), QPointF(1 - factor, 1))));
            result.addFace(generateFace(p, QRectF(QPointF(1 - factor, 1 - factor), QPointF(1, 1))));
        }
    }

    swap(result);
}

void Mesh::catmullClarkSubdivide()
{
    Mesh result;

    for (int i = 0; i < m_faces.size(); ++i) {
        Face *f = m_faces.at(i);

        Point facePoint = f->center();

        bool allPlanar = f->planarNeighborhood();

        QVector<Point> planar;

        for (int j = 0; j < f->edgeCount(); ++j) {
            Edge *edge = f->edgeAt(j);
            Edge *next = f->edgeAt((j + 1) % f->edgeCount());

            if (allPlanar) {
                planar << Edge::sharedVertex(edge, next)->catmullClarkMidpoint();
                if (next->fa()->vertexCount() != 3 || next->fb()->vertexCount() != 3 || !next->planarNeighborhood())
                    planar << next->catmullClarkMidpoint();
            } else {
                QVector<Point> points;

                points << facePoint;
                points << edge->catmullClarkMidpoint();
                points << Edge::sharedVertex(edge, next)->catmullClarkMidpoint();
                points << next->catmullClarkMidpoint();

                result.addFace(points);
            }
        }

        if (allPlanar)
            result.addConvexOutline(planar);
    }

    swap(result);
}

void Mesh::swap(Mesh &other)
{
    qSwap(allocate, other.allocate);

    qSwap(m_faces, other.m_faces);
    qSwap(m_vertices, other.m_vertices);
    qSwap(m_edges, other.m_edges);

    qSwap(m_indexBuffer, other.m_indexBuffer);
    qSwap(m_normalBuffer, other.m_normalBuffer);
    qSwap(m_vertexBuffer, other.m_vertexBuffer);
}
