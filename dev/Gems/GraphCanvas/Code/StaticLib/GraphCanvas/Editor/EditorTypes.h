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

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/any.h>
#include <AzCore/std/containers/unordered_set.h>

namespace GraphCanvas
{
    typedef AZ::Crc32 EditorId;
    typedef AZ::EntityId GraphId;

    typedef AZ::EntityId ViewId;

    typedef AZ::EntityId SlotId;
    typedef AZ::EntityId NodeId;
    typedef AZ::EntityId ConnectionId;
    typedef AZ::EntityId BookmarkId;

    typedef AZ::EntityId DockWidgetId;

    typedef AZ::EntityId GraphicsEffectId;

    typedef AZ::EntityId ToastId;

    typedef AZ::Uuid PersistentGraphMemberId;

    typedef AZ::Crc32 ExtenderId;

    namespace Styling
    {
        // Style of curve for connection lines
        enum class ConnectionCurveType : AZ::u32
        {
            Straight,
            Curved
        };
    }

    enum class DataSlotType
    {
        Unknown,

        // These are options that can be used on most DataSlots
        Value,
        Reference
    };

    enum class DataValueType
    {
        Unknown,
        
        Primitive,

        // Container types
        Container
    };

    // Used to signal a drag/drop state
    enum class DragDropState
    {
        Unknown,
        Idle,
        Valid,
        Invalid
    };

    // Signals out which side of the connection is attempting to be moved.
    enum class ConnectionMoveType
    {
        Unknown,
        Source,
        Target
    };

    enum class ToastType
    {
        Information,
        Warning,
        Error,
        Custom
    };

    enum class ListingType
    {
        Unknown,
        WhiteList,
        BlackList
    };

    template<typename T>
    class TypeListingConfiguration
    {
    public:
        TypeListingConfiguration() = default;
        ~TypeListingConfiguration() = default;

        bool AllowsType(const T& type) const
        {
            size_t count = m_listing.count(type);

            return (count > 0 && m_listingType == ListingType::BlackList) || (count == 0 && m_listingType == ListingType::WhiteList);
        }

        ListingType             m_listingType  = ListingType::BlackList;
        AZStd::unordered_set<T> m_listing;

    private:
    };

    class ToastConfiguration
    {
    public:
        ToastConfiguration(ToastType toastType, const AZStd::string& titleLabel, const AZStd::string& descriptionLabel)
            : m_toastType(toastType)
            , m_titleLabel(titleLabel)
            , m_descriptionLabel(descriptionLabel)
        {
        }

        ~ToastConfiguration() = default;

        ToastType GetToastType() const
        {
            return m_toastType;
        }

        const AZStd::string& GetTitleLabel() const
        {
            return m_titleLabel;
        }

        const AZStd::string& GetDescriptionLabel() const
        {
            return m_descriptionLabel;
        }

        void SetCustomToastImage(const AZStd::string& toastImage)
        {
            AZ_Error("GraphCanvas", m_toastType == ToastType::Custom, "Setting a custom image on a non-custom Toast notification");
            m_customToastImage = toastImage;
        }

        const AZStd::string& GetCustomToastImage() const
        {
            return m_customToastImage;
        }

        void SetDuration(AZStd::chrono::milliseconds duration)
        {
            m_duration = duration;
        }

        AZStd::chrono::milliseconds GetDuration() const
        {
            return m_duration;
        }

        void SetCloseOnClick(bool closeOnClick)
        {
            m_closeOnClick = closeOnClick;
        }

        bool GetCloseOnClick() const
        {
            return m_closeOnClick;
        }

        void SetFadeDuration(AZStd::chrono::milliseconds fadeDuration)
        {
            m_fadeDuration = fadeDuration;
        }

        AZStd::chrono::milliseconds GetFadeDuration() const
        {
            return m_fadeDuration;
        }

    private:

        AZStd::chrono::milliseconds m_fadeDuration = AZStd::chrono::milliseconds(250);

        AZStd::chrono::milliseconds m_duration;
        bool m_closeOnClick;

        AZStd::string m_customToastImage;

        ToastType m_toastType;
        AZStd::string   m_titleLabel;
        AZStd::string   m_descriptionLabel;
    };

    struct ConnectionValidationTooltip
    {
        bool operator()() const
        {
            return m_isValid;
        }

        bool m_isValid;

        NodeId m_nodeId;
        SlotId m_slotId;

        AZStd::string m_failureReason;
    };

    typedef AZ::Outcome<void, AZStd::string> CanHandleMimeEventOutcome;
}
