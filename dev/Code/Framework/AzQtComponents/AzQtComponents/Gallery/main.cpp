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

#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Jobs/JobManagerComponent.h>
#include <AzCore/Memory/PoolAllocator.h>
#include <AzCore/UserSettings/UserSettingsComponent.h>

#include <AzFramework/Asset/CustomAssetTypeComponent.h>

#include <AzToolsFramework/UI/PropertyEditor/PropertyManagerComponent.h>

#include <AzQtComponents/Components/GlobalEventFilter.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzQtComponents/Components/LumberyardStylesheet.h>
#include <AzQtComponents/Utilities/QtPluginPaths.h>
#include <AzQtComponents/Utilities/HandleDpiAwareness.h>
#include <AzQtComponents/Components/WindowDecorationWrapper.h>
#include "ComponentDemoWidget.h"

#include <AzCore/Memory/SystemAllocator.h>

#include <QApplication>
#include <QMainWindow>
#include <QSettings>

#ifdef Q_OS_WIN
#include <Windows.h>
#else
#include <iostream>
#endif

const QString g_ui_1_0_SettingKey = QStringLiteral("useUI_1_0");

static void LogToDebug(QtMsgType Type, const QMessageLogContext& Context, const QString& message)
{
#ifdef Q_OS_WIN
    OutputDebugStringW(L"Qt: ");
    OutputDebugStringW(reinterpret_cast<const wchar_t*>(message.utf16()));
    OutputDebugStringW(L"\n");
#else
    std::wcerr << L"Qt: " << message.toStdWString() << std::endl;
#endif
}

/*
 * Sets up and tears down everything we need for an AZ::ComponentApplication.
 * This is required for the ReflectedPropertyEditorPage.
 */
class ComponentApplicationWrapper
{
public:
    explicit ComponentApplicationWrapper()
    {
        AZ::ComponentApplication::Descriptor appDesc;
        m_systemEntity = m_componentApp.Create(appDesc);

        AZ::AllocatorInstance<AZ::PoolAllocator>::Create();
        AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Create();

        m_componentApp.RegisterComponentDescriptor(AZ::AssetManagerComponent::CreateDescriptor());
        m_componentApp.RegisterComponentDescriptor(AZ::JobManagerComponent::CreateDescriptor());
        m_componentApp.RegisterComponentDescriptor(AZ::UserSettingsComponent::CreateDescriptor());
        m_componentApp.RegisterComponentDescriptor(AzFramework::CustomAssetTypeComponent::CreateDescriptor());
        m_componentApp.RegisterComponentDescriptor(AzToolsFramework::Components::PropertyManagerComponent::CreateDescriptor());

        m_systemEntity->CreateComponent<AZ::AssetManagerComponent>();
        m_systemEntity->CreateComponent<AZ::JobManagerComponent>();
        m_systemEntity->CreateComponent<AZ::UserSettingsComponent>();
        m_systemEntity->CreateComponent<AzFramework::CustomAssetTypeComponent>();
        m_systemEntity->CreateComponent<AzToolsFramework::Components::PropertyManagerComponent>();

        m_systemEntity->Init();
        m_systemEntity->Activate();

        m_serializeContext = m_componentApp.GetSerializeContext();
        m_serializeContext->CreateEditContext();
    }

    ~ComponentApplicationWrapper()
    {
        m_serializeContext->DestroyEditContext();
        m_systemEntity->Deactivate();

        std::vector<AZ::Component*> components =
        {
            m_systemEntity->FindComponent<AZ::AssetManagerComponent>(),
            m_systemEntity->FindComponent<AZ::JobManagerComponent>(),
            m_systemEntity->FindComponent<AZ::UserSettingsComponent>(),
            m_systemEntity->FindComponent<AzFramework::CustomAssetTypeComponent>(),
            m_systemEntity->FindComponent<AzToolsFramework::Components::PropertyManagerComponent>()
        };

        for (auto component : components)
        {
            m_systemEntity->RemoveComponent(component);
            delete component;
        }

        m_componentApp.UnregisterComponentDescriptor(AZ::AssetManagerComponent::CreateDescriptor());
        m_componentApp.UnregisterComponentDescriptor(AZ::JobManagerComponent::CreateDescriptor());
        m_componentApp.UnregisterComponentDescriptor(AZ::UserSettingsComponent::CreateDescriptor());
        m_componentApp.UnregisterComponentDescriptor(AzFramework::CustomAssetTypeComponent::CreateDescriptor());
        m_componentApp.UnregisterComponentDescriptor(AzToolsFramework::Components::PropertyManagerComponent::CreateDescriptor());

        AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::PoolAllocator>::Destroy();

        m_componentApp.Destroy();
    }

private:
    // ComponentApplication must not be a pointer - it cannot be dynamically allocated
    AZ::ComponentApplication m_componentApp;
    AZ::Entity* m_systemEntity = nullptr;
    AZ::SerializeContext* m_serializeContext = nullptr;
};

int main(int argc, char **argv)
{
    ComponentApplicationWrapper componentApplicationWrapper;

    QApplication::setOrganizationName("Amazon");
    QApplication::setOrganizationDomain("amazon.com");
    QApplication::setApplicationName("LumberyardWidgetGallery");

    AzQtComponents::PrepareQtPaths();

    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    qInstallMessageHandler(LogToDebug);

    AzQtComponents::Utilities::HandleDpiAwareness(AzQtComponents::Utilities::PerScreenDpiAware);
    QApplication app(argc, argv);

    auto globalEventFilter = new AzQtComponents::GlobalEventFilter(&app);
    app.installEventFilter(globalEventFilter);

    AzQtComponents::StyleManager styleManager(&app);
    styleManager.Initialize(&app);

    QSettings settings;
    bool legacyUISetting = settings.value(g_ui_1_0_SettingKey, false).toBool();
    styleManager.SwitchUI(&app, legacyUISetting);

    auto wrapper = new AzQtComponents::WindowDecorationWrapper(AzQtComponents::WindowDecorationWrapper::OptionNone);
    auto widget = new ComponentDemoWidget(legacyUISetting, wrapper);
    wrapper->setGuest(widget);
    widget->resize(550, 900);
    widget->show();

    wrapper->enableSaveRestoreGeometry("windowGeometry");
    wrapper->restoreGeometryFromSettings();

    QObject::connect(widget, &ComponentDemoWidget::styleChanged, &styleManager, [&styleManager, &app, &settings](bool checked) {
        styleManager.SwitchUI(&app, checked);

        settings.setValue(g_ui_1_0_SettingKey, checked);
        settings.sync();
    });

    QObject::connect(widget, &ComponentDemoWidget::refreshStyle, &styleManager, [&styleManager, &app]() {
        styleManager.Refresh(&app);
    });

    app.setQuitOnLastWindowClosed(true);

    app.exec();
}
