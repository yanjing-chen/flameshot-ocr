// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "configwindow.h"
#include "config/configresolver.h"
#include "config/filenameeditor.h"
#include "config/generalconf.h"
#include "config/ocrconf.h"
#include "config/shortcutswidget.h"
#include "config/visualseditor.h"
#include "utils/colorutils.h"
#include "utils/confighandler.h"
#include "utils/globalvalues.h"
#include "utils/pathinfo.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QScreen>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QSizePolicy>
#include <QTabBar>
#include <QTextStream>
#include <QVBoxLayout>

// ConfigWindow contains the menus where you can configure the application

ConfigWindow::ConfigWindow(QWidget* parent)
  : QWidget(parent)
{
    // G.2: keep the original QTabWidget and all five pages/signals,
    // but replace its visual tab strip with a GNOME-style sidebar.
    setObjectName(QStringLiteral("flameshotConfigWindow"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(14);

    auto* sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("configSidebar"));
    sidebar->setFixedWidth(202);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(12, 18, 12, 12);
    sidebarLayout->setSpacing(14);
    auto* sidebarTitle = new QLabel(tr("Configuration"), sidebar);
    sidebarTitle->setObjectName(QStringLiteral("configSidebarTitle"));
    sidebarLayout->addWidget(sidebarTitle);

    auto* navigation = new QListWidget(sidebar);
    navigation->setObjectName(QStringLiteral("configNavigation"));
    navigation->setSelectionMode(QAbstractItemView::SingleSelection);
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navigation->setSpacing(4);
    navigation->setIconSize(QSize(20, 20));
    sidebarLayout->addWidget(navigation, 1);
    layout->addWidget(sidebar);

    auto* contentCard = new QFrame(this);
    contentCard->setObjectName(QStringLiteral("configContentCard"));
    auto* contentLayout = new QVBoxLayout(contentCard);
    contentLayout->setContentsMargins(20, 18, 20, 18);
    contentLayout->setSpacing(14);
    auto* pageTitle = new QLabel(contentCard);
    pageTitle->setObjectName(QStringLiteral("configPageTitle"));
    contentLayout->addWidget(pageTitle);

    m_tabWidget = new QTabWidget(contentCard);
    m_tabWidget->setObjectName(QStringLiteral("configPageStack"));
    m_tabWidget->tabBar()->setUsesScrollButtons(false);
    contentLayout->addWidget(m_tabWidget, 1);
    layout->addWidget(contentCard, 1);

    QFont readableFont = font();
    if (readableFont.pointSize() > 0) {
        readableFont.setPointSize(qMax(11, readableFont.pointSize()));
    } else {
        readableFont.setPixelSize(qMax(15, readableFont.pixelSize()));
    }
    setFont(readableFont);
    setMinimumSize(780, 540);
    resize(1050, 800);

    const bool darkTheme =
      ColorUtils::colorIsDark(palette().window().color());
    const QString bg = darkTheme ? QStringLiteral("#202329")
                                 : QStringLiteral("#f3f5f8");
    const QString surface = darkTheme ? QStringLiteral("#2b3038")
                                      : QStringLiteral("#ffffff");
    const QString outline = darkTheme ? QStringLiteral("#464d58")
                                      : QStringLiteral("#dce1e8");
    const QString fg = darkTheme ? QStringLiteral("#ecf0f5")
                                 : QStringLiteral("#253042");
    const QString muted = darkTheme ? QStringLiteral("#b5beca")
                                    : QStringLiteral("#687589");
    const QString hover = darkTheme ? QStringLiteral("#3c4554")
                                    : QStringLiteral("#edf2fa");
    const QString selected = darkTheme ? QStringLiteral("#345373")
                                       : QStringLiteral("#dfecff");
    const QString formBg = darkTheme ? QStringLiteral("#303741")
                                      : QStringLiteral("#ffffff");

    // QComboBox and QSpinBox intentionally retain native subcontrol painting:
    // no custom borders, padding, arrows, or popup-item colors.
    setStyleSheet(QStringLiteral(R"QSS(
QWidget#flameshotConfigWindow { background: %1; }
QFrame#configSidebar, QFrame#configContentCard {
    background: %2; border: 1px solid %3; border-radius: 12px;
}
QLabel#configSidebarTitle {
    color: %4; font-size: 19px; font-weight: 700; padding: 4px 8px;
}
QLabel#configPageTitle {
    color: %4; font-size: 20px; font-weight: 700; padding: 4px 2px 9px 2px;
}
QListWidget#configNavigation {
    background: transparent; border: 0; outline: 0; color: %4;
}
QListWidget#configNavigation::item {
    border-radius: 8px; padding: 11px 10px; margin: 2px 0;
}
QListWidget#configNavigation::item:hover { background: %6; }
QListWidget#configNavigation::item:selected {
    background: %7; color: %4; font-weight: 600;
}
QTabWidget#configPageStack::pane { border: 0; background: transparent; }
QLabel#configSectionHeading {
    color: %5; font-size: 15px; font-weight: 700; padding: 8px 0 3px 0;
}
QWidget#configGeneralPage QGroupBox,
QWidget#configOcrPage QGroupBox {
    color: %4; font-weight: 600; border: 1px solid %3;
    border-radius: 9px; margin-top: 16px; padding: 12px 9px 9px 9px;
    background: %2;
}
QWidget#configGeneralPage QGroupBox::title,
QWidget#configOcrPage QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left;
    left: 12px; padding: 0 6px; background: %2;
}
QPushButton {
    color: %4; background: %8; border: 1px solid %3;
    border-radius: 8px; min-height: 32px; padding: 4px 12px;
}
QPushButton:hover { background: %6; }
QPushButton:disabled { color: %5; }
QLineEdit, QPlainTextEdit, QTextEdit {
    color: %4; background: %8; border: 1px solid %3;
    border-radius: 7px; padding: 5px 8px;
}
QCheckBox { color: %4; spacing: 9px; min-height: 29px; }
QComboBox, QSpinBox, QDoubleSpinBox { min-height: 32px; }
QTableWidget, QListWidget#configButtonList {
    background: %8; color: %4; border: 1px solid %3;
    border-radius: 7px;
}
QTabBar::tab {
    padding: 9px 13px; min-height: 26px;
    color: %4; background: %2;
    border: 1px solid %3; border-radius: 6px;
}
QTabBar::tab:selected { background: %7; font-weight: 600; }
)QSS")
                    .arg(bg)
                    .arg(surface)
                    .arg(outline)
                    .arg(fg)
                    .arg(muted)
                    .arg(hover)
                    .arg(selected)
                    .arg(formBg));

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Configuration"));

    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            &ConfigWindow::updateChildren);

    QColor background = this->palette().window().color();
    bool isDark = ColorUtils::colorIsDark(background);
    QString modifier =
      isDark ? PathInfo::whiteIconPath() : PathInfo::blackIconPath();

    // general
    m_generalConfig = new GeneralConf();
    m_generalConfigTab = new QWidget();
    m_generalConfigTab->setObjectName(QStringLiteral("configGeneralPage"));
    auto* generalConfigLayout = new QVBoxLayout(m_generalConfigTab);
    m_generalConfigTab->setLayout(generalConfigLayout);
    generalConfigLayout->addWidget(m_generalConfig);
    m_tabWidget->addTab(
      m_generalConfigTab, QIcon(modifier + "config.svg"), tr("General"));
    // OCR
    m_ocrConfig = new OcrConf();
    m_ocrConfigTab = new QWidget();
    m_ocrConfigTab->setObjectName(QStringLiteral("configOcrPage"));
    auto* ocrConfigLayout = new QVBoxLayout(m_ocrConfigTab);
    m_ocrConfigTab->setLayout(ocrConfigLayout);
    ocrConfigLayout->addWidget(m_ocrConfig);
    m_tabWidget->addTab(
      m_ocrConfigTab, QIcon(modifier + "ocr.svg"), tr("OCR"));

    // visuals
    m_visuals = new VisualsEditor();
    m_visualsTab = new QWidget();
    auto* visualsLayout = new QVBoxLayout(m_visualsTab);
    m_visualsTab->setLayout(visualsLayout);
    visualsLayout->addWidget(m_visuals);
    m_tabWidget->addTab(
      m_visualsTab, QIcon(modifier + "graphics.svg"), tr("Interface"));

    // filename
    m_filenameEditor = new FileNameEditor();
    m_filenameEditorTab = new QWidget();
    auto* filenameEditorLayout = new QVBoxLayout(m_filenameEditorTab);
    m_filenameEditorTab->setLayout(filenameEditorLayout);
    filenameEditorLayout->addWidget(m_filenameEditor);
    m_tabWidget->addTab(m_filenameEditorTab,
                        QIcon(modifier + "name_edition.svg"),
                        tr("Filename Editor"));

    // shortcuts
    m_shortcuts = new ShortcutsWidget();
    m_shortcutsTab = new QWidget();
    auto* shortcutsLayout = new QVBoxLayout(m_shortcutsTab);
    m_shortcutsTab->setLayout(shortcutsLayout);
    shortcutsLayout->addWidget(m_shortcuts);
    m_tabWidget->addTab(
      m_shortcutsTab, QIcon(modifier + "shortcut.svg"), tr("Shortcuts"));

    // Preserve the original page instances, order, settings, error banners,
    // and update signals. The list is only an alternate navigation control.
    for (int index = 0; index < m_tabWidget->count(); ++index) {
        auto* item = new QListWidgetItem(m_tabWidget->tabIcon(index),
                                         m_tabWidget->tabText(index));
        navigation->addItem(item);
    }
    m_tabWidget->tabBar()->hide();
    connect(navigation,
            &QListWidget::currentRowChanged,
            this,
            [this](int index) {
                if (index >= 0 && index < m_tabWidget->count()) {
                    m_tabWidget->setCurrentIndex(index);
                }
            });
    connect(m_tabWidget,
            &QTabWidget::currentChanged,
            this,
            [this, navigation, pageTitle](int index) {
                if (index >= 0 && index < m_tabWidget->count()) {
                    if (navigation->currentRow() != index) {
                        navigation->setCurrentRow(index);
                    }
                    pageTitle->setText(m_tabWidget->tabText(index));
                }
            });
    navigation->setCurrentRow(0);
    pageTitle->setText(m_tabWidget->tabText(0));

    // connect update sigslots
    connect(this,
            &ConfigWindow::updateChildren,
            m_filenameEditor,
            &FileNameEditor::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_visuals,
            &VisualsEditor::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_generalConfig,
            &GeneralConf::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_ocrConfig,
            &OcrConf::updateComponents);

    // Error indicator (this must come last)
    initErrorIndicator(m_visualsTab, m_visuals);
    initErrorIndicator(m_filenameEditorTab, m_filenameEditor);
    initErrorIndicator(m_generalConfigTab, m_generalConfig);
    initErrorIndicator(m_ocrConfigTab, m_ocrConfig);
    initErrorIndicator(m_shortcutsTab, m_shortcuts);
}

void ConfigWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}

void ConfigWindow::closeEvent(QCloseEvent* event)
{
    if (m_ocrConfig->preloadSavePending()) {
        event->ignore();
        QMessageBox::information(
          this,
          tr("Saving OCR settings"),
          tr("Please wait until the startup loading preference has been "
             "saved, then close the window again."));
        return;
    }

    QWidget::closeEvent(event);
}

void ConfigWindow::initErrorIndicator(QWidget* tab, QWidget* widget)
{
    auto* label = new QLabel(tab);
    auto* btnResolve = new QPushButton(tr("Resolve"), tab);
    auto* btnLayout = new QHBoxLayout();

    // Set up label
    label->setText(tr(
      "<b>Configuration file has errors. Resolve them before continuing.</b>"));
    label->setStyleSheet(QStringLiteral(":disabled { color: %1; }")
                           .arg(qApp->palette().color(QPalette::Text).name()));
    label->setVisible(ConfigHandler().hasError());

    // Set up "Show errors" button
    btnResolve->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    btnLayout->addWidget(btnResolve);
    btnResolve->setVisible(ConfigHandler().hasError());

    widget->setEnabled(!ConfigHandler().hasError());

    // Add label and button to the parent widget's layout
    auto* layout = static_cast<QBoxLayout*>(tab->layout());
    if (layout != nullptr) {
        layout->insertWidget(0, label);
        layout->insertLayout(1, btnLayout);
    } else {
        widget->layout()->addWidget(label);
        widget->layout()->addWidget(btnResolve);
    }

    // Sigslots
    connect(
      ConfigHandler::getInstance(), &ConfigHandler::error, widget, [=, this]() {
          widget->setEnabled(false);
          label->show();
          btnResolve->show();
      });
    connect(ConfigHandler::getInstance(),
            &ConfigHandler::errorResolved,
            widget,
            [=]() {
                widget->setEnabled(true);
                label->hide();
                btnResolve->hide();
            });
    connect(btnResolve, &QPushButton::clicked, this, [this]() {
        ConfigResolver().exec();
    });
}
