#include "pathbar.h"

#include "theme.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>

PathBar::PathBar(Style style, QWidget *parent)
    : QWidget(parent), m_style(style)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget;
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_crumbHost = new QWidget;
    m_crumbLayout = new QHBoxLayout(m_crumbHost);
    m_crumbLayout->setContentsMargins(0, 0, 0, 0);
    m_crumbLayout->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setWidget(m_crumbHost);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(Theme::ControlHeight);
    scroll->viewport()->setAutoFillBackground(false);
    m_crumbHost->setAutoFillBackground(false);

    m_editor = new QLineEdit;
    m_editor->setObjectName(QStringLiteral("pathInput"));
    m_editor->setClearButtonEnabled(true);
    m_editor->setPlaceholderText(style == Style::Remote
                                     ? tr("/var/www/example.com")
                                     : tr("C:\\Users\\…"));

    m_stack->addWidget(scroll);
    m_stack->addWidget(m_editor);
    layout->addWidget(m_stack, 1);

    connect(m_editor, &QLineEdit::returnPressed, this, [this] {
        const QString typed = normalize(m_editor->text());
        m_stack->setCurrentIndex(0);
        if (typed.isEmpty())
            return;
        emit pathActivated(typed);
    });
    connect(m_editor, &QLineEdit::editingFinished, this, [this] {
        if (m_stack->currentIndex() == 1)
            m_stack->setCurrentIndex(0);
    });

    rebuildCrumbs();
}

QString PathBar::normalize(const QString &value) const
{
    QString result = value.trimmed();
    if (result.isEmpty())
        return result;
    if (m_style == Style::Remote) {
        result = QDir::cleanPath(QDir::fromNativeSeparators(result));
        if (!result.startsWith(QLatin1Char('/')))
            result.prepend(QLatin1Char('/'));
        return result;
    }
    return QDir::cleanPath(result);
}

void PathBar::setPath(const QString &path, bool recordHistory)
{
    const QString normalized = normalize(path);
    if (normalized.isEmpty())
        return;
    const bool changed = normalized != m_path;
    m_path = normalized;
    if (recordHistory && changed) {
        while (m_history.size() > m_historyIndex + 1)
            m_history.removeLast();
        m_history.append(normalized);
        m_historyIndex = static_cast<int>(m_history.size()) - 1;
    }
    if (m_history.isEmpty()) {
        m_history.append(normalized);
        m_historyIndex = 0;
    }
    m_editor->setText(normalized);
    rebuildCrumbs();
    emit historyChanged();
}

QString PathBar::parentPath() const
{
    if (m_style == Style::Remote) {
        if (m_path == QStringLiteral("/") || m_path.isEmpty())
            return m_path;
        const qsizetype slash = m_path.lastIndexOf(QLatin1Char('/'));
        return slash <= 0 ? QStringLiteral("/") : m_path.left(slash);
    }
    QDir directory(m_path);
    return directory.cdUp() ? directory.absolutePath() : m_path;
}

void PathBar::beginEdit()
{
    m_editor->setText(m_path);
    m_stack->setCurrentIndex(1);
    m_editor->setFocus();
    m_editor->selectAll();
}

void PathBar::goBack()
{
    if (!canGoBack())
        return;
    --m_historyIndex;
    const QString target = m_history.at(m_historyIndex);
    m_path = target;
    m_editor->setText(target);
    rebuildCrumbs();
    emit historyChanged();
    emit pathActivated(target);
}

void PathBar::goForward()
{
    if (!canGoForward())
        return;
    ++m_historyIndex;
    const QString target = m_history.at(m_historyIndex);
    m_path = target;
    m_editor->setText(target);
    rebuildCrumbs();
    emit historyChanged();
    emit pathActivated(target);
}

void PathBar::goUp()
{
    const QString parent = parentPath();
    if (!parent.isEmpty() && parent != m_path)
        emit pathActivated(parent);
}

void PathBar::rebuildCrumbs()
{
    while (QLayoutItem *item = m_crumbLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    QStringList segments;
    QStringList targets;
    if (m_style == Style::Remote) {
        segments.append(QStringLiteral("/"));
        targets.append(QStringLiteral("/"));
        QString accumulated;
        for (const QString &part :
             m_path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            accumulated += QLatin1Char('/') + part;
            segments.append(part);
            targets.append(accumulated);
        }
    } else {
        const QStringList parts =
            m_path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString accumulated;
        for (const QString &part : parts) {
            accumulated += accumulated.isEmpty() ? part
                                                 : QLatin1Char('/') + part;
            segments.append(part);
            targets.append(accumulated.endsWith(QLatin1Char(':'))
                               ? accumulated + QLatin1Char('/')
                               : accumulated);
        }
    }

    constexpr int MaxSegments = 6;
    int firstVisible = 0;
    if (segments.size() > MaxSegments)
        firstVisible = static_cast<int>(segments.size()) - MaxSegments;

    if (firstVisible > 0) {
        auto *ellipsis = new QLabel(QStringLiteral("…"));
        ellipsis->setObjectName(QStringLiteral("crumbSeparator"));
        ellipsis->setContentsMargins(4, 0, 4, 0);
        m_crumbLayout->addWidget(ellipsis);
    }

    for (int i = firstVisible; i < segments.size(); ++i) {
        if (i > firstVisible || firstVisible > 0) {
            auto *separator = new QLabel(QStringLiteral("›"));
            separator->setObjectName(QStringLiteral("crumbSeparator"));
            m_crumbLayout->addWidget(separator);
        }
        auto *crumb = new QPushButton(segments.at(i));
        crumb->setObjectName(QStringLiteral("crumbButton"));
        crumb->setCursor(Qt::PointingHandCursor);
        crumb->setFlat(true);
        const bool current = i == segments.size() - 1;
        crumb->setProperty("current", current);
        const QString target = targets.at(i);
        connect(crumb, &QPushButton::clicked, this,
                [this, target] { emit pathActivated(target); });
        m_crumbLayout->addWidget(crumb);
    }
    m_crumbLayout->addStretch();
}
