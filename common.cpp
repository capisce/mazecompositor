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

#include "common.h"

#include "camera.h"

#include <QColor>
#include <QCoreApplication>

namespace {
    QByteArray drawTextureVertexSrc =
        "attribute highp vec4 vertexAttr;\n"
        "attribute highp vec2 texCoordAttr;\n"
        "varying highp vec2 texCoord;\n"
        "void main(void)\n"
        "{\n"
        "    texCoord = texCoordAttr;\n"
        "    gl_Position = vertexAttr;\n"
        "}\n";

    QByteArray drawTextureFragmentSrc =
        "uniform sampler2D texture;\n"
        "varying highp vec2 texCoord;\n"
        "uniform highp float alpha;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = texture2D(texture, texCoord) * alpha;\n"
        "}\n";

    QByteArray drawSolidVertexSrc =
        "attribute highp vec4 vertexAttr;\n"
        "uniform mediump mat4 matrix;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = matrix * vertexAttr;\n"
        "}\n";

    QByteArray drawSolidFragmentSrc =
        "uniform lowp vec4 color;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = color;\n"
        "}\n";

    QByteArray drawRectVertexSrc =
        "attribute highp vec4 vertexAttr;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vertexAttr;\n"
        "}\n";

    QByteArray drawRectFragmentSrc =
        "uniform lowp vec4 color;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = color;\n"
        "}\n";
}

void drawRect(const QRectF &target, const QSizeF &viewport, const QColor &color, GLdouble z)
{
    static QOpenGLShaderProgram *program = 0;
    static int vertexAttr;
    static int colorUniform;

    if (program == 0) {
        program = generateShaderProgram(0, drawRectVertexSrc, drawRectFragmentSrc);
        vertexAttr = program->attributeLocation("vertexAttr");
        colorUniform = program->uniformLocation("color");
    }

    qreal xmin = -1 + 2 * (target.left() / viewport.width());
    qreal xmax = -1 + 2 * (target.right() / viewport.width());
    qreal ymin = -1 + 2 * (target.top() / viewport.height());
    qreal ymax = -1 + 2 * (target.bottom() / viewport.height());

    QVector3D va(xmin, ymin, z);
    QVector3D vb(xmax, ymin, z);
    QVector3D vc(xmax, ymax, z);
    QVector3D vd(xmin, ymax, z);

    QVector3D vertexCoords[] =
    {
        va, vb, vd, vd, vb, vc
    };

    program->bind();
    program->setUniformValue(colorUniform, color);

    program->enableAttributeArray(vertexAttr);
    program->setAttributeArray(vertexAttr, vertexCoords);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    program->disableAttributeArray(vertexAttr);
}

void drawConvexSolid(const Camera &camera, const QVector<QVector3D> &outline, const QColor &color)
{
    static QOpenGLShaderProgram *program = 0;
    static int vertexAttr;
    static int colorUniform;
    static int matrixUniform;

    if (program == 0) {
        program = generateShaderProgram(0, drawSolidVertexSrc, drawSolidFragmentSrc);
        vertexAttr = program->attributeLocation("vertexAttr");
        colorUniform = program->uniformLocation("color");
        matrixUniform = program->uniformLocation("matrix");
    }

    if (color.alpha() != 255) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    program->bind();
    program->setUniformValue(matrixUniform, camera.viewProjectionMatrix());
    program->setUniformValue(colorUniform, color);

    program->enableAttributeArray(vertexAttr);
    program->setAttributeArray(vertexAttr, outline.constData());

    glDrawArrays(GL_TRIANGLE_FAN, 0, outline.size());

    program->disableAttributeArray(vertexAttr);

    if (color.alpha() != 255)
        glDisable(GL_BLEND);
}

void drawTexture(const QRectF &target, const QSizeF &viewport, GLuint texture, qreal alpha, const QRectF &source)
{
    static QOpenGLShaderProgram *program = 0;
    static int vertexAttr;
    static int texCoordAttr;
    static int opacityUniform;

    if (program == 0) {
        program = generateShaderProgram(0, drawTextureVertexSrc, drawTextureFragmentSrc);
        vertexAttr = program->attributeLocation("vertexAttr");
        texCoordAttr = program->attributeLocation("texCoordAttr");
        opacityUniform = program->uniformLocation("alpha");
    }

    qreal xmin = -1 + 2 * (target.left() / viewport.width());
    qreal xmax = -1 + 2 * (target.right() / viewport.width());
    qreal ymin = -1 + 2 * (viewport.height() - target.top()) / viewport.height();
    qreal ymax = -1 + 2 * (viewport.height() - target.bottom()) / viewport.height();

    QVector2D va(xmin, ymin);
    QVector2D vb(xmax, ymin);
    QVector2D vc(xmax, ymax);
    QVector2D vd(xmin, ymax);

    QVector2D vertexCoords[] =
    {
        va, vb, vd, vd, vb, vc
    };

    QVector2D ta, tb, tc, td;

    QRectF s = source;
    if (s.isNull()) {
        s = QRectF(0, 0, 1, 1);
        ta = QVector2D(s.left(), s.top());
        tb = QVector2D(s.right(), s.top());
        tc = QVector2D(s.right(), s.bottom());
        td = QVector2D(s.left(), s.bottom());
    } else {
        ta = QVector2D(s.left(), s.top());
        tb = QVector2D(s.right(), s.top());
        tc = QVector2D(s.right(), s.bottom());
        td = QVector2D(s.left(), s.bottom());
    }

    QVector2D texCoords[] =
    {
        ta, tb, td, td, tb, tc
    };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    program->bind();
    program->setUniformValue(opacityUniform, GLfloat(alpha));

    program->enableAttributeArray(vertexAttr);
    program->setAttributeArray(vertexAttr, vertexCoords);
    program->enableAttributeArray(texCoordAttr);
    program->setAttributeArray(texCoordAttr, texCoords);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    program->disableAttributeArray(vertexAttr);
    program->disableAttributeArray(texCoordAttr);
}

int isPowerOfTwo (unsigned int x)
{
      return !(x & (x - 1));
}

bool canUseMipmaps(const QSize &size)
{
    static bool initialized = false;
    static bool supportsNonPowerOfTwoMipmaps = true;
    if (!initialized) {
        printf("VENDOR string: %s\n", glGetString(GL_VENDOR));
        printf("RENDERER string: %s\n", glGetString(GL_RENDERER));

        supportsNonPowerOfTwoMipmaps = !QByteArray(reinterpret_cast<const char *>(glGetString(GL_RENDERER))).contains("Tegra 3");
        initialized = true;

        printf("Supports non-power-of-two mipmaps: %d\n", int(supportsNonPowerOfTwoMipmaps));
    }

    return supportsNonPowerOfTwoMipmaps || (isPowerOfTwo(size.width()) && isPowerOfTwo(size.height()));
}

GLuint generateTexture(const QImage &image, bool mipmaps, bool repeat)
{
    mipmaps = mipmaps && canUseMipmaps(image.size());

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D,  0, GL_RGBA, image.width(), image.height(), 0, GL_RGBA,
        GL_UNSIGNED_BYTE, 0);
    updateSubImage(id, image, image.rect(), mipmaps);

    if (mipmaps) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);

    return id;
}

void updateSubImage(GLuint texture, const QImage &image, const QRect &rect, bool mipmaps)
{
    mipmaps = mipmaps && canUseMipmaps(image.size());

    QVector<uchar> data;
    for (int i = rect.y(); i <= rect.bottom(); ++i) {
        QRgb *p = (QRgb *)image.scanLine(i);
        for (int j = rect.x(); j <= rect.right(); ++j) {
            data << qRed(p[j]);
            data << qGreen(p[j]);
            data << qBlue(p[j]);
            data << qAlpha(p[j]);
        }
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D,  0, rect.x(), rect.y(), rect.width(), rect.height(), GL_RGBA,
        GL_UNSIGNED_BYTE, data.constData());

    if (mipmaps)
        QOpenGLFunctions(QOpenGLContext::currentContext()).glGenerateMipmap(GL_TEXTURE_2D);
}

QOpenGLShaderProgram *generateShaderProgram(QObject *parent, QByteArray vsrc, QByteArray fsrc)
{
    QOpenGLShaderProgram *program = new QOpenGLShaderProgram(parent);

    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, program);
    vshader->compileSourceCode(vsrc);
    if (!vshader->compileSourceCode(vsrc))
        qFatal("Error in vertex src:\n%s\n", vsrc.constData());

    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, program);
    if (!fshader->compileSourceCode(fsrc))
        qFatal("Error in fragment src:\n%s\n", fsrc.constData());

    program->addShader(vshader);
    program->addShader(fshader);
    if (!program->link())
        qFatal("Error linking:\n%s\n%s\n", vsrc.constData(), fsrc.constData());

    return program;
}

namespace {

QVector3D offsets[][4] =
{
    { // Ceiling
        QVector3D(0, 1, 1),
        QVector3D(1, 1, 1),
        QVector3D(1, 1, 0),
        QVector3D(0, 1, 0)
    },
    { // Floor
        QVector3D(0, 0, 0),
        QVector3D(1, 0, 0),
        QVector3D(1, 0, 1),
        QVector3D(0, 0, 1)
    },
    { // North
        QVector3D(0, 1, 0),
        QVector3D(1, 1, 0),
        QVector3D(1, 0, 0),
        QVector3D(0, 0, 0)
    },
    { // South
        QVector3D(0, 0, 1),
        QVector3D(1, 0, 1),
        QVector3D(1, 1, 1),
        QVector3D(0, 1, 1)
    },
    { // East
        QVector3D(1, 1, 0),
        QVector3D(1, 1, 1),
        QVector3D(1, 0, 1),
        QVector3D(1, 0, 0)
    },
    { // West
        QVector3D(0, 0, 0),
        QVector3D(0, 0, 1),
        QVector3D(0, 1, 1),
        QVector3D(0, 1, 0),
    }
};

}

QVector<QVector3D> tile(int x, int z, TileType type, QVector3D scale, QVector3D dim)
{
    QVector<QVector3D> result;
    for (int i = 0; i < 4; ++i)
        result << QVector3D(x, 0, z) * scale + offsets[type][i] * scale * dim;
    return result;
}

bool useSimpleShading()
{
    static bool initialized = false;
    static bool simple = false;
    if (!initialized) {
        simple = QCoreApplication::arguments().contains(QLatin1String("--simple-shading"));
        initialized = true;
    }
    return simple;
}

bool fpsDebug()
{
    static bool initialized = false;
    static bool fpsDebug = false;
    if (!initialized) {
        fpsDebug = QCoreApplication::arguments().contains(QLatin1String("--show-fps"));
        initialized = true;
    }
    return fpsDebug;
}

