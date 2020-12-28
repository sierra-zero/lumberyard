/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "MainStatusBar.h"

#include "MainStatusBarItems.h"
#include <AzFramework/Asset/AssetSystemBus.h>
#include "CryLibrary.h" // for extension of current platform

#include "Core/QtEditorApplication.h"

#include "ProcessInfo.h"

#include "EngineSettingsManager.h"
#include "QtUtil.h"

#include <QLabel>
#include <QString>
#include <QPixmap>
#include <QHBoxLayout>
#if defined(Q_OS_WIN)
#include "QtWinExtras/qwinfunctions.h"
#endif
#include <QTimer>
#include <QTime>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QCheckBox>
#include <QWidgetAction>
#include <QMessageBox>
#include <QMutexLocker>
#include <QStringList>
#include <QFrame>
#include <QCursor>
#include <QStylePainter>
#include <QStyleOption>

#include <AzToolsFramework/SourceControl/SourceControlAPI.h>
#include <AzToolsFramework/SourceControl/QtSourceControlNotificationHandler.h>
#include <AzQtComponents/Components/Widgets/CheckBox.h>
#include <AzQtComponents/Components/Style.h>

const int iconTextSpacing = 3;
const int marginSpacing = 2;
const int spacerSpacing = 5;
const int spacerColor = 0x6F6D6D;

StatusBarItem::StatusBarItem(const QString& name, bool isClickable, MainStatusBar* parent, bool hasLeadingSpacer)
    : QWidget(parent)
    , m_isClickable(isClickable)
    , m_hasLeadingSpacer(hasLeadingSpacer)
{
    setObjectName(name);
}

StatusBarItem::StatusBarItem(const QString& name, MainStatusBar* parent, bool hasLeadingSpacer)
    : StatusBarItem(name, false, parent, hasLeadingSpacer)
{}

void StatusBarItem::SetText(const QString& text)
{
    if (text != m_text)
    {
        m_text = text;
        updateGeometry();
        update();
    }
}

void StatusBarItem::SetIcon(const QPixmap& icon)
{
    QIcon origIcon = m_icon;

    if (icon.isNull())
    {
        m_icon = icon;
    }
    else
    {
        // avoid generating new pixmaps if we don't need to.
        if (icon.height() != 16)
        {
            m_icon = icon.scaledToHeight(16);
        }
        else
        {
            m_icon = icon;
        }
    }

    if (icon.isNull() ^ origIcon.isNull())
    {
        updateGeometry();
    }

    // don't generate paintevents unless we absolutely have changed!
    if (origIcon.cacheKey() != m_icon.cacheKey())
    {
        update();
    }
}

void StatusBarItem::SetIcon(const QIcon& icon)
{
    QIcon origIcon = m_icon;

    m_icon = icon;

    if (icon.isNull() ^ origIcon.isNull())
    {
        updateGeometry();
    }

    // don't generate paintevents unless we absolutely have changed!
    if (origIcon.cacheKey() != m_icon.cacheKey())
    {
        update();
    }
}

void StatusBarItem::SetToolTip(const QString& tip)
{
    setToolTip(tip);
}

void StatusBarItem::mousePressEvent(QMouseEvent* e)
{
    if (m_isClickable && e->button() == Qt::LeftButton)
    {
        emit clicked();
    }
}

QSize StatusBarItem::sizeHint() const
{
    QSize hint(4, 20);
    if (!m_icon.isNull())
    {
        hint.rwidth() += 16;
    }
    if (!m_icon.isNull() && !CurrentText().isEmpty())
    {
        hint.rwidth() += iconTextSpacing; //spacing
    }
    auto fm = fontMetrics();
    hint.rwidth() += fm.horizontalAdvance(CurrentText());

    hint.rwidth() += 2 * marginSpacing;
    hint.rheight() += 2 * marginSpacing;

    hint.rwidth() += m_hasLeadingSpacer ? spacerSpacing : 0;

    return hint;
}

QSize StatusBarItem::minimumSizeHint() const
{
    return sizeHint();
}

void StatusBarItem::paintEvent(QPaintEvent* pe)
{
    QStylePainter painter(this);
    QStyleOption opt;
    opt.initFrom(this);
    painter.drawPrimitive(QStyle::PE_Widget, opt);

    auto rect = contentsRect();
    rect.adjust(marginSpacing, marginSpacing, -marginSpacing, -marginSpacing);

    QRect textRect = rect;
    if (m_hasLeadingSpacer)
    {
        textRect.adjust(spacerSpacing, 0, 0, 0);

    }
    if (!CurrentText().isEmpty())
    {
        painter.drawItemText(textRect, Qt::AlignLeft | Qt::AlignVCenter, this->palette(), true, CurrentText(), QPalette::WindowText);
    }

    if (!m_icon.isNull())
    {
        auto textWidth = textRect.width();
        if (textWidth > 0)
        {
            textWidth += iconTextSpacing; //margin
        }
        QRect iconRect = { textRect.left() + textWidth - textRect.height() - 1, textRect.top() + 2,  textRect.height() - 4, textRect.height() - 4 };
        m_icon.paint(&painter, iconRect, Qt::AlignCenter);
    }

    if (m_hasLeadingSpacer)
    {
        QPen pen{ spacerColor };
        painter.setPen(pen);
        painter.drawLine(spacerSpacing / 2, 3, spacerSpacing / 2, rect.height() + 2);
    }
}

QString StatusBarItem::CurrentText() const
{
    return m_text;
}

MainStatusBar* StatusBarItem::StatusBar() const
{
    return static_cast<MainStatusBar*>(parentWidget());
}

///////////////////////////////////////////////////////////////////////////////////////

MainStatusBar::MainStatusBar(QWidget* parent)
    : QStatusBar(parent)
{
    addPermanentWidget(new GeneralStatusItem(QStringLiteral("status"), this), 50);

    addPermanentWidget(new SourceControlItem(QStringLiteral("source_control"), this), 1);

    addPermanentWidget(new StatusBarItem(QStringLiteral("connection"), true, this, true), 1);

    addPermanentWidget(new StatusBarItem(QStringLiteral("game_info"), this, true), 1);

    addPermanentWidget(new MemoryStatusItem(QStringLiteral("memory"), this), 1);
}

void MainStatusBar::Init()
{
    //called on mainwindow initialization
    const int statusbarTimerUpdateInterval {
        500
    };                                             //in ms, so 2 FPS

    QString strGameInfo;
    ICVar* pCVar = gEnv->pConsole->GetCVar("sys_game_folder");
    if (pCVar)
    {
        strGameInfo = tr("GameFolder: '%1'").arg(QtUtil::ToQString(pCVar->GetString()));
    }
    pCVar = gEnv->pConsole->GetCVar("sys_dll_game");
    if (pCVar)
    {
        strGameInfo += QLatin1String(" - ") + tr("GameDLL: '%1'").arg(QtUtil::ToQString(pCVar->GetString()));
    }
    SetItem(QStringLiteral("game_info"), strGameInfo, tr("Game Info"), QPixmap());

    //ask for updates for items regulary. This is basically what MFC does
    auto timer = new QTimer(this);
    timer->setInterval(statusbarTimerUpdateInterval);
    connect(timer, &QTimer::timeout, this, &MainStatusBar::requestStatusUpdate);
    timer->start();
}

void MainStatusBar::SetStatusText(const QString& text)
{
    SetItem(QStringLiteral("status"), text, QString(), QPixmap());
}

QWidget* MainStatusBar::SetItem(QString indicatorName, QString text, QString tip, const QPixmap& icon)
{
    auto item = findChild<StatusBarItem*>(indicatorName, Qt::FindDirectChildrenOnly);
    assert(item);

    item->SetText(text);
    item->SetToolTip(tip);
    item->SetIcon(icon);

    return item;
}

QWidget* MainStatusBar::SetItem(QString indicatorName, QString text, QString tip, int iconId)
{
    static std::unordered_map<int, QPixmap> idImages {
        {
            IDI_BALL_DISABLED, QPixmap(QStringLiteral(":/statusbar/res/ball_disabled.ico")).scaledToHeight(16)
        },
        {
            IDI_BALL_OFFLINE, QPixmap(QStringLiteral(":/statusbar/res/ball_offline.ico")).scaledToHeight(16)
        },
        {
            IDI_BALL_ONLINE, QPixmap(QStringLiteral(":/statusbar/res/ball_online.ico")).scaledToHeight(16)
        },
        {
            IDI_BALL_PENDING, QPixmap(QStringLiteral(":/statusbar/res/ball_pending.ico")).scaledToHeight(16)
        }
    };

    auto search = idImages.find(iconId);

    return SetItem(indicatorName, text, tip, search == idImages.end() ? QPixmap() : search->second);
}


QWidget* MainStatusBar::GetItem(QString indicatorName)
{
    return findChild<QWidget*>(indicatorName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

SourceControlItem::SourceControlItem(QString name, MainStatusBar* parent)
    : StatusBarItem(name, true, parent)
    , m_sourceControlAvailable(false)
{
    if (AzToolsFramework::SourceControlConnectionRequestBus::HasHandlers())
    {
        AzToolsFramework::SourceControlNotificationBus::Handler::BusConnect();
        m_sourceControlAvailable = true;
    }

    InitMenu();
    connect(this, &StatusBarItem::clicked, this, &SourceControlItem::UpdateAndShowMenu);
}

SourceControlItem::~SourceControlItem()
{
    AzToolsFramework::SourceControlNotificationBus::Handler::BusDisconnect();
}

void SourceControlItem::UpdateAndShowMenu()
{
    if (m_sourceControlAvailable)
    {
        UpdateMenuItems();
        m_menu->popup(QCursor::pos());
    }
}

void SourceControlItem::ConnectivityStateChanged(const AzToolsFramework::SourceControlState state)
{
    m_SourceControlState = state;
    UpdateMenuItems();
}

void SourceControlItem::InitMenu()
{
    m_scIconOk = QIcon(":statusbar/res/source_control_connected.svg");
    m_scIconError = QIcon(":statusbar/res/source_control_error_v2.svg");
    m_scIconWarning = QIcon(":statusbar/res/source_control-warning_v2.svg");
    m_scIconDisabled = QIcon(":statusbar/res/source_control-not_setup.svg");

    if (m_sourceControlAvailable)
    {
        m_menu = std::make_unique<QMenu>();

        m_settingsAction = m_menu->addAction(tr("Settings"));
        m_checkBox = new QCheckBox(m_menu.get());
        m_checkBox->setText(tr("Enable"));
        AzQtComponents::CheckBox::applyToggleSwitchStyle(m_checkBox);
        m_enableAction = new QWidgetAction(m_menu.get());
        m_enableAction->setDefaultWidget(m_checkBox);
        m_menu->addAction(m_settingsAction);
        m_menu->addAction(m_enableAction);
        AzQtComponents::Style::addClass(m_menu.get(), "SourceControlMenu");
        m_enableAction->setCheckable(true);

        m_enableAction->setEnabled(true);

        {
            using namespace AzToolsFramework;
            m_SourceControlState = SourceControlState::Disabled;
            SourceControlConnectionRequestBus::BroadcastResult(m_SourceControlState, &SourceControlConnectionRequestBus::Events::GetSourceControlState);
            UpdateMenuItems();
        }

        connect(m_settingsAction, &QAction::triggered, this, [&]()
        {
            GetIEditor()->GetSourceControl()->ShowSettings();
        });

        connect(m_checkBox, &QCheckBox::stateChanged, this, [this](int state) {SetSourceControlEnabledState(state); });
    }
    else
    {
        SetIcon(m_scIconDisabled);
        SetToolTip(tr("No source control provided"));
    }
    SetText("P4V");
}

void SourceControlItem::SetSourceControlEnabledState(bool state)
{
    using SCRequest = AzToolsFramework::SourceControlConnectionRequestBus;
    SCRequest::Broadcast(&SCRequest::Events::EnableSourceControl, state);
    m_menu->hide();
}

void SourceControlItem::UpdateMenuItems()
{
    QString toolTip;
    bool disabled = false;
    bool errorIcon = false;

    switch (m_SourceControlState)
    {
    case AzToolsFramework::SourceControlState::Disabled:
        toolTip = tr("Perforce disabled");
        disabled = true;
        errorIcon = true;
        break;
    case AzToolsFramework::SourceControlState::ConfigurationInvalid:
        errorIcon = true;
        toolTip = tr("Perforce configuration invalid");
        break;
    case AzToolsFramework::SourceControlState::Active:
        toolTip = tr("Perforce connected");
        break;
    }

    m_settingsAction->setEnabled(!disabled);
    m_checkBox->setChecked(!disabled);
    m_checkBox->setText(disabled ? tr("Status: Offline") : tr("Status: Online"));
    SetIcon(errorIcon ? disabled ? m_scIconDisabled : m_scIconWarning : m_scIconOk);
    SetToolTip(toolTip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemoryStatusItem::MemoryStatusItem(QString name, MainStatusBar* parent)
    : StatusBarItem(name, parent)
{
    connect(parent, &MainStatusBar::requestStatusUpdate, this, &MemoryStatusItem::updateStatus);
    SetToolTip(tr("Memory usage"));
}

MemoryStatusItem::~MemoryStatusItem()
{
}

void MemoryStatusItem::updateStatus()
{
    ProcessMemInfo mi;
    CProcessInfo::QueryMemInfo(mi);

    uint64 nSizeMb = (uint64)(mi.WorkingSet / (1024 * 1024));

    SetText(QString("%1 Mb").arg(nSizeMb));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
GeneralStatusItem::GeneralStatusItem(QString name, MainStatusBar* parent)
    : StatusBarItem(name, parent)
{
    connect(parent, SIGNAL(messageChanged(QString)), this, SLOT(update()));
}

QString GeneralStatusItem::CurrentText() const
{
    if (!StatusBar()->currentMessage().isEmpty())
    {
        return StatusBar()->currentMessage();
    }

    return StatusBarItem::CurrentText();
}

#include <MainStatusBar.moc>
#include <MainStatusBarItems.moc>
