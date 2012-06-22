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

#ifndef ENTITY_H
#define ENTITY_H

#include <QSize>
#include <QVector3D>
#include <QObject>

class Camera;
class Map;
class QOpenGLShaderProgram;

class Entity : public QObject
{
    Q_OBJECT
public:
    Entity(QObject *parent = 0);
    void updateTransform(const Camera &camera);

    void setPosition(const QVector3D &pos) { m_pos = pos; }
    void setDirection(const QVector3D &dir) { m_dir = dir; }

    void setWalking(bool walking) { m_walking = walking; }
    void setScale(qreal scale) { m_scale = scale; }

    void initialize();
    void render(const Map &map, const Camera &camera) const;

protected:
    void timerEvent(QTimerEvent *event);

private:
    QVector3D m_pos;
    QVector3D m_dir;
    QVector3D m_a;
    QVector3D m_b;

    bool m_walking;
    qreal m_scale;

    int m_animationIndex;
    int m_angleIndex;

    int m_tileMod;
    int m_tileWidth;
    int m_tileHeight;
    QSize m_textureSize;
    uint m_texture;

    uint m_vertexAttr;
    uint m_texAttr;
    uint m_matrixUniform;

    QOpenGLShaderProgram *m_program;
};

#endif
