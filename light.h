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

#ifndef LIGHT_H
#define LIGHT_H

#include <QVector>
#include <QVector3D>

class Camera;
class Map;

class QOpenGLShaderProgram;
class QObject;

class Light
{
public:
    Light(int zone, int index)
        : m_zone(zone)
        , m_index(index)
    {
    }

    void render(const Map &map, const Camera &camera) const;

    static void initialize(QObject *parent);

private:
    int m_zone;
    int m_index;

    static uint m_vertexAttr;
    static uint m_normalAttr;
    static uint m_matrixUniform;
    static uint m_offsetUniform;

    static QOpenGLShaderProgram *m_program;

    static QVector<QVector3D> m_normalBuffer;
    static QVector<QVector3D> m_vertexBuffer;
    static QVector<ushort> m_indexBuffer;

};

#endif
