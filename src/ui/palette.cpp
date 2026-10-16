#include "palette.h"

#include "theme.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int IconRole = Qt::UserRole + 1;
constexpr int SubtitleRole = Qt::UserRole + 2;
constexpr int GroupRole = Qt::UserRole + 3;

class CommandDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {300, 48};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(option.rect).adjusted(2, 2, -2, -2);
        const bool selected = option.state & QStyle::State_Selected;
        if (selected) {
            painter->setPen(QPen(QColor(255, 90, 108, 90), 1));
            painter->setBrush(QColor(255, 66, 86, 46));
            painter->drawRoundedRect(r, 10, 10);
        }
        const auto icon = static_cast<Icon>(index.data(IconRole).toInt());
        const QRectF bubble(r.left() + 8, r.center().y() - 15, 30, 30);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, selected ? 26 : 14));
        painter->drawRoundedRect(bubble, 9, 9);
        Icons::paint(*painter, icon, bubble.adjusted(7, 7, -7, -7),
                     selected ? Theme::Text : Theme::TextSecondary, 1.9);
        painter->setFont(Theme::font(13.5, QFont::Medium));
        painter->setPen(Theme::Text);
        const QString subtitle = index.data(SubtitleRole).toString();
        const QRectF textRect(bubble.right() + 12, r.top(), r.width() - 160, r.height());
        if (subtitle.isEmpty()) {
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                              index.data(Qt::DisplayRole).toString());
        } else {
            painter->drawText(textRect.adjusted(0, 5, 0, -r.height() / 2 + 2),
                              Qt::AlignBottom | Qt::AlignLeft, index.data(Qt::DisplayRole).toString());
            painter->setFont(Theme::font(12));
            painter->setPen(Theme::TextMuted);
            painter->drawText(textRect.adjusted(0, r.height() / 2 + 1, 0, -4),
                              Qt::AlignTop | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(subtitle, Qt::ElideMiddle,
                                                                static_cast<int>(textRect.width())));
        }
        painter->setFont(Theme::font(11, QFont::DemiBold));
        painter->setPen(Theme::TextFaint);
        painter->drawText(r.adjusted(0, 0, -14, 0), Qt::AlignVCenter | Qt::AlignRight,
                          index.data(GroupRole).toString());
        painter->restore();
    }
};
}

CommandPalette::CommandPalette(QWidget *host, const QVector<Command> &commands)
    : Sheet(host, 620), m_commands(commands)
{
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Сервер, вкладка или действие…"));
    m_search->addAction(Icons::icon(Icon::Search, Theme::TextMuted, 16),
                        QLineEdit::LeadingPosition);
    m_search->setStyleSheet(QStringLiteral("QLineEdit{min-height:44px;font-size:15px;border-radius:12px;}"));
    m_search->installEventFilter(this);
    body()->addWidget(m_search);

    m_list = new QListWidget;
    m_list->setItemDelegate(new CommandDelegate(m_list));
    m_list->setFixedHeight(48 * 7 + 4);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->viewport()->setAutoFillBackground(false);
    body()->addWidget(m_list);

    connect(m_search, &QLineEdit::textChanged, this, &CommandPalette::refill);
    connect(m_list, &QListWidget::itemActivated, this, &CommandPalette::runCurrent);
    connect(m_list, &QListWidget::itemClicked, this, &CommandPalette::runCurrent);
    refill();
    QTimer::singleShot(0, m_search, qOverload<>(&QWidget::setFocus));
}

void CommandPalette::refill()
{
    m_list->clear();
    m_visible.clear();
    const QString query = m_search->text().trimmed();
    for (int i = 0; i < m_commands.size(); ++i) {
        const Command &command = m_commands.at(i);
        if (!query.isEmpty() && !command.title.contains(query, Qt::CaseInsensitive) &&
            !command.subtitle.contains(query, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(command.title);
        item->setData(IconRole, static_cast<int>(command.icon));
        item->setData(SubtitleRole, command.subtitle);
        item->setData(GroupRole, command.group);
        m_list->addItem(item);
        m_visible.append(i);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void CommandPalette::runCurrent()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_visible.size())
        return;
    const auto run = m_commands.at(m_visible.at(row)).run;
    dismiss();
    if (run)
        QTimer::singleShot(0, run);
}

bool CommandPalette::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        const int count = m_list->count();
        if (key->key() == Qt::Key_Down && count > 0) {
            m_list->setCurrentRow((m_list->currentRow() + 1) % count);
            return true;
        }
        if (key->key() == Qt::Key_Up && count > 0) {
            m_list->setCurrentRow((m_list->currentRow() - 1 + count) % count);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            runCurrent();
            return true;
        }
    }
    return Sheet::eventFilter(watched, event);
}
