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

#ifndef PROPERTY_COLORCTRL_CTRL
#define PROPERTY_COLORCTRL_CTRL

#include <AzCore/base.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <QtWidgets/QWidget>
#include <AzQtComponents/Components/Widgets/ColorPicker.h>
#include "PropertyEditorAPI.h"

#pragma once

class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QToolButton;
class QLabel;

namespace AzToolsFramework
{
    using TransformColorCallback = AZStd::function<AZ::Color(const AZ::Color& /*color*/, uint32_t /* fromColorSpaceId*/, uint32_t /* toColorSpaceId*/)>;

    //! Can be used to configure PropertyColorCtrl, especially for handling conversions between color spaces.
    struct ColorEditorConfiguration
    {
        AZ_TYPE_INFO(AzToolsFramework::ColorEditorConfiguration, "{1B9DBA31-4C1F-4E0A-8EFC-2A3C9FEC78E8}");
        
        uint32_t m_propertyColorSpaceId;                                 //!< Color space of the color property being edited, an displayed in the PropertyColorCtrl's numeric field.
        uint32_t m_colorSwatchColorSpaceId;                              //!< Color space used to display the PropertyColorCtrl small color swatch.
        uint32_t m_colorPickerDialogColorSpaceId;                        //!< Color space used by the color picker dialog for numeric input, swatches, the dropper, etc.
        AZStd::unordered_map<uint32_t, AZStd::string> m_colorSpaceNames; //!< Lists display names for each of the available color space IDs
        TransformColorCallback m_transformColorCallback;                 //!< A custom function for transforming from one of the above color spaces to another.
        AzQtComponents::ColorPicker::Configuration m_colorPickerDialogConfiguration = AzQtComponents::ColorPicker::Configuration::RGB;
    };

    class PropertyColorCtrl
        : public QWidget
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(PropertyColorCtrl, AZ::SystemAllocator, 0);

        PropertyColorCtrl(QWidget* pParent = NULL);
        virtual ~PropertyColorCtrl();

        QColor value() const;

        QWidget* GetFirstInTabOrder();
        QWidget* GetLastInTabOrder();
        void UpdateTabOrder();

        void SetColorEditorConfiguration(const ColorEditorConfiguration& configuration);

    signals:
        void valueChanged(QColor newValue);
        void currentColorChanged(const QColor& color);
        void colorSelected(const QColor& color);
        void rejected();
        void editingFinished();

    public slots:
        void setValue(QColor val);

    protected slots:
        void openColorDialog();
        void onSelected(QColor color);
        void OnEditingFinished();
        void OnTextEdited(const QString& newText);

    private:
        void CreateColorDialog();
        void SetColor(QColor color, bool updateDialogColor = true);
        QColor convertFromString(const QString& string);

        ColorEditorConfiguration m_config;

        AZ::Color TransformColor(const AZ::Color& color, uint32_t fromColorSpaceId, uint32_t toColorSpaceId) const;
        QColor TransformColor(const QColor& color, uint32_t fromColorSpaceId, uint32_t toColorSpaceId) const;

        QToolButton* m_pDefaultButton;
        AzQtComponents::ColorPicker* m_pColorDialog;

        QColor m_originalColor;
        QColor m_lastSetColor;

        QLineEdit* m_colorEdit;
        QColor m_color;
    };

    template <class ValueType>
    class ColorHandlerCommon
        : public PropertyHandler<ValueType, PropertyColorCtrl>
    {
        AZ::u32 GetHandlerName(void) const override  { return AZ::Edit::UIHandlers::Color; }
        QWidget* GetFirstInTabOrder(PropertyColorCtrl* widget) override { return widget->GetFirstInTabOrder(); }
        QWidget* GetLastInTabOrder(PropertyColorCtrl* widget) override { return widget->GetLastInTabOrder(); }
        void UpdateWidgetInternalTabbing(PropertyColorCtrl* widget) override { widget->UpdateTabOrder(); }
    };


    class Vector3ColorPropertyHandler : QObject, public ColorHandlerCommon<AZ::Vector3>
    {
        // this is a Qt Object purely so it can connect to slots with context.  This is the only reason its in this header.
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(Vector3ColorPropertyHandler, AZ::SystemAllocator, 0);

        virtual QWidget* CreateGUI(QWidget* pParent) override;
        virtual void ConsumeAttribute(PropertyColorCtrl* GUI, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName) override;
        virtual void WriteGUIValuesIntoProperty(size_t index, PropertyColorCtrl* GUI, property_t& instance, InstanceDataNode* node) override;
        virtual bool ReadValuesIntoGUI(size_t index, PropertyColorCtrl* GUI, const property_t& instance, InstanceDataNode* node)  override;
    };

    class AZColorPropertyHandler : QObject, public ColorHandlerCommon<AZ::Color>
    {
        // this is a Qt Object purely so it can connect to slots with context.  This is the only reason its in this header.
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(AZColorPropertyHandler, AZ::SystemAllocator, 0);
        bool IsDefaultHandler() const override { return true;  }
        QWidget* CreateGUI(QWidget* pParent) override;
        void ConsumeAttribute(PropertyColorCtrl* GUI, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName) override;
        void WriteGUIValuesIntoProperty(size_t index, PropertyColorCtrl* GUI, property_t& instance, InstanceDataNode* node) override;
        bool ReadValuesIntoGUI(size_t index, PropertyColorCtrl* GUI, const property_t& instance, InstanceDataNode* node)  override;
    };

    void RegisterColorPropertyHandlers();
};

#endif
