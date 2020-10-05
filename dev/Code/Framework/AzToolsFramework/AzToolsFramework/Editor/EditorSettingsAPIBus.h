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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/any.h>
#include <AzCore/std/string/string.h>

namespace AzToolsFramework
{
    //! Editor Settings API Bus
    class EditorSettingsAPIRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~EditorSettingsAPIRequests() = default;

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual AZStd::vector<AZStd::string> BuildSettingsList() = 0;

        using SettingOutcome = AZ::Outcome<AZStd::any, AZStd::string>;

        virtual SettingOutcome GetValue(const AZStd::string_view path) = 0;
        virtual SettingOutcome SetValue(const AZStd::string_view path, const AZStd::any& value) = 0;
    };

    using EditorSettingsAPIBus = AZ::EBus<EditorSettingsAPIRequests>;
}
