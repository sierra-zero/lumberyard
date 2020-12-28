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
#include <AzQtComponents/Components/Widgets/ElidingLabel.h>

#include <QWidget>
#include <QTimer>
AZ_PUSH_DISABLE_WARNING(4244 4251, "-Wunknown-warning-option") // 4244: conversion from 'int' to 'qint8', possible loss of data
                                                                // 4251: 'QLayoutItem::align': class 'QFlags<Qt::AlignmentFlag>' needs to have dll-interface to be used by clients of class 'QLayoutItem'
#include <QHBoxLayout>
AZ_POP_DISABLE_WARNING

namespace Ui
{
    class AssetEditorHeader
        : public QFrame
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR(AssetEditorHeader, AZ::SystemAllocator, 0);
       
        explicit AssetEditorHeader(QWidget* parent = nullptr);

        void setName(const QString& name);
        void setLocation(const QString& location);
        void setIcon(const QIcon& icon);
    private:
        QFrame* m_backgroundFrame;
        QHBoxLayout* m_backgroundLayout;
        QVBoxLayout* m_mainLayout;
        QLabel* m_iconLabel;

        QIcon m_icon;

        AzQtComponents::ElidingLabel* m_assetName;
        AzQtComponents::ElidingLabel* m_assetLocation;
    };
} // namespace Ui
