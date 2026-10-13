// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "textconfig.h"
#include "utils/colorutils.h"
#include "utils/confighandler.h"
#include "utils/pathinfo.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPushButton>

TextConfig::TextConfig(QWidget* parent)
  : QWidget(parent)
  , m_layout(new QVBoxLayout(this))
  , m_fontsCB(new QComboBox())
  , m_strikeOutButton(nullptr)
  , m_underlineButton(nullptr)
  , m_weightButton(nullptr)
  , m_italicButton(nullptr)
  , m_leftAlignButton(nullptr)
  , m_centerAlignButton(nullptr)
  , m_rightAlignButton(nullptr)
{

    connect(m_fontsCB,
            &QComboBox::currentTextChanged,
            this,
            &TextConfig::fontFamilyChanged);
    m_fontsCB->addItems(QFontDatabase::families());
    setFontFamily(ConfigHandler().fontFamily());

    QString iconPrefix = ColorUtils::colorIsDark(palette().windowText().color())
                           ? PathInfo::blackIconPath()
                           : PathInfo::whiteIconPath();

    m_strikeOutButton = new QPushButton(
      QIcon(iconPrefix + "format_strikethrough.svg"), QLatin1String(""));
    m_strikeOutButton->setCheckable(true);
    connect(m_strikeOutButton,
            &QPushButton::clicked,
            this,
            &TextConfig::fontStrikeOutChanged);
    m_strikeOutButton->setToolTip(tr("StrikeOut"));

    m_underlineButton = new QPushButton(
      QIcon(iconPrefix + "format_underlined.svg"), QLatin1String(""));
    m_underlineButton->setCheckable(true);
    connect(m_underlineButton,
            &QPushButton::clicked,
            this,
            &TextConfig::fontUnderlineChanged);
    m_underlineButton->setToolTip(tr("Underline"));

    m_weightButton =
      new QPushButton(QIcon(iconPrefix + "format_bold.svg"), QLatin1String(""));
    m_weightButton->setCheckable(true);
    connect(m_weightButton,
            &QPushButton::clicked,
            this,
            &TextConfig::weightButtonPressed);
    m_weightButton->setToolTip(tr("Bold"));

    m_italicButton = new QPushButton(QIcon(iconPrefix + "format_italic.svg"),
                                     QLatin1String(""));
    m_italicButton->setCheckable(true);
    connect(m_italicButton,
            &QPushButton::clicked,
            this,
            &TextConfig::fontItalicChanged);
    m_italicButton->setToolTip(tr("Italic"));
    auto* modifiersLayout = new QHBoxLayout();

    m_leftAlignButton =
      new QPushButton(QIcon(iconPrefix + "leftalign.svg"), QLatin1String(""));
    m_leftAlignButton->setCheckable(true);
    m_leftAlignButton->setAutoExclusive(true);
    connect(m_leftAlignButton, &QPushButton::clicked, this, [this] {
        emit alignmentChanged(Qt::AlignLeft);
    });
    m_leftAlignButton->setToolTip(tr("Left Align"));

    m_centerAlignButton =
      new QPushButton(QIcon(iconPrefix + "centeralign.svg"), QLatin1String(""));
    m_centerAlignButton->setCheckable(true);
    m_centerAlignButton->setAutoExclusive(true);
    connect(m_centerAlignButton, &QPushButton::clicked, this, [this] {
        emit alignmentChanged(Qt::AlignCenter);
    });
    m_centerAlignButton->setToolTip(tr("Center Align"));

    m_rightAlignButton =
      new QPushButton(QIcon(iconPrefix + "rightalign.svg"), QLatin1String(""));
    m_rightAlignButton->setCheckable(true);
    m_rightAlignButton->setAutoExclusive(true);
    connect(m_rightAlignButton, &QPushButton::clicked, this, [this] {
        emit alignmentChanged(Qt::AlignRight);
    });
    m_rightAlignButton->setToolTip(tr("Right Align"));

    auto* alignmentLayout = new QHBoxLayout();
    alignmentLayout->addWidget(m_leftAlignButton);
    alignmentLayout->addWidget(m_centerAlignButton);
    alignmentLayout->addWidget(m_rightAlignButton);

    m_layout->addWidget(m_fontsCB);
    // On Wayland a transient QComboBox popup may lose its mouse grab as the
    // screenshot editor and its scrollable tool panel exchange focus.
    // An inline view avoids creating a second Wayland popup surface entirely.
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        m_fontsCB->installEventFilter(this);
        m_fontsCB->setToolTip(tr("Select font (click to expand list)"));
        m_fontSearch = new QLineEdit(this);
        m_fontSearch->setPlaceholderText(tr("Search fonts..."));
        m_fontSearch->setClearButtonEnabled(true);
        m_fontSearch->hide();
        m_layout->addWidget(m_fontSearch);
        m_fontChoices = new QListWidget(this);
        m_fontChoices->setObjectName(QStringLiteral("windowEditorInlineFontList"));
        m_fontChoices->setMaximumHeight(180);
        m_fontChoices->setMinimumHeight(135);
        m_fontChoices->setSelectionMode(QAbstractItemView::SingleSelection);
        m_fontChoices->addItems(QFontDatabase::families());
        m_fontChoices->hide();
        m_layout->addWidget(m_fontChoices);
        connect(m_fontSearch, &QLineEdit::textChanged, this,
                [this](const QString& text) {
            for (int i = 0; i < m_fontChoices->count(); ++i) {
                auto* item = m_fontChoices->item(i);
                item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
            }
        });
        connect(m_fontChoices, &QListWidget::itemClicked, this,
                [this](QListWidgetItem* item) {
            if (!item) return;
            const QString family = item->text();
            m_fontChoices->hide();
            m_fontSearch->hide();
            m_fontsCB->setCurrentText(family);
        });
    }
    modifiersLayout->addWidget(m_strikeOutButton);
    modifiersLayout->addWidget(m_underlineButton);
    modifiersLayout->addWidget(m_weightButton);
    modifiersLayout->addWidget(m_italicButton);
    m_layout->addLayout(modifiersLayout);
    m_layout->addLayout(alignmentLayout);
}

bool TextConfig::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_fontsCB && m_fontChoices && event) {
        const auto toggleFontList = [this]() {
            const bool open = !m_fontChoices->isVisible();
            m_fontChoices->setVisible(open);
            m_fontSearch->setVisible(open);
            if (open) {
                m_fontSearch->clear();
                const auto hits = m_fontChoices->findItems(
                  m_fontsCB->currentText(), Qt::MatchExactly);
                if (!hits.isEmpty()) m_fontChoices->scrollToItem(hits.first());
                m_fontSearch->setFocus();
            }
        };
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                toggleFontList();
                return true;
            }
        }
        // Do not deliver a release without its press to QComboBox; Qt would
        // otherwise try to display its native Wayland popup.
        if (event->type() == QEvent::MouseButtonRelease) return true;
        if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Space || key->key() == Qt::Key_Return ||
                key->key() == Qt::Key_Enter || key->key() == Qt::Key_Down) {
                toggleFontList();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TextConfig::setFontFamily(const QString& fontFamily)
{
    m_fontsCB->setCurrentIndex(
      m_fontsCB->findText(fontFamily.isEmpty() ? font().family() : fontFamily));
}

void TextConfig::setUnderline(const bool underline)
{
    m_underlineButton->setChecked(underline);
}

void TextConfig::setStrikeOut(const bool strikeout)
{
    m_strikeOutButton->setChecked(strikeout);
}

void TextConfig::setWeight(const int weight)
{
    m_weightButton->setChecked(static_cast<QFont::Weight>(weight) ==
                               QFont::Bold);
}

void TextConfig::setItalic(const bool italic)
{
    m_italicButton->setChecked(italic);
}

void TextConfig::weightButtonPressed(const bool weight)
{
    if (weight) {
        emit fontWeightChanged(QFont::Bold);
    } else {
        emit fontWeightChanged(QFont::Normal);
    }
}

void TextConfig::setTextAlignment(Qt::AlignmentFlag alignment)
{
    switch (alignment) {
        case (Qt::AlignCenter):
            m_leftAlignButton->setChecked(false);
            m_centerAlignButton->setChecked(true);
            m_rightAlignButton->setChecked(false);
            break;
        case (Qt::AlignRight):
            m_leftAlignButton->setChecked(false);
            m_centerAlignButton->setChecked(false);
            m_rightAlignButton->setChecked(true);
            break;
        case (Qt::AlignLeft):
        default:
            m_leftAlignButton->setChecked(true);
            m_centerAlignButton->setChecked(false);
            m_rightAlignButton->setChecked(false);
            break;
    }
    emit alignmentChanged(alignment);
}
