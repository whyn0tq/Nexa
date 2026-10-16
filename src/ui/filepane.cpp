#include "filepane.h"

#include "controls.h"
#include "filelist.h"
#include "format.h"
#include "theme.h"

#include <QAction>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

Breadcrumb::Breadcrumb(bool remote, QWidget *parent) : QWidget(parent), m_remote(remote)
{
    setMouseTracking(true);
    setFixedHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::IBeamCursor);
    setToolTip(tr("Нажмите на свободное место, чтобы ввести путь"));

    m_editor = new QLineEdit(this);
    m_editor->setProperty("compact", true);
    m_editor->hide();
    connect(m_editor, &QLineEdit::returnPressed, this, [this] {
        const QString text = m_editor->text().trimmed();
        m_editor->hide();
        if (!text.isEmpty())
            emit pathActivated(text);
    });
    connect(m_editor, &QLineEdit::editingFinished, this, [this] {
        m_editor->hide();
        update();
    });
    auto *cancel = new QAction(m_editor);
    cancel->setShortcut(Qt::Key_Escape);
    cancel->setShortcutContext(Qt::WidgetShortcut);
    m_editor->addAction(cancel);
    connect(cancel, &QAction::triggered, this, [this] {
        m_editor->hide();
        update();
    });
}

QSize Breadcrumb::sizeHint() const
{
    return {300, 32};
}

void Breadcrumb::setPath(const QString &path)
{
    m_path = path;
    m_segments.clear();
    if (m_remote) {
        m_segments.append({QStringLiteral("/"), QStringLiteral("/"), {}, true});
        QString accumulated;
        for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            accumulated += QLatin1Char('/') + part;
            m_segments.append({part, accumulated, {}, false});
        }
    } else {
        m_segments.append({tr("Этот компьютер"), QString(), {}, true});
        const QString clean = QDir::fromNativeSeparators(path);
        QString accumulated;
        for (const QString &part : clean.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            accumulated = accumulated.isEmpty() ? part + QLatin1Char('/')
                                                : QDir(accumulated).filePath(part);
            m_segments.append({part, accumulated, {}, false});
        }
    }
    relayout();
    update();
}

void Breadcrumb::beginEdit()
{
    m_editor->setGeometry(rect());
    m_editor->setText(m_remote ? m_path : QDir::toNativeSeparators(m_path));
    m_editor->show();
    m_editor->setFocus();
    m_editor->selectAll();
    update();
}

void Breadcrumb::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_editor->setGeometry(rect());
    relayout();
}

void Breadcrumb::relayout()
{
    const QFontMetrics metrics(Theme::font(13, QFont::Medium));
    QVector<qreal> widths;
    for (const Segment &segment : m_segments) {
        const qreal text = segment.root && m_remote ? 18
                           : segment.root           ? 18
                                                    : metrics.horizontalAdvance(segment.label);
        widths.append(text + 16);
    }
    const qreal separator = 14;
    const qreal available = width() - 4;
    m_firstVisible = 0;
    auto total = [&](int from) {
        qreal sum = widths.isEmpty() ? 0 : widths.first();
        for (int i = std::max(from, 1); i < widths.size(); ++i)
            sum += widths.at(i) + separator;
        if (from > 1)
            sum += 22 + separator;
        return sum;
    };
    while (m_firstVisible < m_segments.size() - 1 && total(m_firstVisible) > available)
        ++m_firstVisible;
    qreal x = 0;
    for (int i = 0; i < m_segments.size(); ++i) {
        Segment &segment = m_segments[i];
        if (i > 0 && i < m_firstVisible && m_firstVisible > 1) {
            segment.rect = QRectF();
            continue;
        }
        if (i > 0)
            x += separator;
        if (i == m_firstVisible && m_firstVisible > 1)
            x += 22 + separator;
        segment.rect = QRectF(x, 2, widths.at(i), height() - 4);
        x += widths.at(i);
    }
}

int Breadcrumb::segmentAt(const QPoint &point) const
{
    for (int i = 0; i < m_segments.size(); ++i) {
        if (m_segments.at(i).rect.contains(point))
            return i;
    }
    return -1;
}

void Breadcrumb::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_editor->isVisible())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(Theme::font(13, QFont::Medium));
    for (int i = 0; i < m_segments.size(); ++i) {
        const Segment &segment = m_segments.at(i);
        if (segment.rect.isNull())
            continue;
        const bool last = i == m_segments.size() - 1;
        if (i == m_hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 18));
            painter.drawRoundedRect(segment.rect, 8, 8);
        }
        if (i > 0) {
            const qreal x = segment.rect.left() - 14;
            if (i == m_firstVisible && m_firstVisible > 1) {
                painter.setPen(Theme::TextMuted);
                painter.drawText(QRectF(x - 22 - 14 + 14, 0, 22, height()), Qt::AlignCenter,
                                 QStringLiteral("…"));
                Icons::paint(painter, Icon::ChevronRight, QRectF(x - 22 - 13, 10, 12, 12),
                             Theme::TextFaint, 2);
            }
            Icons::paint(painter, Icon::ChevronRight, QRectF(x + 1, 10, 12, 12),
                         Theme::TextFaint, 2);
        }
        if (segment.root) {
            Icons::paint(painter, m_remote ? Icon::Server : Icon::Computer,
                         QRectF(segment.rect.center().x() - 8, segment.rect.center().y() - 8,
                                16, 16),
                         last ? Theme::Text : Theme::TextSecondary, 1.8);
            continue;
        }
        painter.setPen(last ? Theme::Text : Theme::TextSecondary);
        painter.drawText(segment.rect, Qt::AlignCenter, segment.label);
    }
}

void Breadcrumb::mouseMoveEvent(QMouseEvent *event)
{
    const int index = segmentAt(event->position().toPoint());
    if (index != m_hovered) {
        m_hovered = index;
        setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::IBeamCursor);
        update();
    }
}

void Breadcrumb::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int index = segmentAt(event->position().toPoint());
    if (index >= 0) {
        emit pathActivated(m_segments.at(index).path);
        return;
    }
    beginEdit();
}

void Breadcrumb::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    beginEdit();
}

void Breadcrumb::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

FilePane::FilePane(bool remote, QWidget *parent)
    : GlassPanel(Glass::Tone::Panel, Theme::RadiusLarge, parent), m_remote(remote)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 8);
    layout->setSpacing(6);

    auto *top = new QHBoxLayout;
    top->setContentsMargins(6, 0, 0, 0);
    top->setSpacing(8);
    m_captionIcon = new QLabel;
    m_captionIcon->setFixedSize(16, 16);
    m_caption = makeLabel(QString(), 12, Theme::TextMuted, QFont::DemiBold);
    top->addWidget(m_captionIcon);
    top->addWidget(m_caption);
    top->addStretch();
    m_filter = new QLineEdit;
    m_filter->setProperty("compact", true);
    m_filter->setPlaceholderText(tr("Фильтр"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setFixedWidth(170);
    m_filter->setFocusPolicy(Qt::ClickFocus);
    m_filter->addAction(Icons::icon(Icon::Search, Theme::TextMuted, 14),
                        QLineEdit::LeadingPosition);
    top->addWidget(m_filter);
    layout->addLayout(top);

    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(2);
    m_backButton = new IconButton(Icon::Back, tr("Назад (Alt+←)"));
    m_forwardButton = new IconButton(Icon::Forward, tr("Вперёд (Alt+→)"));
    m_upButton = new IconButton(Icon::Up, tr("Вверх (Backspace)"));
    m_refreshButton = new IconButton(Icon::Refresh, tr("Обновить (F5)"));
    for (IconButton *button : {m_backButton, m_forwardButton, m_upButton}) {
        button->setFixedSize(32, 32);
        button->setIconSize(17);
        navigation->addWidget(button);
    }
    navigation->addSpacing(4);
    m_crumbs = new Breadcrumb(remote);
    navigation->addWidget(m_crumbs, 1);
    m_refreshButton->setFixedSize(32, 32);
    m_refreshButton->setIconSize(16);
    navigation->addWidget(m_refreshButton);
    layout->addLayout(navigation);

    m_stack = new QStackedWidget;
    m_model = new FileListModel(remote, this);
    m_proxy = new FileFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(FileListModel::SortRole);
    m_view = new FileView;
    m_view->setModel(m_proxy);
    m_view->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
    m_view->header()->setSectionResizeMode(FileListModel::NameColumn, QHeaderView::Stretch);
    m_view->header()->setSectionResizeMode(FileListModel::SizeColumn, QHeaderView::Fixed);
    m_view->header()->setSectionResizeMode(FileListModel::ModifiedColumn, QHeaderView::Fixed);
    m_view->header()->resizeSection(FileListModel::SizeColumn, 96);
    m_view->header()->resizeSection(FileListModel::ModifiedColumn, 132);
    m_view->header()->setStretchLastSection(false);
    m_stack->addWidget(m_view);

    m_messagePage = new QWidget;
    auto *message = new QVBoxLayout(m_messagePage);
    message->setContentsMargins(32, 24, 32, 40);
    message->setSpacing(10);
    message->addStretch();
    m_messageIcon = new QLabel;
    m_messageIcon->setAlignment(Qt::AlignCenter);
    m_messageTitle = makeLabel(QString(), 16, Theme::Text, QFont::DemiBold);
    m_messageTitle->setAlignment(Qt::AlignCenter);
    m_messageText = makeLabel(QString(), 13, Theme::TextSecondary);
    m_messageText->setAlignment(Qt::AlignCenter);
    m_messageText->setWordWrap(true);
    m_messageAction = makeButton(QString(), QStringLiteral("primary"));
    m_messageAction->setFixedWidth(200);
    message->addWidget(m_messageIcon);
    message->addWidget(m_messageTitle);
    message->addWidget(m_messageText);
    message->addSpacing(6);
    message->addWidget(m_messageAction, 0, Qt::AlignHCenter);
    message->addStretch();
    m_stack->addWidget(m_messagePage);
    layout->addWidget(m_stack, 1);

    m_footer = makeLabel(QString(), 12, Theme::TextMuted);
    m_footer->setContentsMargins(8, 2, 8, 0);
    layout->addWidget(m_footer);

    connect(m_backButton, &IconButton::clicked, this, &FilePane::goBack);
    connect(m_forwardButton, &IconButton::clicked, this, &FilePane::goForward);
    connect(m_upButton, &IconButton::clicked, this, &FilePane::goUp);
    connect(m_refreshButton, &IconButton::clicked, this, &FilePane::refreshRequested);
    connect(m_crumbs, &Breadcrumb::pathActivated, this, &FilePane::navigateRequested);
    connect(m_messageAction, &QPushButton::clicked, this, &FilePane::messageActionTriggered);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setNameFilter(text);
        updateFooter();
    });
    connect(m_view, &FileView::openRequested, this, [this](const QModelIndex &index) {
        if (const RemoteEntry *entry = entryAt(index))
            emit entryOpened(*entry);
    });
    connect(m_view, &FileView::backRequested, this, &FilePane::goUp);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        const QModelIndex index = m_view->indexAt(point);
        if (index.isValid() &&
            !m_view->selectionModel()->isRowSelected(index.row(), index.parent())) {
            m_view->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        if (!index.isValid())
            m_view->clearSelection();
        emit contextMenuRequested(m_view->viewport()->mapToGlobal(point), index.isValid());
    });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &FilePane::updateFooter);
    updateButtons();
}

void FilePane::setCaption(const QString &caption, Icon icon)
{
    m_caption->setText(caption.toUpper());
    m_captionIcon->setPixmap(Icons::pixmap(icon, Theme::TextMuted, 16));
}

QString FilePane::parentOf(const QString &path, bool remote)
{
    if (remote) {
        if (path == QStringLiteral("/") || path.isEmpty())
            return QStringLiteral("/");
        const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
        return slash <= 0 ? QStringLiteral("/") : path.left(slash);
    }
    if (path.isEmpty())
        return path;
    QDir directory(path);
    if (!directory.cdUp())
        return QString();
    return directory.absolutePath();
}

void FilePane::setPath(const QString &path, bool record)
{
    if (record && path != m_path && !m_path.isNull()) {
        m_back.append(m_path);
        m_forward.clear();
    }
    m_path = path;
    m_crumbs->setPath(path);
    m_filter->clear();
    updateButtons();
}

void FilePane::goBack()
{
    if (m_back.isEmpty())
        return;
    m_forward.prepend(m_path);
    const QString target = m_back.takeLast();
    setPath(target, false);
    emit navigateRequested(target);
}

void FilePane::goForward()
{
    if (m_forward.isEmpty())
        return;
    m_back.append(m_path);
    const QString target = m_forward.takeFirst();
    setPath(target, false);
    emit navigateRequested(target);
}

void FilePane::goUp()
{
    const QString parent = parentOf(m_path, m_remote);
    if (parent != m_path || (!m_remote && !m_path.isEmpty()))
        emit navigateRequested(parent);
}

void FilePane::updateButtons()
{
    m_backButton->setEnabled(!m_back.isEmpty());
    m_forwardButton->setEnabled(!m_forward.isEmpty());
    m_upButton->setEnabled(m_remote ? m_path != QStringLiteral("/") : !m_path.isEmpty());
}

void FilePane::setInteractive(bool interactive)
{
    for (QWidget *widget : std::initializer_list<QWidget *>{m_backButton, m_forwardButton,
                                                           m_upButton, m_refreshButton,
                                                           m_crumbs, m_filter})
        widget->setEnabled(interactive);
    if (interactive)
        updateButtons();
}

void FilePane::setEntries(const RemoteEntries &entries)
{
    QStringList selected;
    for (const RemoteEntry &entry : selectedEntries())
        selected.append(entry.name);
    m_model->setEntries(entries);
    if (!selected.isEmpty()) {
        for (int row = 0; row < m_proxy->rowCount(); ++row) {
            const QModelIndex index = m_proxy->index(row, 0);
            if (selected.contains(index.data().toString()))
                m_view->selectionModel()->select(
                    index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
    if (entries.isEmpty())
        showMessage(Icon::Folder, tr("Папка пуста"),
                    m_remote ? tr("Перетащите сюда файлы или папки — загрузка начнётся сразу.")
                             : tr("Здесь пока ничего нет."));
    else
        showList();
    updateFooter();
}

void FilePane::removeEntries(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    RemoteEntries kept;
    for (const RemoteEntry &entry : m_model->entries()) {
        if (!paths.contains(entry.path))
            kept.append(entry);
    }
    setEntries(kept);
}

void FilePane::showLoading(const QString &text)
{
    if (m_model->rowCount() > 0) {
        m_footer->setText(text);
        return;
    }
    m_footer->clear();
    showMessage(Icon::Refresh, text, QString());
}

void FilePane::showMessage(Icon icon, const QString &title, const QString &text,
                           const QString &action)
{
    QPixmap pixmap(56 * 2, 56 * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(255, 255, 255, 30), 1));
        p.setBrush(QColor(255, 255, 255, 12));
        p.drawRoundedRect(QRectF(0.5, 0.5, 55, 55), 16, 16);
        if (icon == Icon::Folder)
            Icons::paintFolder(p, QRectF(14, 14, 28, 28));
        else
            Icons::paint(p, icon, QRectF(15, 15, 26, 26), Theme::TextSecondary, 1.6);
    }
    m_messageIcon->setPixmap(pixmap);
    m_messageTitle->setText(title);
    m_messageText->setText(text);
    m_messageText->setVisible(!text.isEmpty());
    m_messageAction->setText(action);
    m_messageAction->setVisible(!action.isEmpty());
    m_stack->setCurrentWidget(m_messagePage);
}

void FilePane::showList()
{
    m_stack->setCurrentWidget(m_view);
}

void FilePane::updateFooter()
{
    int files = 0;
    int folders = 0;
    qint64 bytes = 0;
    for (const RemoteEntry &entry : m_model->entries()) {
        if (entry.directory) {
            ++folders;
        } else {
            ++files;
            bytes += entry.size;
        }
    }
    QString text = tr("Папок: %1 · Файлов: %2 · %3")
                       .arg(folders)
                       .arg(files)
                       .arg(Format::size(bytes));
    const RemoteEntries selected = selectedEntries();
    if (!selected.isEmpty()) {
        qint64 selectedBytes = 0;
        for (const RemoteEntry &entry : selected)
            selectedBytes += entry.directory ? 0 : entry.size;
        text = tr("Выбрано: %1 · %2").arg(selected.size()).arg(Format::size(selectedBytes));
    }
    m_footer->setText(text);
}

RemoteEntries FilePane::selectedEntries() const
{
    RemoteEntries result;
    for (const QModelIndex &index : m_view->selectionModel()->selectedRows()) {
        if (const RemoteEntry *entry = entryAt(index))
            result.append(*entry);
    }
    return result;
}

const RemoteEntry *FilePane::entryAt(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return nullptr;
    return m_model->entry(m_proxy->mapToSource(proxyIndex).row());
}

const RemoteEntry *FilePane::entryAtViewport(const QPoint &point) const
{
    return entryAt(m_view->indexAt(point));
}

void FilePane::focusFilter()
{
    m_filter->setFocus();
    m_filter->selectAll();
}

void FilePane::editPath()
{
    m_crumbs->beginEdit();
}

void FilePane::resizeEvent(QResizeEvent *event)
{
    GlassPanel::resizeEvent(event);
    const bool narrow = width() < 520;
    m_view->setColumnHidden(FileListModel::ModifiedColumn, narrow);
    m_filter->setFixedWidth(narrow ? 120 : 170);
}
