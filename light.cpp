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

#include "light.h"

#include "camera.h"
#include "common.h"
#include "map.h"

uint Light::m_vertexAttr = 0;
uint Light::m_normalAttr = 0;
uint Light::m_matrixUniform = 0;
uint Light::m_offsetUniform = 0;

QOpenGLShaderProgram *Light::m_program = 0;

QVector<QVector3D> Light::m_normalBuffer;
QVector<QVector3D> Light::m_vertexBuffer;
QVector<ushort> Light::m_indexBuffer;

const QVector3D lightSize(0.1, 0.04, 0.1);

void Light::render(const Map &map, const Camera &camera) const
{
    glCullFace(GL_FRONT);

    m_program->bind();

    m_program->setUniformValue(m_matrixUniform, camera.viewProjectionMatrix());

    m_program->setUniformValue(m_offsetUniform, QVector4D(map.lights(m_zone).at(m_index) - QVector3D(lightSize.x(), 0.0, lightSize.z()) * 0.5, 0));

    m_program->enableAttributeArray(m_vertexAttr);
    m_program->setAttributeArray(m_vertexAttr, m_vertexBuffer.constData());
    m_program->enableAttributeArray(m_normalAttr);
    m_program->setAttributeArray(m_normalAttr, m_normalBuffer.constData());

    glDrawElements(GL_TRIANGLES, m_indexBuffer.size(), GL_UNSIGNED_SHORT, m_indexBuffer.constData());

    m_program->disableAttributeArray(m_normalAttr);
    m_program->disableAttributeArray(m_vertexAttr);
}

void Light::initialize(QObject *parent)
{
    QByteArray vsrc =
        "attribute highp vec4 vertexAttr;\n"
        "attribute highp vec3 normalAttr;\n"
        "uniform highp vec4 offset;\n"
        "uniform mediump mat4 matrix;\n"
        "void main(void)\n"
        "{\n"
        "    vec4 pos = vertexAttr + offset;\n"
        "    gl_Position = matrix * pos;\n"
        "}\n";

    QByteArray fsrc =
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = vec4(1);\n"
        "}\n";

    m_program = generateShaderProgram(parent, vsrc, fsrc);

    m_vertexAttr = m_program->attributeLocation("vertexAttr");
    m_normalAttr = m_program->attributeLocation("normalAttr");
    m_matrixUniform = m_program->uniformLocation("matrix");
    m_offsetUniform = m_program->uniformLocation("offset");

    Mesh mesh;
    for (int i = 0; i < 6; ++i)
        mesh.addFace(tile(0, 0, TileType(i), lightSize));

    mesh.borderize(0.45);
    mesh.catmullClarkSubdivide();

    m_normalBuffer = mesh.normalBuffer();
    m_vertexBuffer = mesh.vertexBuffer();
    for (int i = 0; i < mesh.indexBuffer().size(); ++i)
        m_indexBuffer << mesh.indexBuffer().at(i);
}
