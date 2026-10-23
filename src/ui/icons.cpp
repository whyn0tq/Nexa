#include "icons.h"

#include "hoststore.h"
#include "theme.h"

#include <QApplication>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QtMath>

#include <algorithm>
#include <cmath>

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

QPainterPath roundedPolygon(std::initializer_list<QPointF> list, qreal radius)
{
    const QVector<QPointF> points(list);
    const qsizetype n = points.size();
    QPainterPath path;
    for (qsizetype i = 0; i < n; ++i) {
        const QPointF prev = points.at((i + n - 1) % n);
        const QPointF at = points.at(i);
        const QPointF next = points.at((i + 1) % n);
        const QLineF in(at, prev);
        const QLineF out(at, next);
        const QPointF a = in.pointAt(std::min(0.5, radius / in.length()));
        const QPointF b = out.pointAt(std::min(0.5, radius / out.length()));
        i == 0 ? path.moveTo(a) : path.lineTo(a);
        path.quadTo(at, b);
    }
    path.closeSubpath();
    return path;
}

QPainterPath strokeOf(const QPainterPath &path, qreal width)
{
    QPainterPathStroker stroker;
    stroker.setWidth(width);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path);
}

QPainterPath alertTriangle()
{
    return roundedPolygon({{12, 3.6}, {21.2, 19.6}, {2.8, 19.6}}, 2.2);
}

QPainterPath trashCan()
{
    return roundedPolygon({{6.3, 7}, {17.7, 7}, {16.8, 19.8}, {7.2, 19.8}}, 1.8);
}

QPainterPath databaseBody()
{
    QPainterPath path;
    path.moveTo(4.5, 6);
    path.arcTo(QRectF(4.5, 3.5, 15, 5), 180, -180);
    path.lineTo(19.5, 18);
    path.arcTo(QRectF(4.5, 15.5, 15, 5), 0, -180);
    path.closeSubpath();
    return path;
}

QPainterPath filePage()
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
    return path;
}

QPainterPath gamepadBody()
{
    QPainterPath path;
    path.moveTo(7, 5.5);
    path.lineTo(17, 5.5);
    path.quadTo(20.6, 5.5, 21, 9);
    path.lineTo(21.6, 15.2);
    path.quadTo(21.9, 18.5, 19, 18.5);
    path.quadTo(17.6, 18.5, 16.6, 17.4);
    path.lineTo(15.6, 16.3);
    path.quadTo(15, 15.6, 14, 15.6);
    path.lineTo(10, 15.6);
    path.quadTo(9, 15.6, 8.4, 16.3);
    path.lineTo(7.4, 17.4);
    path.quadTo(6.4, 18.5, 5, 18.5);
    path.quadTo(2.1, 18.5, 2.4, 15.2);
    path.lineTo(3, 9);
    path.quadTo(3.4, 5.5, 7, 5.5);
    path.closeSubpath();
    return path;
}

QPainterPath gearTeeth()
{
    const auto at = [](qreal degrees, qreal radius) {
        const qreal r = qDegreesToRadians(degrees);
        return QPointF(12 + radius * std::cos(r), 12 + radius * std::sin(r));
    };
    QPainterPath path;
    for (int tooth = 0; tooth < 8; ++tooth) {
        const qreal a = tooth * 45.0;
        const QPointF start = at(a - 22.5, 6.6);
        tooth == 0 ? path.moveTo(start) : path.lineTo(start);
        path.lineTo(at(a - 13, 6.6));
        path.lineTo(at(a - 8.5, 9));
        path.lineTo(at(a + 8.5, 9));
        path.lineTo(at(a + 13, 6.6));
    }
    path.closeSubpath();
    return path;
}

QPainterPath houseBody()
{
    return roundedPolygon({{12, 4.2}, {18.5, 9.6}, {18.5, 19.5}, {5.5, 19.5}, {5.5, 9.6}}, 1.6);
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
    QPainterPath path = filePage();
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
        p = houseBody();
        p.addPath(polyline({{10.2, 19.5}, {10.2, 15}, {13.8, 15}, {13.8, 19.5}}));
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
        p.addPath(polyline({{9.3, 7}, {9.3, 5.6}, {10.4, 4.5}, {13.6, 4.5}, {14.7, 5.6}, {14.7, 7}}));
        p.addPath(trashCan());
        p.moveTo(9.6, 10.5);
        p.lineTo(9.9, 16.5);
        p.moveTo(12, 10.5);
        p.lineTo(12, 16.5);
        p.moveTo(14.4, 10.5);
        p.lineTo(14.1, 16.5);
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
        p = alertTriangle();
        p.moveTo(12, 9.8);
        p.lineTo(12, 13.8);
        p.addPath(circle(12, 16.6, 0.35));
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
    case Icon::Gear:
        p = gearTeeth();
        p.addPath(circle(12, 12, 2.8));
        return p;
    case Icon::Globe:
        p = circle(12, 12, 8.5);
        p.addEllipse(QRectF(8.2, 3.5, 7.6, 17));
        p.moveTo(3.5, 12);
        p.lineTo(20.5, 12);
        p.moveTo(5.2, 7.6);
        p.quadTo(12, 9.4, 18.8, 7.6);
        p.moveTo(5.2, 16.4);
        p.quadTo(12, 14.6, 18.8, 16.4);
        return p;
    case Icon::Gamepad:
        p = gamepadBody();
        p.moveTo(6, 10.5);
        p.lineTo(10, 10.5);
        p.moveTo(8, 8.5);
        p.lineTo(8, 12.5);
        p.addPath(circle(15, 11.5, 0.45));
        p.addPath(circle(18, 9.5, 0.45));
        return p;
    case Icon::Database: {
        const QRectF top(4.5, 3.5, 15, 5);
        const QRectF middle(4.5, 9.5, 15, 5);
        const QRectF bottom(4.5, 15.5, 15, 5);
        Q_UNUSED(bottom);
        p = databaseBody();
        p.arcMoveTo(top, 180);
        p.arcTo(top, 180, 180);
        p.arcMoveTo(middle, 180);
        p.arcTo(middle, 180, 180);
        return p;
    }
    case Icon::HardDrive:
        p = roundedRect(3, 6.5, 18, 11, 3);
        p.moveTo(6.5, 13.5);
        p.lineTo(10.5, 13.5);
        p.addPath(circle(17, 13.5, 0.45));
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

bool fillShape(Icon icon, qreal stroke, QPainterPath &result)
{
    QPainterPath body;
    QPainterPath cut;
    QPainterPath hole;
    const auto solid = [&body, stroke](const QPainterPath &closed) {
        body.addPath(closed);
        body.addPath(strokeOf(closed, stroke));
    };
    switch (icon) {
    case Icon::Server:
        solid(roundedRect(4, 4.5, 16, 6.5, 2.5));
        solid(roundedRect(4, 13, 16, 6.5, 2.5));
        cut.moveTo(2, 12);
        cut.lineTo(22, 12);
        cut.addPath(circle(7.8, 7.75, 0.4));
        cut.addPath(circle(7.8, 16.25, 0.4));
        break;
    case Icon::Computer:
        solid(roundedRect(3.5, 4.5, 17, 11.5, 2.5));
        body.addPath(strokeOf(polyline({{9, 20}, {15, 20}}), stroke));
        body.addPath(strokeOf(polyline({{12, 16}, {12, 20}}), stroke));
        break;
    case Icon::Globe:
        solid(circle(12, 12, 8.5));
        cut.addEllipse(QRectF(8.2, 3.5, 7.6, 17));
        cut.moveTo(3.5, 12);
        cut.lineTo(20.5, 12);
        break;
    case Icon::Gamepad:
        solid(gamepadBody());
        cut.moveTo(6, 10.5);
        cut.lineTo(10, 10.5);
        cut.moveTo(8, 8.5);
        cut.lineTo(8, 12.5);
        cut.addPath(circle(15, 11.5, 0.45));
        cut.addPath(circle(18, 9.5, 0.45));
        break;
    case Icon::Database:
        solid(databaseBody());
        cut.arcMoveTo(QRectF(4.5, 3.5, 15, 5), 180);
        cut.arcTo(QRectF(4.5, 3.5, 15, 5), 180, 180);
        cut.arcMoveTo(QRectF(4.5, 9.5, 15, 5), 180);
        cut.arcTo(QRectF(4.5, 9.5, 15, 5), 180, 180);
        break;
    case Icon::HardDrive:
        solid(roundedRect(3, 6.5, 18, 11, 3));
        cut.moveTo(6.5, 13.5);
        cut.lineTo(10.5, 13.5);
        cut.addPath(circle(17, 13.5, 0.45));
        break;
    case Icon::Folder:
        solid(folderOutline());
        break;
    case Icon::File:
        solid(filePage());
        cut.addPath(polyline({{14, 3}, {14, 8}, {19, 8}}));
        break;
    case Icon::Gear:
        solid(gearTeeth());
        hole = circle(12, 12, 3.2);
        break;
    case Icon::Check:
        solid(circle(12, 12, 9));
        cut.addPath(polyline({{8, 12.4}, {10.8, 15.2}, {16.2, 9.2}}));
        break;
    case Icon::Alert:
        solid(alertTriangle());
        cut.moveTo(12, 9.8);
        cut.lineTo(12, 13.8);
        cut.addPath(circle(12, 16.6, 0.35));
        break;
    case Icon::Info:
        solid(circle(12, 12, 8.5));
        cut.moveTo(12, 11);
        cut.lineTo(12, 16);
        cut.addPath(circle(12, 8, 0.35));
        break;
    case Icon::Shield: {
        QPainterPath outline;
        outline.moveTo(12, 3.5);
        outline.lineTo(19, 6.5);
        outline.lineTo(19, 11.5);
        outline.cubicTo(19, 16, 15.8, 19, 12, 20.5);
        outline.cubicTo(8.2, 19, 5, 16, 5, 11.5);
        outline.lineTo(5, 6.5);
        outline.closeSubpath();
        solid(outline);
        cut.addPath(polyline({{9, 12}, {11.2, 14.2}, {15.2, 10}}));
        break;
    }
    case Icon::Trash:
        solid(trashCan());
        body.addPath(strokeOf(polyline({{4.5, 7}, {19.5, 7}}), stroke));
        body.addPath(strokeOf(polyline({{9.3, 7}, {9.3, 5.6}, {10.4, 4.5}, {13.6, 4.5},
                                        {14.7, 5.6}, {14.7, 7}}),
                              stroke));
        cut.moveTo(9.6, 10.5);
        cut.lineTo(9.9, 16.5);
        cut.moveTo(12, 10.5);
        cut.lineTo(12, 16.5);
        cut.moveTo(14.4, 10.5);
        cut.lineTo(14.1, 16.5);
        break;
    case Icon::Home:
        solid(houseBody());
        hole = roundedRect(10.2, 15, 3.6, 6, 0.8);
        break;
    default:
        return false;
    }
    body.setFillRule(Qt::WindingFill);
    result = body.simplified();
    if (!cut.isEmpty())
        result = result.subtracted(strokeOf(cut, stroke));
    if (!hole.isEmpty())
        result = result.subtracted(hole);
    return true;
}

qreal strokeFor(qreal size)
{
    return std::clamp(size * 0.085, 1.2, 2.2) / (size / 24.0);
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
    const qreal size = std::min(rect.width(), rect.height());
    const qreal scale = size / 24.0;
    painter.scale(scale, scale);
    if (strokeWidth <= 0)
        strokeWidth = strokeFor(size);
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

void paintFilled(QPainter &painter, Icon icon, const QRectF &rect, const QColor &color)
{
    const qreal size = std::min(rect.width(), rect.height());
    QPainterPath path;
    if (!fillShape(icon, strokeFor(size), path)) {
        paint(painter, icon, rect, color);
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    painter.scale(size / 24.0, size / 24.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(path);
    painter.restore();
}

QPixmap pixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio, bool filled)
{
    const qreal ratio = devicePixelRatio > 0 ? devicePixelRatio : qApp->devicePixelRatio();
    QPixmap result(qCeil(size * ratio), qCeil(size * ratio));
    result.setDevicePixelRatio(ratio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    const QRectF rect(0, 0, size, size);
    filled ? paintFilled(painter, icon, rect, color) : paint(painter, icon, rect, color);
    return result;
}

Icon hostIcon(HostKind kind)
{
    switch (kind) {
    case HostKind::Server:
        return Icon::Server;
    case HostKind::Web:
        return Icon::Globe;
    case HostKind::Game:
        return Icon::Gamepad;
    case HostKind::Database:
        return Icon::Database;
    case HostKind::Storage:
        return Icon::HardDrive;
    case HostKind::Desktop:
        return Icon::Computer;
    }
    return Icon::Server;
}

QString hostKindName(HostKind kind)
{
    switch (kind) {
    case HostKind::Server:
        break;
    case HostKind::Web:
        return QCoreApplication::translate("HostKind", "Веб-сервер");
    case HostKind::Game:
        return QCoreApplication::translate("HostKind", "Игровой сервер");
    case HostKind::Database:
        return QCoreApplication::translate("HostKind", "База данных");
    case HostKind::Storage:
        return QCoreApplication::translate("HostKind", "Хранилище, NAS");
    case HostKind::Desktop:
        return QCoreApplication::translate("HostKind", "Компьютер");
    }
    return QCoreApplication::translate("HostKind", "Сервер");
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
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(48, 118, 190));
    painter.drawPath(back);

    QPainterPath front;
    front.addRoundedRect(QRectF(2.5, 8.5, 19, 11), 2.5, 2.5);
    painter.setBrush(QColor(92, 162, 230));
    painter.drawPath(front);
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

    QPainterPath tilePath;
    constexpr int steps = 360;
    constexpr qreal exponent = 5.0;
    for (int i = 0; i <= steps; ++i) {
        const qreal t = 2 * M_PI * i / steps;
        const qreal c = std::cos(t);
        const qreal sn = std::sin(t);
        const QPointF p(50 + 47 * (c < 0 ? -1 : 1) * std::pow(std::abs(c), 2 / exponent),
                        50 + 47 * (sn < 0 ? -1 : 1) * std::pow(std::abs(sn), 2 / exponent));
        i == 0 ? tilePath.moveTo(p) : tilePath.lineTo(p);
    }
    tilePath.closeSubpath();

    const QColor tile(24, 24, 27);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tile);
    painter.drawPath(tilePath);
    painter.setPen(QPen(QColor(255, 255, 255, 40), std::max(1.0, 1.0 / s)));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(tilePath);

    const QPointF a(31, 72.3);
    const QPointF b(31, 27.7);
    const QPointF c(68.9, 72.3);
    const QPointF d(68.9, 27.7);
    QPainterPath mark;
    mark.moveTo(a);
    mark.lineTo(b);
    mark.lineTo(c);
    mark.lineTo(d);
    const bool small = rect.width() < 28;
    painter.setPen(QPen(Theme::Text, small ? 9.5 : 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(mark);

    painter.setPen(Qt::NoPen);
    for (const QPointF &node : {a, d}) {
        painter.setBrush(tile);
        painter.drawEllipse(node, 10, 10);
        painter.setBrush(Theme::Accent);
        painter.drawEllipse(node, 6.6, 6.6);
    }
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
