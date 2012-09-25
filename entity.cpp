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

#include "entity.h"

#include "map.h"
#include "camera.h"
#include "common.h"

#include <QLineF>
#include <QImage>
#include <QPainter>

#include <qopengl.h>
#include <QOpenGLShaderProgram>

const QImage toAlpha(const QImage &image)
{
    if (image.isNull())
        return image;
    QRgb alpha = image.pixel(0, 0);
    QImage result = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QRgb *data = reinterpret_cast<QRgb *>(result.bits());
    int size = image.width() * image.height();
    for (int i = 0; i < size; ++i)
        if (data[i] == alpha)
            data[i] = 0;
    return result;
}

Entity::Entity(QObject *parent)
    : QObject(parent)
    , m_walking(false)
    , m_animationIndex(0)
    , m_angleIndex(0)
{
    startTimer(300);
}

static QVector<QImage> loadSoldierImages()
{
    QVector<QImage> images;
    for (int i = 1; i <= 40; ++i) {
        QImage image(QString("soldier/O%0.png").arg(i, 2, 10, QLatin1Char('0')));
        images << toAlpha(image.convertToFormat(QImage::Format_RGB32));
    }
    return images;
}

static inline int mod(int x, int y)
{
    return ((x % y) + y) % y;
}

QPointF to2d(const QVector3D &v) { return QPointF(v.x(), v.z()); }
QVector3D to3d(const QPointF &p) { return QVector3D(p.x(), 0, p.y()); }

void Entity::updateTransform(const Camera &camera)
{
    qreal angleToCamera = QLineF(to2d(m_pos), to2d(camera.pos())).angle();
    qreal angle = QLineF(QPointF(), to2d(m_dir)).angle();
    int cameraAngleIndex = mod(qRound(angleToCamera + 22.5), 360) / 45;

    m_angleIndex = mod(qRound(cameraAngleIndex * 45 - angle + 22.5), 360) / 45;

    QVector3D delta = to3d(QLineF::fromPolar(0.18 * m_scale, 270.1 + 45 * cameraAngleIndex).p2());
    m_a = m_pos - delta;
    m_b = m_pos + delta;
}

void Entity::timerEvent(QTimerEvent *)
{
    ++m_animationIndex;
}

void Entity::render(const Map &map, const Camera &camera) const
{
    m_program->bind();

    m_program->setUniformValue(m_matrixUniform, camera.viewProjectionMatrix());

    QVector3D up(0, 0.65 * m_scale, 0);

    QVector3D va = m_a;
    QVector3D vb = m_a + up;
    QVector3D vc = m_b + up;
    QVector3D vd = m_b;

    qreal dx = 1. / m_textureSize.width();
    qreal dy = 1. / m_textureSize.height();

    int index = m_angleIndex;
    if (m_walking)
        index += 8 + 8 * (m_animationIndex % 4);

    qreal tx1 = (m_tileWidth * (index % m_tileMod) + 1) * dx;
    qreal tx2 = tx1 + (m_tileWidth - 2) * dx;
    qreal ty1 = (m_tileHeight * (index / m_tileMod) + 1) * dy;
    qreal ty2 = ty1 + (m_tileHeight - 2) * dy;

    QVector2D ta(tx2, ty2);
    QVector2D tb(tx2, ty1);
    QVector2D tc(tx1, ty1);
    QVector2D td(tx1, ty2);

    QVector<QVector3D> vertexBuffer;
    vertexBuffer << va << vb << vd << vd << vb << vc;
    QVector<QVector3D> texBuffer;
    texBuffer << ta << tb << td << td << tb << tc;

    m_program->enableAttributeArray(m_vertexAttr);
    m_program->setAttributeArray(m_vertexAttr, vertexBuffer.constData());
    m_program->enableAttributeArray(m_texAttr);
    m_program->setAttributeArray(m_texAttr, texBuffer.constData());

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);

    m_program->disableAttributeArray(m_texAttr);
    m_program->disableAttributeArray(m_vertexAttr);
}

void Entity::initialize()
{
    QByteArray vsrc =
        "attribute highp vec4 vertexAttr;\n"
        "attribute highp vec2 texAttr;\n"
        "uniform mediump mat4 matrix;\n"
        "varying highp vec2 texCoord;\n"
        "void main(void)\n"
        "{\n"
        "    texCoord = texAttr;\n"
        "    gl_Position = matrix * vertexAttr;\n"
        "}\n";

    QByteArray fsrc =
        "uniform sampler2D texture;\n"
        "varying highp vec2 texCoord;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = texture2D(texture, texCoord);\n"
        "}\n";

    m_program = generateShaderProgram(this, vsrc, fsrc);

    m_vertexAttr = m_program->attributeLocation("vertexAttr");
    m_texAttr = m_program->attributeLocation("texAttr");
    m_matrixUniform = m_program->uniformLocation("matrix");

    QVector<QImage> images = loadSoldierImages();
    int w = images.first().width() + 2;
    int h = images.first().height() + 2;

    m_tileMod = (images.size() + 3) / 4;

    QImage image(m_tileMod * w, 4 * h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    for (int i = 0; i < images.size(); ++i)
        p.drawImage(w * (i % m_tileMod) + 1, h * (i / m_tileMod) + 1, images.at(i));
    p.end();

    qDebug() << "Initialized soldier image" << image.size();

    m_tileWidth = w;
    m_tileHeight = h;

    m_texture = generateTexture(image, false, false);
    m_textureSize = image.size();
}
