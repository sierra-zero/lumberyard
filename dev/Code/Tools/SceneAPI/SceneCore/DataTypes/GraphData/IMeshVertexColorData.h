#pragma once

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

#include <AzCore/RTTI/RTTI.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            enum class ColorChannel : AZ::u8
            {
                Red = 0,
                Green,
                Blue,
                Alpha
            };

            struct Color
            {
                union
                {
                    struct
                    {
                        float red;
                        float green;
                        float blue;
                        float alpha;
                    };
                    float m_channels[4];
                };

                Color(float r, float g, float b, float a)
                    : red(r), green(g), blue(b), alpha(a)
                {
                }

                float GetChannel(ColorChannel channel) const
                {
                    return m_channels[static_cast<AZ::u8>(channel)];
                }
            };

            class IMeshVertexColorData
                : public IGraphObject
            {
            public:
                AZ_RTTI(IMeshVertexColorData, "{27659F76-1245-4549-87A6-AF4E8B94CD51}", IGraphObject);

                virtual ~IMeshVertexColorData() override = default;

                virtual size_t GetCount() const = 0;
                virtual const Color& GetColor(size_t index) const = 0;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ
