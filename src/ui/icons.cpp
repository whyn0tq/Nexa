#include "icons.h"

#include "theme.h"

#include <QApplication>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace Icons {

namespace {
QPainterPath roundedRect(qreal x, qreal y, qreal w, qreal h, qreal r)
{
    QPainterPath path;
    path.addRoundedRect(QRectF(x, y, w, h), r, r);
    return path;
}

QPainterPath polyline(std::initializer_list<QPointF> points)
{
    QPainterPath path;
    bool first = true;
    for (const QPointF &point : points) {
        if (first)
            path.moveTo(point);
        else
            path.lineTo(point);
        first = false;
    }
    return path;
}

QPainterPath arrow(QPointF from, QPointF to, qreal head)
{
    QPainterPath path;
    path.moveTo(from);
    path.lineTo(to);
    QLineF line(to, from);
    QLineF left = line;
    left.setLength(head);
    left.setAngle(line.angle() + 45);
    QLineF right = line;
    right.setLength(head);
    right.setAngle(line.angle() - 45);
    path.moveTo(left.p2());
    path.lineTo(to);
    path.lineTo(right.p2());
    return path;
}

QPainterPath circle(qreal cx, qreal cy, qreal r)
{
    QPainterPath path;
    path.addEllipse(QPointF(cx, cy), r, r);
    return path;
}

QPainterPath folderOutline()
{
    QPainterPath path;
    path.moveTo(3, 7.5);
    path.quadTo(3, 5, 5.5, 5);
    path.lineTo(9.2, 5);
    path.lineTo(11.2, 7);
    path.lineTo(18.5, 7);
    path.quadTo(21, 7, 21, 9.5);
    path.lineTo(21, 16.5);
    path.quadTo(21, 19, 18.5, 19);
    path.lineTo(5.5, 19);
    path.quadTo(3, 19, 3, 16.5);
    path.closeSubpath();
    return path;
}

QPainterPath fileOutline()
{
    QPainterPath path;
    path.moveTo(14, 3);
    path.lineTo(7.5, 3);
    path.quadTo(5, 3, 5, 5.5);
    path.lineTo(5, 18.5);
    path.quadTo(5, 21, 7.5, 21);
    path.lineTo(16.5, 21);
    path.quadTo(19, 21, 19, 18.5);
    path.lineTo(19, 8);
    path.closeSubpath();
    path.moveTo(14, 3);
    path.lineTo(14, 8);
    path.lineTo(19, 8);
    return path;
}

QPainterPath shape(Icon icon, bool &filled)
{
    filled = false;
    QPainterPath p;
    switch (icon) {
    case Icon::Back:
        return polyline({{14.5, 6}, {8.5, 12}, {14.5, 18}});
    case Icon::Forward:
        return polyline({{9.5, 6}, {15.5, 12}, {9.5, 18}});
    case Icon::ChevronDown:
        return polyline({{6.5, 9.5}, {12, 15}, {17.5, 9.5}});
    case Icon::ChevronUp:
        return polyline({{6.5, 14.5}, {12, 9}, {17.5, 14.5}});
    case Icon::ChevronRight:
        return polyline({{10, 7}, {15, 12}, {10, 17}});
    case Icon::Up:
        return arrow({12, 19}, {12, 5.5}, 5.5);
    case Icon::Upload:
        p = arrow({12, 15}, {12, 4}, 4.5);
        p.addPath(polyline({{4.5, 14.5}, {4.5, 17.5}, {6.5, 19.5}, {17.5, 19.5}, {19.5, 17.5},
                            {19.5, 14.5}}));
        return p;
    case Icon::Download:
        p = arrow({12, 4}, {12, 15}, 4.5);
        p.addPath(polyline({{4.5, 14.5}, {4.5, 17.5}, {6.5, 19.5}, {17.5, 19.5}, {19.5, 17.5},
                            {19.5, 14.5}}));
        return p;
    case Icon::Transfers:
        p = arrow({8, 19}, {8, 5}, 3.8);
        p.addPath(arrow({16, 5}, {16, 19}, 3.8));
        return p;
    case Icon::Refresh: {
        p.arcMoveTo(QRectF(4.5, 4.5, 15, 15), 60);
        p.arcTo(QRectF(4.5, 4.5, 15, 15), 60, 290);
        p.moveTo(17.2, 3.6);
        p.lineTo(16.3, 7.9);
        p.lineTo(20.5, 8.3);
        return p;
    }
    case Icon::Retry: {
        p.arcMoveTo(QRectF(4.5, 4.5, 15, 15), 120);
        p.arcTo(QRectF(4.5, 4.5, 15, 15), 120, -290);
        p.moveTo(6.8, 3.6);
        p.lineTo(7.7, 7.9);
        p.lineTo(3.5, 8.3);
        return p;
    }
    case Icon::Home:
        p = polyline({{4, 11}, {12, 4.5}, {20, 11}});
        p.addPath(polyline({{6, 9.5}, {6, 18}, {7.5, 19.5}, {16.5, 19.5}, {18, 18}, {18, 9.5}}));
        p.addPath(polyline({{10, 19.5}, {10, 14.5}, {14, 14.5}, {14, 19.5}}));
        return p;
    case Icon::Plus:
        p.moveTo(12, 5);
        p.lineTo(12, 19);
        p.moveTo(5, 12);
        p.lineTo(19, 12);
        return p;
    case Icon::Close:
        p.moveTo(6.5, 6.5);
        p.lineTo(17.5, 17.5);
        p.moveTo(17.5, 6.5);
        p.lineTo(6.5, 17.5);
        return p;
    case Icon::Minimize:
        p.moveTo(6, 12);
        p.lineTo(18, 12);
        return p;
    case Icon::Maximize:
        return roundedRect(6, 6, 12, 12, 3);
    case Icon::Restore:
        p = roundedRect(5.5, 8.5, 10, 10, 2.5);
        p.addPath(polyline({{8.5, 5.5}, {16, 5.5}, {18.5, 8}, {18.5, 15.5}}));
        return p;
    case Icon::Search:
        p = circle(10.5, 10.5, 6);
        p.moveTo(15, 15);
        p.lineTo(19.5, 19.5);
        return p;
    case Icon::Server:
        p = roundedRect(4, 4.5, 16, 6.5, 2.5);
        p.addPath(roundedRect(4, 13, 16, 6.5, 2.5));
        p.addPath(circle(7.8, 7.75, 0.4));
        p.addPath(circle(7.8, 16.25, 0.4));
        return p;
    case Icon::Computer:
        p = roundedRect(3.5, 4.5, 17, 11.5, 2.5);
        p.moveTo(9, 20);
        p.lineTo(15, 20);
        p.moveTo(12, 16);
        p.lineTo(12, 20);
        return p;
    case Icon::Folder:
        return folderOutline();
    case Icon::FolderPlus:
        p = folderOutline();
        p.moveTo(12, 10);
        p.lineTo(12, 16);
        p.moveTo(9, 13);
        p.lineTo(15, 13);
        return p;
    case Icon::File:
        return fileOutline();
    case Icon::FilePlus:
        p = fileOutline();
        p.moveTo(12, 11.5);
        p.lineTo(12, 17.5);
        p.moveTo(9, 14.5);
        p.lineTo(15, 14.5);
        return p;
    case Icon::Trash:
        p.moveTo(4.5, 7);
        p.lineTo(19.5, 7);
        p.addPath(polyline({{9.5, 7}, {9.5, 4.5}, {14.5, 4.5}, {14.5, 7}}));
        p.addPath(polyline({{6.5, 7}, {7.3, 18.2}, {8.8, 19.5}, {15.2, 19.5}, {16.7, 18.2},
                            {17.5, 7}}));
        p.moveTo(10.3, 10.5);
        p.lineTo(10.3, 16);
        p.moveTo(13.7, 10.5);
        p.lineTo(13.7, 16);
        return p;
    case Icon::Rename:
        p = roundedRect(3, 7.5, 18, 9, 3);
        p.moveTo(15, 4.5);
        p.lineTo(15, 19.5);
        p.moveTo(13, 4.5);
        p.lineTo(17, 4.5);
        p.moveTo(13, 19.5);
        p.lineTo(17, 19.5);
        return p;
    case Icon::Edit:
        p = polyline({{15.5, 5}, {19, 8.5}, {9, 18.5}, {4.5, 19.5}, {5.5, 15}, {15.5, 5}});
        p.moveTo(13.5, 7);
        p.lineTo(17, 10.5);
        return p;
    case Icon::Link:
        p = roundedRect(3, 9, 9, 6, 3);
        p.addPath(roundedRect(12, 9, 9, 6, 3));
        p.moveTo(9, 12);
        p.lineTo(15, 12);
        return p;
    case Icon::Check:
        return polyline({{5, 12.5}, {10, 17.5}, {19, 7}});
    case Icon::Alert:
        p = polyline({{12, 4}, {21, 19.5}, {3, 19.5}, {12, 4}});
        p.moveTo(12, 10);
        p.lineTo(12, 14);
        p.addPath(circle(12, 16.8, 0.35));
        return p;
    case Icon::Info:
        p = circle(12, 12, 8.5);
        p.moveTo(12, 11);
        p.lineTo(12, 16);
        p.addPath(circle(12, 8, 0.35));
        return p;
    case Icon::Shield:
        p.moveTo(12, 3.5);
        p.lineTo(19, 6.5);
        p.lineTo(19, 11.5);
        p.cubicTo(19, 16, 15.8, 19, 12, 20.5);
        p.cubicTo(8.2, 19, 5, 16, 5, 11.5);
        p.lineTo(5, 6.5);
        p.closeSubpath();
        p.addPath(polyline({{9, 12}, {11.2, 14.2}, {15.2, 10}}));
        return p;
    case Icon::Key:
        p = circle(8, 15.5, 4);
        p.moveTo(10.9, 12.6);
        p.lineTo(19, 4.5);
        p.moveTo(16, 7.5);
        p.lineTo(18.5, 10);
        p.moveTo(13.8, 9.7);
        p.lineTo(15.8, 11.7);
        return p;
    case Icon::Eye:
        p.moveTo(2.5, 12);
        p.cubicTo(5, 7, 8.5, 5.5, 12, 5.5);
        p.cubicTo(15.5, 5.5, 19, 7, 21.5, 12);
        p.cubicTo(19, 17, 15.5, 18.5, 12, 18.5);
        p.cubicTo(8.5, 18.5, 5, 17, 2.5, 12);
        p.addPath(circle(12, 12, 3));
        return p;
    case Icon::More:
        filled = true;
        p = circle(6, 12, 1.6);
        p.addPath(circle(12, 12, 1.6));
        p.addPath(circle(18, 12, 1.6));
        return p;
    case Icon::Command:
        p = roundedRect(3.5, 5, 17, 14, 3.5);
        p.addPath(polyline({{7.5, 10}, {10, 12.5}, {7.5, 15}}));
        p.moveTo(12, 15);
        p.lineTo(16.5, 15);
        return p;
    case Icon::Terminal:
        p = polyline({{5, 7}, {10, 12}, {5, 17}});
        p.moveTo(12, 17.5);
        p.lineTo(19, 17.5);
        return p;
    case Icon::Plug:
        p.moveTo(9, 3.5);
        p.lineTo(9, 7.5);
        p.moveTo(15, 3.5);
        p.lineTo(15, 7.5);
        p.addPath(polyline({{6, 7.5}, {18, 7.5}, {18, 11}, {12, 17}, {6, 11}, {6, 7.5}}));
        p.moveTo(12, 17);
        p.lineTo(12, 20.5);
        return p;
    case Icon::Copy:
        p = roundedRect(8.5, 8.5, 11.5, 11.5, 2.5);
        p.addPath(polyline({{15.5, 8.5}, {15.5, 6.5}, {13.5, 4}, {6.5, 4}, {4, 6.5}, {4, 13.5},
                            {6.5, 15.5}, {8.5, 15.5}}));
        return p;
    case Icon::Bolt:
        return polyline({{13, 3.5}, {5.5, 13.5}, {11.5, 13.5}, {10.5, 20.5}, {18.5, 10.5},
                         {12.5, 10.5}, {13, 3.5}});
    case Icon::Stop:
        p = circle(12, 12, 8.5);
        p.moveTo(6, 6);
        p.lineTo(18, 18);
        return p;
    case Icon::Sliders:
        p.moveTo(4, 7);
        p.lineTo(12, 7);
        p.moveTo(17, 7);
        p.lineTo(20, 7);
        p.addPath(circle(14.5, 7, 2.3));
        p.moveTo(4, 12);
        p.lineTo(6, 12);
        p.moveTo(11, 12);
        p.lineTo(20, 12);
        p.addPath(circle(8.5, 12, 2.3));
        p.moveTo(4, 17);
        p.lineTo(10, 17);
        p.moveTo(15, 17);
        p.lineTo(20, 17);
        p.addPath(circle(12.5, 17, 2.3));
        return p;
    case Icon::ExternalLink:
        p = polyline({{13, 4.5}, {19.5, 4.5}, {19.5, 11}});
        p.moveTo(19.5, 4.5);
        p.lineTo(11, 13);
        p.addPath(polyline({{17, 14}, {17, 17.5}, {15, 19.5}, {6.5, 19.5}, {4.5, 17.5},
                            {4.5, 9}, {6.5, 7}, {10, 7}}));
        return p;
    }
    return p;
}
}

void paint(QPainter &painter, Icon icon, const QRectF &rect, const QColor &color,
           qreal strokeWidth)
{
    bool filled = false;
    const QPainterPath path = shape(icon, filled);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    const qreal scale = std::min(rect.width(), rect.height()) / 24.0;
    painter.scale(scale, scale);
    if (filled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    } else {
        QPen pen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
    }
    painter.drawPath(path);
    painter.restore();
}

QPixmap pixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio)
{
    const qreal ratio = devicePixelRatio > 0 ? devicePixelRatio : qApp->devicePixelRatio();
    QPixmap result(qCeil(size * ratio), qCeil(size * ratio));
    result.setDevicePixelRatio(ratio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    paint(painter, icon, QRectF(0, 0, size, size), color, size <= 16 ? 1.8 : 1.7);
    return result;
}

QIcon icon(Icon value, const QColor &color, int size)
{
    QIcon result;
    result.addPixmap(pixmap(value, color, size, 1));
    result.addPixmap(pixmap(value, color, size, 2));
    return result;
}

void paintFolder(QPainter &painter, const QRectF &rect, bool open)
{
    Q_UNUSED(open);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    const qreal s = std::min(rect.width(), rect.height()) / 24.0;
    painter.scale(s, s);

    QPainterPath back;
    back.moveTo(2.5, 7);
    back.quadTo(2.5, 4.5, 5, 4.5);
    back.lineTo(9, 4.5);
    back.lineTo(11, 6.5);
    back.lineTo(19, 6.5);
    back.quadTo(21.5, 6.5, 21.5, 9);
    back.lineTo(21.5, 17);
    back.quadTo(21.5, 19.5, 19, 19.5);
    back.lineTo(5, 19.5);
    back.quadTo(2.5, 19.5, 2.5, 17);
    back.closeSubpath();
    QLinearGradient backFill(0, 4, 0, 20);
    backFill.setColorAt(0, QColor(200, 38, 64));
    backFill.setColorAt(1, QColor(130, 18, 38));
    painter.setPen(Qt::NoPen);
    painter.setBrush(backFill);
    painter.drawPath(back);

    QPainterPath front;
    front.addRoundedRect(QRectF(2.5, 8.5, 19, 11), 2.5, 2.5);
    QLinearGradient frontFill(0, 8.5, 0, 19.5);
    frontFill.setColorAt(0, QColor(255, 104, 120));
    frontFill.setColorAt(1, QColor(232, 46, 72));
    painter.setBrush(frontFill);
    painter.drawPath(front);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 0.6));
    painter.drawLine(QPointF(4.2, 9.3), QPointF(19.8, 9.3));
    painter.restore();
}

void paintFile(QPainter &painter, const QRectF &rect, const QString &suffix)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    const qreal s = std::min(rect.width(), rect.height()) / 24.0;
    painter.scale(s, s);
    QPainterPath body;
    body.moveTo(14, 2.5);
    body.lineTo(7, 2.5);
    body.quadTo(4.5, 2.5, 4.5, 5);
    body.lineTo(4.5, 19);
    body.quadTo(4.5, 21.5, 7, 21.5);
    body.lineTo(17, 21.5);
    body.quadTo(19.5, 21.5, 19.5, 19);
    body.lineTo(19.5, 8);
    body.closeSubpath();
    painter.setPen(QPen(QColor(255, 255, 255, 60), 1));
    painter.setBrush(QColor(255, 255, 255, 22));
    painter.drawPath(body);
    QPainterPath fold;
    fold.moveTo(14, 2.5);
    fold.lineTo(14, 6.5);
    fold.quadTo(14, 8, 15.5, 8);
    fold.lineTo(19.5, 8);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(fold);
    if (!suffix.isEmpty()) {
        QFont font = Theme::font(5.6, QFont::Bold);
        font.setPixelSize(6);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 150));
        painter.drawText(QRectF(4.5, 12, 15, 8), Qt::AlignCenter,
                         suffix.left(4).toUpper());
    }
    painter.restore();
}

void paintLogo(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    const qreal s = std::min(rect.width(), rect.height()) / 100.0;
    painter.scale(s, s);

    const QRectF tile(3, 3, 94, 94);
    QPainterPath tilePath;
    tilePath.addRoundedRect(tile, 26, 26);
    QLinearGradient body(0, 3, 0, 97);
    body.setColorAt(0, QColor(34, 34, 40));
    body.setColorAt(1, QColor(10, 10, 12));
    painter.setPen(Qt::NoPen);
    painter.setBrush(body);
    painter.drawPath(tilePath);

    QRadialGradient glow(QPointF(24, 92), 70);
    glow.setColorAt(0, QColor(255, 66, 86, 120));
    glow.setColorAt(1, QColor(255, 66, 86, 0));
    painter.setBrush(glow);
    painter.drawPath(tilePath);

    QLinearGradient rim(0, 3, 0, 97);
    rim.setColorAt(0, QColor(255, 255, 255, 70));
    rim.setColorAt(0.5, QColor(255, 255, 255, 14));
    rim.setColorAt(1, QColor(255, 255, 255, 26));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QBrush(rim), 1.6));
    painter.drawRoundedRect(tile.adjusted(0.8, 0.8, -0.8, -0.8), 25.2, 25.2);

    const auto drawArrow = [&painter](qreal x, qreal top, qreal bottom, bool up,
                                      const QBrush &brush) {
        QPainterPath path;
        const qreal tip = up ? top : bottom;
        const qreal tail = up ? bottom : top;
        const qreal head = 13;
        path.moveTo(x, tail);
        path.lineTo(x, tip);
        path.moveTo(x - head, tip + (up ? head : -head));
        path.lineTo(x, tip);
        path.lineTo(x + head, tip + (up ? head : -head));
        painter.setPen(QPen(brush, 9.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    };
    QLinearGradient red(0, 24, 0, 72);
    red.setColorAt(0, QColor(255, 112, 126));
    red.setColorAt(1, QColor(228, 34, 62));
    QLinearGradient silver(0, 28, 0, 76);
    silver.setColorAt(0, QColor(255, 255, 255));
    silver.setColorAt(1, QColor(170, 170, 180));
    drawArrow(38, 25, 70, true, red);
    drawArrow(62, 30, 75, false, silver);
    painter.restore();
}

QImage logoImage(int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paintLogo(painter, QRectF(0, 0, size, size));
    return image;
}

QIcon appIcon()
{
    QIcon result;
    for (const int size : {16, 20, 24, 32, 40, 48, 64, 128, 256})
        result.addPixmap(QPixmap::fromImage(logoImage(size)));
    return result;
}

}
