#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

class QPainter;
class QRectF;

enum class Icon {
    Back,
    Forward,
    Up,
    Refresh,
    Home,
    Plus,
    Close,
    Minimize,
    Maximize,
    Restore,
    Upload,
    Download,
    Search,
    Server,
    Folder,
    FolderPlus,
    File,
    FilePlus,
    Trash,
    Edit,
    Rename,
    Link,
    Check,
    Alert,
    Info,
    Shield,
    Key,
    Eye,
    Transfers,
    ChevronDown,
    ChevronUp,
    ChevronRight,
    More,
    Command,
    Computer,
    Terminal,
    Plug,
    Copy,
    Bolt,
    Stop,
    Retry,
    ExternalLink,
    Sliders,
};

namespace Icons {

void paint(QPainter &painter, Icon icon, const QRectF &rect, const QColor &color,
           qreal strokeWidth = 1.7);
QPixmap pixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio = 0);
QIcon icon(Icon icon, const QColor &color, int size = 16);

void paintFolder(QPainter &painter, const QRectF &rect, bool open = false);
void paintFile(QPainter &painter, const QRectF &rect, const QString &suffix);

void paintLogo(QPainter &painter, const QRectF &rect);
QIcon appIcon();
QImage logoImage(int size);

}
