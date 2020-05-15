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

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphStateMachine.h>
#include <EMotionFX/Source/BlendTree.h>
#include <EMotionFX/Source/BlendTreeParameterNode.h>

namespace EMotionFX
{
    class EmptyAnimGraph
        : public AnimGraph
    {
    public:
        AZ_RTTI(EmptyAnimGraph, "{B4BFE0F0-3A7D-4D90-A4C5-219F0A8E3997}", AnimGraph)

        EmptyAnimGraph();
        static void Reflect(AZ::ReflectContext* context);
        AnimGraphInstance* GetAnimGraphInstance(ActorInstance* actorInstance, MotionSet* motionSet);
    };

    ///////////////////////////////////////////////////////////////////////////

    class TwoMotionNodeAnimGraph
        : public EmptyAnimGraph
    {
    public:
        AZ_RTTI(TwoMotionNodeAnimGraph, "{CBF4DE6B-BCDA-42A4-8AAC-1184019459CA}", EmptyAnimGraph)

        TwoMotionNodeAnimGraph();
        static void Reflect(AZ::ReflectContext* context);
        AnimGraphMotionNode* GetMotionNodeA();
        AnimGraphMotionNode* GetMotionNodeB();

    private:
        AnimGraphMotionNode* m_motionNodeA = nullptr;
        AnimGraphMotionNode* m_motionNodeB = nullptr;
    };

    ///////////////////////////////////////////////////////////////////////////

    class OneBlendTreeNodeAnimGraph
        : public EmptyAnimGraph
    {
    public:
        AZ_RTTI(OneBlendTreeNodeAnimGraph, "{C939CFD0-B50F-4694-8CDD-5E8C7A10CE58}", AnimGraph)

        OneBlendTreeNodeAnimGraph();
        static void Reflect(AZ::ReflectContext* context);
        BlendTree* GetBlendTreeNode() const;

    private:
        BlendTree* m_blendTree = nullptr;
    };

    ///////////////////////////////////////////////////////////////////////////

    class OneBlendTreeParameterNodeAnimGraph
        : public EmptyAnimGraph
    {
    public:
        OneBlendTreeParameterNodeAnimGraph();
        BlendTreeParameterNode* GetParameterNode() const;

    private:
        BlendTreeParameterNode* m_parameterNode = nullptr;
    };

    ///////////////////////////////////////////////////////////////////////////

    class AnimGraphFactory
    {
    public:
        template <class AnimGraphType, class... Args>
        static AZStd::unique_ptr<AnimGraphType> Create(Args&&... args)
        {
            AZStd::unique_ptr<AnimGraphType> animGraph = AZStd::make_unique<AnimGraphType>(AZStd::forward(args)...);
            return animGraph;
        }

        static void ReflectTestTypes(AZ::ReflectContext* context)
        {
            // Reflect all subclasses of animgraph in AnimGraphFactory
            EMotionFX::EmptyAnimGraph::Reflect(context);
            EMotionFX::TwoMotionNodeAnimGraph::Reflect(context);
            EMotionFX::OneBlendTreeNodeAnimGraph::Reflect(context);
        }
    };
} // namespace EMotionFX