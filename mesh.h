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

#ifndef MESH_H
#define MESH_H

#include "point.h"

#include <QHash>
#include <QSet>
#include <QVarLengthArray>
#include <QVector>
#include <QVector3D>

#include <QtDebug>

class Mesh
{
public:
    Mesh();
    ~Mesh();

    void verify();

    void borderize(qreal factor);
    void catmullClarkSubdivide();

    void addFace(const QVector<QVector3D> &points);
    void addFace(const QVector<Point> &points);

    void addConvexOutline(const QVector<Point> &outline);

    QVector<QVector3D> normalBuffer() const;
    QVector<QVector3D> vertexBuffer() const;
    QVector<uint> indexBuffer() const;

private:
    void makeBuffers() const;
    void swap(Mesh &other);

    class Allocator;
    class Edge;
    class Face;

    class Vertex
    {
    public:
        Point point() const { return m_point; }

        Point normal() const
        {
            Point sum;

            foreach (Face *face, m_faceList)
                sum += face->normal();

            return sum;
        }

        bool isPlanar() const
        {
            for (int i = 1; i < m_faceList.size(); ++i) {
                if (m_faceList.at(i-1)->normal() != m_faceList.at(i)->normal())
                    return false;
            }
            return true;
        }

        int index() const { return m_index; }

        Point catmullClarkMidpoint() const
        {
            int n = m_faceList.size();
            Point result = point() / (n / (n - 3.));
            Point faceSum;
            for (int i = 0; i < n; ++i)
                faceSum += m_faceList.at(i)->center();
            result += faceSum / (n * n);
            Point edgeSum;
            for (int i = 0; i < n; ++i)
                edgeSum += m_edges.at(i)->center();
            result += edgeSum / (n * n * 0.5);
            return result;
        }

        QSet<Face *> faceSet() const
        {
            return m_faces;
        }

    private:
        Vertex(Point p, uint index)
            : m_point(p)
            , m_index(index)
        {}

        Point m_point;
        QVarLengthArray<Edge *, 4> m_edges;
        QSet<Face *> m_faces;
        QList<Face *> m_faceList;

        uint m_index;

        friend class Mesh::Allocator;
    };

    class Edge
    {
    public:
        Vertex *pa() const { return m_pa; }
        Vertex *pb() const { return m_pb; }

        Face *fa() const { return m_fa; }
        Face *fb() const { return m_fb; }

        Point center() const
        {
            return (m_pa->point() + m_pb->point()) / 2;
        }

        Point catmullClarkMidpoint() const
        {
            return (center() + (m_fa->center() + m_fb->center()) / 2) / 2;
        }

        bool planarNeighborhood() const
        {
            return m_fa->planarNeighborhood() && m_fb->planarNeighborhood();
        }

        bool contains(Vertex *v) const
        {
            return pa() == v || pb() == v;
        }

        static Vertex *sharedVertex(Edge *ea, Edge *eb)
        {
            if (ea->pa() == eb->pa() || ea->pa() == eb->pb())
                return ea->pa();
            else
                return ea->pb();
        }

    private:
        Edge(Vertex *a, Vertex *b, Face *f)
            : m_pa(a), m_pb(b), m_fa(f), m_fb(0)
        {
        }

        Vertex *m_pa;
        Vertex *m_pb;

        Face *m_fa;
        Face *m_fb;

        bool m_subdivides;

        friend class Mesh::Allocator;
    };

    class Face
    {
    public:
        void addEdge(Edge *edge);

        int edgeCount() const { return m_edges.size(); }
        Edge *edgeAt(int i) const { return m_edges.at(i); }

        int edgeDirection(int i) const
        {
            Vertex *shared = Edge::sharedVertex(edgeAt(i), edgeAt((i+1)%edgeCount()));
            return shared == edgeAt(i)->pb() ? 1 : -1;
        }

        bool planarNeighborhood() const
        {
            for (int i = 0; i < vertexCount(); ++i)
                if (!vertexAt(i)->isPlanar())
                    return false;
            return true;
        }

        QVector<Point> points() const
        {
            QVector<Point> result;
            for (int i = 0; i < vertexCount(); ++i)
                result << vertexAt(i)->point();
            return result;
        }

        int vertexCount() const { return m_edges.size(); }
        Vertex *vertexAt(int i) const;

        Point normal() const
        {
            Point pa = vertexAt(0)->point();
            Point pb = vertexAt(1)->point();
            Point pc = vertexAt(2)->point();

            return Point::crossProduct(pb - pa, pc - pa).normalized();
        }

        Point center() const
        {
            Point p;

            for (int i = 0; i < vertexCount(); ++i)
                p += vertexAt(i)->point();

            return p / vertexCount();
        }

    private:
        QVarLengthArray<Edge *, 4> m_edges;
        bool m_subdivides;
    };

    class Allocator
    {
    public:
        Allocator(Mesh *mesh);
        ~Allocator();

        Edge *operator()(const Point &a, const Point &b, Face *face);

    private:
        typedef QHash<Point, Vertex *> VertexHash;
        VertexHash m_vertexHash;

        Mesh *m_mesh;

        Mesh::Vertex *findVertex(const Point &p);
    } allocate;

    QList<Face *> m_faces;
    QList<Vertex *> m_vertices;
    QList<Edge *> m_edges;

    mutable QVector<uint> m_indexBuffer;
    mutable QVector<QVector3D> m_normalBuffer;
    mutable QVector<QVector3D> m_vertexBuffer;

    friend class Allocator;
};

#endif
