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

#ifndef COMMON_H
#define COMMON_H

#include <QImage>

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include "mesh.h"

class Camera;

void drawTexture(const QRectF &target, const QSizeF &viewport, GLuint texture, qreal alpha = 1.0, const QRectF &source = QRectF());
void drawRect(const QRectF &target, const QSizeF &viewport, const QColor &color, GLdouble z = 0.0);
void drawConvexSolid(const Camera &camera, const QVector<QVector3D> &outline, const QColor &color);

GLuint generateTexture(const QImage &image, bool mipmaps = true);
void updateSubImage(GLuint texture, const QImage &image, const QRect &rect, bool mipmaps = true);

QOpenGLShaderProgram *generateShaderProgram(QObject *parent, QByteArray vsrc, QByteArray fsrc);

enum TileType
{
    Ceiling,
    Floor,
    North,
    South,
    East,
    West
};

QVector<QVector3D> tile(int x, int z, TileType type, QVector3D scale = QVector3D(1, 1, 1), QVector3D dim = QVector3D(1, 1, 1));

bool useSimpleShading();

#endif
