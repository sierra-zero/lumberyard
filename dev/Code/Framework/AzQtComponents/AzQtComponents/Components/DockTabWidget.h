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

#pragma once

#include <AzQtComponents/AzQtComponentsAPI.h>
#include <AzQtComponents/Components/Widgets/TabWidget.h>

#include <QMap>

class QDockWidget;
class QMouseEvent;

namespace AzQtComponents
{
    class DockTabBar;

    class AZ_QT_COMPONENTS_API DockTabWidget
        : public TabWidget
    {
        Q_OBJECT
    public:
        explicit DockTabWidget(QWidget* mainEditorWindow, QWidget* parent = nullptr);
        int addTab(QDockWidget* page);
        void removeTab(int index);
        void removeTab(QDockWidget* page);
        bool closeTabs();
        void moveTab(int from, int to);
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void finishDrag();

        static bool IsTabbed(QDockWidget* dockWidget);
        static DockTabWidget* ParentTabWidget(QDockWidget* dockWidget);

    Q_SIGNALS:
        void tabIndexPressed(int index);
        void tabCountChanged(int count);
        void tabWidgetInserted(QWidget* widget);
        void undockTab(int index);
        void tabBarDoubleClicked();

    protected:
        void closeEvent(QCloseEvent* event) override;
        void contextMenuEvent(QContextMenuEvent* event) override;
        void tabInserted(int index) override;
        void tabRemoved(int index) override;

    protected Q_SLOTS:
        bool handleTabCloseRequested(int index);
        void handleTabAboutToClose();
        void handleTabIndexPressed(int index);

    private:
        DockTabBar* m_tabBar;
        QWidget* m_mainEditorWindow;

        AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
        QMap<QDockWidget*, QMetaObject::Connection> m_titleBarChangedConnections;
        AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING
    };
} // namespace AzQtComponents
