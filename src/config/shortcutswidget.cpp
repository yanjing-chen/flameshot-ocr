// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Yurii Puchkov at Namecheap & Contributors

#include "shortcutswidget.h"
#include "config/setshortcutwidget.h"
#include "core/qguiappcurrentscreen.h"
#include "tools/capturetool.h"
#include "tools/toolfactory.h"
#include "utils/globalvalues.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QCursor>
#include <QDir>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QRect>
#include <QScreen>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

ShortcutsWidget::ShortcutsWidget(QWidget* parent)
  : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Hot Keys"));

    QRect position = frameGeometry();
    QScreen* screen = QGuiAppCurrentScreen().currentScreen();
    position.moveCenter(screen->availableGeometry().center());
    move(position.topLeft());

    m_layout = new QVBoxLayout(this);
    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->setSpacing(12);

#if defined(Q_OS_WIN)
    checkPrintScreenForcesSnipping();
#endif

    initInfoTable();
    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            &ShortcutsWidget::populateInfoTable);

#if defined(Q_OS_WIN)
    initMsScreenclipCheckbox();
#endif

    show();
}

void ShortcutsWidget::initInfoTable()
{
    m_table = new QTableWidget(this);
    m_table->setToolTip(tr("Available shortcuts in the screen capture mode."));

    // Retain the original table as the configuration/backing model only.
    // The visible interface below uses individually interactive preference rows.
    m_layout->addWidget(m_table);
    m_table->hide();
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("shortcutSearchField"));
    m_search->setPlaceholderText(tr("Search shortcuts..."));
    m_search->setClearButtonEnabled(true);
    m_layout->addWidget(m_search);
    m_rowsScroll = new QScrollArea(this);
    m_rowsScroll->setObjectName(QStringLiteral("shortcutPreferencesScroll"));
    m_rowsScroll->setFrameShape(QFrame::NoFrame);
    m_rowsScroll->setWidgetResizable(true);
    auto* body = new QWidget(m_rowsScroll);
    body->setObjectName(QStringLiteral("shortcutPreferencesBody"));
    m_rowsLayout = new QVBoxLayout(body);
    m_rowsLayout->setContentsMargins(3, 4, 3, 12);
    m_rowsLayout->setSpacing(12);
    m_rowsScroll->setWidget(body);
    m_layout->addWidget(m_rowsScroll, 1);
    connect(m_search, &QLineEdit::textChanged,
            this, [this](const QString&) { rebuildPreferenceRows(); });

    m_table->setColumnCount(2);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->hide();

    // header creation
    QStringList names;
    names << tr("Description") << tr("Key");
    m_table->setHorizontalHeaderLabels(names);
    connect(m_table,
            &QTableWidget::cellClicked,
            this,
            &ShortcutsWidget::onShortcutCellClicked);

    // populate with dynamic data
    populateInfoTable();

    // adjust size
    m_table->horizontalHeader()->setMinimumSectionSize(200);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSizePolicy(QSizePolicy::Expanding,
                                               QSizePolicy::Expanding);
    m_table->resizeColumnsToContents();
    m_table->resizeRowsToContents();
}

void ShortcutsWidget::populateInfoTable()
{
    loadShortcuts();
    m_table->setRowCount(m_shortcuts.size());

    // add content
    for (int i = 0; i < m_shortcuts.size(); ++i) {
        const auto current_shortcut = m_shortcuts.at(i);
        const auto identifier = current_shortcut.at(0);
        const auto description = current_shortcut.at(1);
        const auto key_sequence = current_shortcut.at(2);
        m_table->setItem(i, 0, new QTableWidgetItem(description));

#if defined(Q_OS_MACOS)
        auto* item = new QTableWidgetItem(nativeOSHotKeyText(key_sequence));
#else
        QTableWidgetItem* item = new QTableWidgetItem(key_sequence);
#endif
        item->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 1, item);

        if (identifier.isEmpty()) {
            QFont font;
            font.setBold(true);
            item->setFont(font);
            item->setFlags(item->flags() ^ Qt::ItemIsEnabled);
            m_table->item(i, 1)->setFont(font);
        }
    }

    // Read-only table items
    for (int x = 0; x < m_table->rowCount(); ++x) {
        for (int y = 0; y < m_table->columnCount(); ++y) {
            QTableWidgetItem* item = m_table->item(x, y);
            item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        }
    }
    if (m_rowsLayout) rebuildPreferenceRows();
}

void ShortcutsWidget::rebuildPreferenceRows()
{
    if (!m_rowsLayout || !m_search) return;
    while (auto* item = m_rowsLayout->takeAt(0)) {
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }
    const QString filter = m_search->text().trimmed();
    int extraStart = m_shortcuts.size();
    for (int i = 0; i < m_shortcuts.size(); ++i) {
        if (m_shortcuts.at(i).at(0) == QStringLiteral("TYPE_TOGGLE_PANEL")) {
            extraStart = i;
            break;
        }
    }
    QVBoxLayout* activeGroup = nullptr;
    int activeSection = -1;
    for (int i = 0; i < m_shortcuts.size(); ++i) {
        const auto& entry = m_shortcuts.at(i);
        const QString description = entry.at(1);
        const QString key = entry.at(2);
        if (!filter.isEmpty() &&
            !description.contains(filter, Qt::CaseInsensitive) &&
            !key.contains(filter, Qt::CaseInsensitive)) continue;
        const int section = i < extraStart ? 0 : 1;
        if (section != activeSection) {
            activeSection = section;
            auto* card = new QFrame(m_rowsScroll->widget());
            card->setObjectName(QStringLiteral("shortcutPreferencesCard"));
            auto* column = new QVBoxLayout(card);
            column->setContentsMargins(18, 16, 18, 16);
            column->setSpacing(7);
            auto* title = new QLabel(
              section == 0 ? tr("Capture tools") : tr("Other shortcuts"), card);
            title->setObjectName(QStringLiteral("shortcutSectionTitle"));
            column->addWidget(title);
            m_rowsLayout->addWidget(card);
            activeGroup = column;
        }
        auto* row = new QFrame(m_rowsScroll->widget());
        row->setObjectName(QStringLiteral("shortcutPreferenceRow"));
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(4, 7, 4, 7);
        line->setSpacing(14);
        auto* name = new QLabel(description, row);
        name->setObjectName(QStringLiteral("shortcutPreferenceName"));
        name->setWordWrap(true);
        line->addWidget(name, 1);
        const bool editable = !entry.at(0).isEmpty() &&
          m_table->item(i, 1) &&
          (m_table->item(i, 1)->flags() & Qt::ItemIsEnabled);
        if (editable) {
            auto* edit = new QPushButton(
              key.isEmpty() ? tr("Not assigned") : key, row);
            edit->setObjectName(QStringLiteral("shortcutKeyButton"));
            edit->setToolTip(tr("Click to change shortcut"));
            edit->setMinimumWidth(110);
            connect(edit, &QPushButton::clicked, this,
                    [this, i]() { onShortcutCellClicked(i, 1); });
            line->addWidget(edit, 0, Qt::AlignVCenter);
        } else {
            auto* fixed = new QLabel(key, row);
            fixed->setObjectName(QStringLiteral("shortcutFixedKey"));
            line->addWidget(fixed, 0, Qt::AlignVCenter);
        }
        activeGroup->addWidget(row);
    }
    m_rowsLayout->addStretch(1);
}


void ShortcutsWidget::onShortcutCellClicked(int row, int col)
{
    if (col == 1) {
        // Ignore non-changable shortcuts
        if (Qt::ItemIsEnabled !=
            (Qt::ItemIsEnabled & m_table->item(row, col)->flags())) {
            return;
        }

        QString shortcutName = m_shortcuts.at(row).at(0);
        auto* setShortcutDialog = new SetShortcutDialog(nullptr, shortcutName);
        if (0 != setShortcutDialog->exec()) {
            QKeySequence shortcutValue = setShortcutDialog->shortcut();

            // set no shortcut is Backspace
#if defined(Q_OS_MACOS)
            if (shortcutValue == QKeySequence(Qt::CTRL | Qt::Key_Backspace)) {
                shortcutValue = QKeySequence("");
            }
#else
            if (shortcutValue == QKeySequence(Qt::Key_Backspace)) {
                shortcutValue = QKeySequence("");
            }
#endif
            if (m_config.setShortcut(shortcutName, shortcutValue.toString())) {
                populateInfoTable();
            }
        }
        delete setShortcutDialog;
    }
}

void ShortcutsWidget::loadShortcuts()
{
    m_shortcuts.clear();
    auto buttonTypes = CaptureToolButton::getIterableButtonTypes();

    // get shortcuts names from capture buttons
    for (const CaptureTool::Type& t : buttonTypes) {
        CaptureTool* tool = ToolFactory().CreateTool(t);
        QString shortcutName = QVariant::fromValue(t).toString();
        appendShortcut(shortcutName, tool->description());
        if (shortcutName == "TYPE_COPY") {
            if (m_config.copyOnDoubleClick()) {
                m_shortcuts << (QStringList() << "" << tool->description()
                                              << tr("Left Double-click"));
            }
        }
        delete tool;
    }

    // additional tools that don't have their own buttons
    appendShortcut("TYPE_TOGGLE_PANEL", tr("Toggle side panel"));
    appendShortcut("TYPE_GRAB_COLOR", tr("Grab a color from the screen"));
    appendShortcut("TYPE_RESIZE_LEFT", tr("Resize selection left 1px"));
    appendShortcut("TYPE_RESIZE_RIGHT", tr("Resize selection right 1px"));
    appendShortcut("TYPE_RESIZE_UP", tr("Resize selection up 1px"));
    appendShortcut("TYPE_RESIZE_DOWN", tr("Resize selection down 1px"));
    appendShortcut("TYPE_SYM_RESIZE_LEFT",
                   tr("Symmetrically decrease width by 2px"));
    appendShortcut("TYPE_SYM_RESIZE_RIGHT",
                   tr("Symmetrically increase width by 2px"));
    appendShortcut("TYPE_SYM_RESIZE_UP",
                   tr("Symmetrically increase height by 2px"));
    appendShortcut("TYPE_SYM_RESIZE_DOWN",
                   tr("Symmetrically decrease height by 2px"));
    appendShortcut("TYPE_SELECT_ALL", tr("Select entire screen"));
    appendShortcut("TYPE_MOVE_LEFT", tr("Move selection left 1px"));
    appendShortcut("TYPE_MOVE_RIGHT", tr("Move selection right 1px"));
    appendShortcut("TYPE_MOVE_UP", tr("Move selection up 1px"));
    appendShortcut("TYPE_MOVE_DOWN", tr("Move selection down 1px"));
    appendShortcut("TYPE_COMMIT_CURRENT_TOOL", tr("Commit text in text area"));
    appendShortcut("TYPE_DELETE_CURRENT_TOOL",
                   tr("Delete selected drawn object"));
    appendShortcut("TYPE_CANCEL", tr("Cancel current selection"));

    // non-editable shortcuts have an empty shortcut name

    m_shortcuts << (QStringList() << "" << QObject::tr("Quit capture")
                                  << QKeySequence(Qt::Key_Escape).toString());

    // Global hotkeys
#if defined(Q_OS_MACOS)
    appendShortcut("TAKE_SCREENSHOT", tr("Capture screen"));
#ifdef ENABLE_IMGUR
    appendShortcut("SCREENSHOT_HISTORY", tr("Screenshot history"));
#endif
#elif defined(Q_OS_WIN)
    if (this->isPrintScreenKeyForSnippingDisabled()) {
        m_shortcuts << (QStringList() << "" << QObject::tr("Capture screen")
                                      << "Print Screen");
    }
    appendShortcut("TAKE_SCREENSHOT", tr("Capture screen"));
#ifdef ENABLE_IMGUR
    m_shortcuts << (QStringList() << "" << QObject::tr("Screenshot history")
                                  << "Shift+Print Screen");
#endif
#else
    // TODO - Linux doesn't support global shortcuts for (XServer and Wayland),
    // possibly it will be solved in the QHotKey library later. So it is
    // disabled for now.
#endif
    m_shortcuts << (QStringList()
                    << "" << QObject::tr("Show color picker") << "Right Click");
    m_shortcuts << (QStringList() << "" << QObject::tr("Change the tool's size")
                                  << "Mouse Wheel");
}

void ShortcutsWidget::appendShortcut(const QString& shortcutName,
                                     const QString& description)
{
    QString shortcut = ConfigHandler().shortcut(shortcutName);
    m_shortcuts << (QStringList()
                    << shortcutName
                    << QObject::tr(description.toStdString().c_str())
                    << shortcut.replace("Return", "Enter"));
}

#if defined(Q_OS_MACOS)
const QString& ShortcutsWidget::nativeOSHotKeyText(const QString& text)
{
    m_res = text;
    m_res.replace("Ctrl+", "⌘");
    m_res.replace("Alt+", "⌥");
    m_res.replace("Meta+", "⌃");
    m_res.replace("Shift+", "⇧");
    return m_res;
}
#endif

#if defined(Q_OS_WIN)
void ShortcutsWidget::checkPrintScreenForcesSnipping()
{
    if (!isPrintScreenKeyForSnippingDisabled() &&
        !ConfigHandler().ignorePrntScrForcesSnipping()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Flameshot");
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("It seems, that Windows forces to open its screenshot"
                          " tool when the 'Print Screen' key is pressed. Would "
                          "you like to disable this so that Flameshot can use "
                          "the 'Print Screen' key?") +
                       "\n\n" +
                       tr("Flameshot must be restarted for changes to take "
                          "effect."));
        QPushButton* yesBtn = msgBox.addButton(QMessageBox::Yes);
        QPushButton* noBtn = msgBox.addButton(QMessageBox::No);
        QPushButton* noDontAskAgainBtn =
          new QPushButton(tr("No, don't ask again"));
        msgBox.addButton(noDontAskAgainBtn, QMessageBox::RejectRole);
        msgBox.setDefaultButton(yesBtn);
        msgBox.exec();

        if (msgBox.clickedButton() == yesBtn) {
            if (!disablePrintScreenKeyForSnipping()) {
                QMessageBox::warning(
                  this, "Flameshot", tr("The registry could not be changed!"));
            }
        } else if (msgBox.clickedButton() == noDontAskAgainBtn) {
            ConfigHandler().setIgnorePrntScrForcesSnipping(true);
        }
    }
}

bool ShortcutsWidget::isPrintScreenKeyForSnippingDisabled()
{
    QSettings PrintKeyForSnipping("HKEY_CURRENT_USER\\Control Panel\\Keyboard",
                                  QSettings::NativeFormat);
    return PrintKeyForSnipping.value("PrintScreenKeyForSnippingEnabled", 1)
             .toInt() == 0;
}

bool ShortcutsWidget::disablePrintScreenKeyForSnipping()
{
    QSettings PrintKeyForSnipping("HKEY_CURRENT_USER\\Control Panel\\Keyboard",
                                  QSettings::NativeFormat);
    PrintKeyForSnipping.setValue("PrintScreenKeyForSnippingEnabled", 0);
    PrintKeyForSnipping.sync();
    if (QSettings::AccessError == PrintKeyForSnipping.status()) {
        return false;
    }
    return this->isPrintScreenKeyForSnippingDisabled();
}

void ShortcutsWidget::initMsScreenclipCheckbox()
{
    m_registerMsScreenclip =
      new QCheckBox(tr("Register Flameshot as MS-SCREENCLIP application "
                       "(administrator privileges required)"),
                    this);
    m_registerMsScreenclip->setToolTip(
      tr("After registering, you can select Flameshot as the default "
         "screenshot application in Windows Settings."));
    m_registerMsScreenclip->setChecked(isMsScreenclipRegistered());
    m_layout->addWidget(m_registerMsScreenclip);

    connect(
      m_registerMsScreenclip, &QCheckBox::clicked, this, [this](bool checked) {
          if (checked) {
              if (!registerMsScreenclip()) {
                  QMessageBox::warning(
                    this,
                    "Flameshot",
                    tr("The registry could not be changed!") + "\n" +
                      tr("You may start Flameshot as administrator ONCE and "
                         "try again!"));
                  m_registerMsScreenclip->setChecked(false);
              }
          } else {
              if (!unregisterMsScreenclip()) {
                  QMessageBox::warning(
                    this,
                    "Flameshot",
                    tr("The registry could not be changed!") + "\n" +
                      tr("You may start Flameshot as administrator ONCE and "
                         "try again!"));
                  m_registerMsScreenclip->setChecked(true);
              }
          }
      });
}

bool ShortcutsWidget::isMsScreenclipRegistered()
{
    QSettings URLAssociations(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\Flameshot\\Capabilities\\URLAssociations",
      QSettings::NativeFormat);
    QString value = URLAssociations.value("ms-screenclip", "").toString();
    if (value.toLower() != "flameshot")
        return false;

    QSettings RegisteredApplications(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\RegisteredApplications",
      QSettings::NativeFormat);
    value = RegisteredApplications.value("Flameshot", "").toString();
    if (value.toLower() !=
        QString("SOFTWARE\\Flameshot\\Capabilities").toLower())
        return false;

    QSettings FlameshotShellCmd(
      "HKEY_CURRENT_USER\\Software\\Classes\\Flameshot\\Shell\\Open\\command",
      QSettings::NativeFormat);
    value = FlameshotShellCmd.value(".").toString();
    if (value.toLower() != QString("\"" +
                                   QDir::toNativeSeparators(
                                     QCoreApplication::applicationFilePath()) +
                                   "\" gui")
                             .toLower())
        return false;

    return true; // All registry entries found
}

bool ShortcutsWidget::registerMsScreenclip()
{
    QSettings URLAssociations(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\Flameshot\\Capabilities\\URLAssociations",
      QSettings::NativeFormat);
    URLAssociations.setValue("ms-screenclip", "Flameshot");
    URLAssociations.sync();
    if (QSettings::AccessError == URLAssociations.status()) {
        return false;
    }

    QSettings RegisteredApplications(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\RegisteredApplications",
      QSettings::NativeFormat);
    RegisteredApplications.setValue("Flameshot",
                                    "SOFTWARE\\Flameshot\\Capabilities");
    RegisteredApplications.sync();
    if (QSettings::AccessError == RegisteredApplications.status()) {
        return false;
    }

    QSettings FlameshotShellCmd(
      "HKEY_CURRENT_USER\\Software\\Classes\\Flameshot\\Shell\\Open\\command",
      QSettings::NativeFormat);
    FlameshotShellCmd.setValue(
      ".",
      "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) +
        "\" gui");
    FlameshotShellCmd.sync();
    if (QSettings::AccessError == FlameshotShellCmd.status()) {
        return false;
    }

    return isMsScreenclipRegistered();
}

bool ShortcutsWidget::unregisterMsScreenclip()
{
    QSettings FlameshotShellCmd("HKEY_CURRENT_USER\\Software\\Classes",
                                QSettings::NativeFormat);
    FlameshotShellCmd.remove("Flameshot");
    FlameshotShellCmd.sync();
    if (QSettings::AccessError == FlameshotShellCmd.status()) {
        return false;
    }

    QSettings RegisteredApplications(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\RegisteredApplications",
      QSettings::NativeFormat);
    RegisteredApplications.remove("Flameshot");
    RegisteredApplications.sync();
    if (QSettings::AccessError == RegisteredApplications.status()) {
        return false;
    }

    QSettings URLAssociations(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\Flameshot\\Capabilities\\URLAssociations",
      QSettings::NativeFormat);
    URLAssociations.remove("ms-screenclip");
    URLAssociations.sync();
    if (QSettings::AccessError == URLAssociations.status()) {
        return false;
    }

    return !isMsScreenclipRegistered();
}

#endif
