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

#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Math/Quaternion.h>
#include <AtomCore/Math/Frustum.h>
#include <AtomCore/Math/Sphere.h>
#include <AtomCore/Math/ShapeIntersection.h>

namespace UnitTest
{    
    TEST(MATH_Sphere, Test)
    {
        AZ::Sphere unitSphere = AZ::Sphere::CreateUnitSphere();
        EXPECT_TRUE(unitSphere.GetCenter() == AZ::Vector3::CreateZero());
        EXPECT_TRUE(unitSphere.GetRadius() == 1.f);

        AZ::Sphere sphere1(AZ::Vector3(10.f, 10.f, 10.f), 15.f);
        EXPECT_TRUE(sphere1.GetCenter() == AZ::Vector3(10.f, 10.f,10.f));
        EXPECT_TRUE(sphere1.GetRadius() == 15.f);

        AZ::Sphere sphere2(AZ::Vector3(12.f, 12.f, 12.f), 13.f);
        EXPECT_TRUE(sphere2 != sphere1);

        sphere1.Set(sphere2);
        EXPECT_TRUE(sphere2 == sphere1);

        AZ::Sphere sphere3(AZ::Vector3(10.f, 10.f, 10.f), 15.f);
        sphere3.SetCenter(AZ::Vector3(12.f, 12.f, 12.f));
        sphere3.SetRadius(13.f);
        EXPECT_TRUE(sphere2 == sphere3);

        sphere2 = unitSphere;
        EXPECT_TRUE(sphere2 == unitSphere);
    }
    
    TEST(MATH_Frustum, Test)
    {
        //Assumes +x runs to the 'right', +y runs 'out' and +z points 'up'

        //A frustum is defined by 6 planes. In this case a box shape.
        AZ::Plane near = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 1.f, 0.f), AZ::Vector3(0.f, -5.f, 0.f));
        AZ::Plane far = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, -1.f, 0.f), AZ::Vector3(0.f, 5.f, 0.f));
        AZ::Plane left = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(1.f, 0.f, 0.f), AZ::Vector3(-5.f, 0.f, 0.f));
        AZ::Plane right = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(-1.f, 0.f, 0.f), AZ::Vector3(5.f, 0.f, 0.f));
        AZ::Plane top = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, -1.f), AZ::Vector3(0.f, 0.f, 5.f));
        AZ::Plane bottom = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, 1.f), AZ::Vector3(0.f, 0.f, -5.f));

        AZ::Frustum frustum(near, far, left, right, top, bottom);
        EXPECT_TRUE(frustum.GetNearPlane() == near);
        EXPECT_TRUE(frustum.GetFarPlane() == far);
        EXPECT_TRUE(frustum.GetLeftPlane() == left);
        EXPECT_TRUE(frustum.GetRightPlane() == right);
        EXPECT_TRUE(frustum.GetTopPlane() == top);
        EXPECT_TRUE(frustum.GetBottomPlane() == bottom);

        AZ::Plane near1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 1.f, 0.f), AZ::Vector3(0.f, -2.f, 0.f));
        AZ::Plane far1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, -1.f, 0.f), AZ::Vector3(0.f, 2.f, 0.f));
        AZ::Plane left1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(1.f, 0.f, 0.f), AZ::Vector3(-2.f, 0.f, 0.f));
        AZ::Plane right1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(-1.f, 0.f, 0.f), AZ::Vector3(2.f, 0.f, 0.f));
        AZ::Plane top1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, -1.f), AZ::Vector3(0.f, 0.f, 2.f));
        AZ::Plane bottom1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, 1.f), AZ::Vector3(0.f, 0.f, -2.f));
        AZ::Frustum frustum1(near1, far1, left1, right1, top1, bottom1);

        EXPECT_TRUE(frustum != frustum1);
        frustum.Set(frustum1);
        EXPECT_TRUE(frustum == frustum1);

        frustum.SetNearPlane(near);
        frustum.SetFarPlane(far);
        frustum.SetLeftPlane(left);
        frustum.SetRightPlane(right);
        frustum.SetTopPlane(top);
        frustum.SetBottomPlane(bottom);

        EXPECT_TRUE(frustum.GetNearPlane() == near);
        EXPECT_TRUE(frustum.GetFarPlane() == far);
        EXPECT_TRUE(frustum.GetLeftPlane() == left);
        EXPECT_TRUE(frustum.GetRightPlane() == right);
        EXPECT_TRUE(frustum.GetTopPlane() == top);
        EXPECT_TRUE(frustum.GetBottomPlane() == bottom);

        frustum = frustum1;
        EXPECT_TRUE(frustum == frustum1);

        //TODO: Test frustum creation from View-Projection Matrices
    }

    TEST(MATH_ShapeIntersection, Test)
    {
        //Assumes +x runs to the 'right', +y runs 'out' and +z points 'up'

        //A frustum is defined by 6 planes. In this case a box shape.
        AZ::Plane near = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 1.f, 0.f), AZ::Vector3(0.f, -5.f, 0.f));
        AZ::Plane far = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, -1.f, 0.f), AZ::Vector3(0.f, 5.f, 0.f));
        AZ::Plane left = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(1.f, 0.f, 0.f), AZ::Vector3(-5.f, 0.f, 0.f));
        AZ::Plane right = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(-1.f, 0.f, 0.f), AZ::Vector3(5.f, 0.f, 0.f));
        AZ::Plane top = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, -1.f), AZ::Vector3(0.f, 0.f, 5.f));
        AZ::Plane bottom = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, 1.f), AZ::Vector3(0.f, 0.f, -5.f));
        AZ::Frustum frustum(near, far, left, right, top, bottom);

        AZ::Plane near1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 1.f, 0.f), AZ::Vector3(0.f, -2.f, 0.f));
        AZ::Plane far1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, -1.f, 0.f), AZ::Vector3(0.f, 2.f, 0.f));
        AZ::Plane left1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(1.f, 0.f, 0.f), AZ::Vector3(-2.f, 0.f, 0.f));
        AZ::Plane right1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(-1.f, 0.f, 0.f), AZ::Vector3(2.f, 0.f, 0.f));
        AZ::Plane top1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, -1.f), AZ::Vector3(0.f, 0.f, 2.f));
        AZ::Plane bottom1 = AZ::Plane::CreateFromNormalAndPoint(AZ::Vector3(0.f, 0.f, 1.f), AZ::Vector3(0.f, 0.f, -2.f));
        AZ::Frustum frustum1(near1, far1, left1, right1, top1, bottom1);

        AZ::Sphere unitSphere = AZ::Sphere::CreateUnitSphere();
        AZ::Sphere sphere1(AZ::Vector3(10.f, 10.f, 10.f), 20.f);
        AZ::Sphere sphere2(AZ::Vector3(12.f, 12.f, 12.f), 13.f);

        AZ::Aabb unitBox = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3::CreateZero(), AZ::Vector3(1.f, 1.f, 1.f));
        AZ::Aabb aabb = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3(10.f, 10.f, 10.f), AZ::Vector3(1.f, 1.f, 1.f));
        AZ::Aabb aabb1 = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3(10.f, 10.f, 10.f), AZ::Vector3(100.f, 100.f, 100.f));

        AZ::Vector3 point(0.f, 0.f, 0.f);
        AZ::Vector3 point1(10.f, 10.f, 10.f);

        {
            AZ::Vector3 intersectionPoint;
            EXPECT_TRUE(AZ::ShapeIntersection::IntersectThreePlanes(near, left, bottom, intersectionPoint));
            EXPECT_TRUE(intersectionPoint.IsClose(AZ::Vector3(-5.f, -5.f, -5.f)));
            EXPECT_FALSE(AZ::ShapeIntersection::IntersectThreePlanes(near, far, bottom, intersectionPoint));
        }

        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(unitSphere, unitBox));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(unitSphere, aabb1));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(unitSphere, frustum));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(sphere1, frustum));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(unitSphere, sphere1));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(sphere1, unitSphere));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(frustum, unitBox));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(frustum, aabb1));
        EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(sphere1, far));

        EXPECT_FALSE(AZ::ShapeIntersection::Overlaps(frustum, aabb));
        EXPECT_FALSE(AZ::ShapeIntersection::Overlaps(unitSphere, aabb));
        EXPECT_FALSE(AZ::ShapeIntersection::Overlaps(unitSphere, sphere2));
        EXPECT_FALSE(AZ::ShapeIntersection::Overlaps(unitSphere, near));

        EXPECT_TRUE(AZ::ShapeIntersection::Contains(sphere1, unitSphere));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(frustum, unitSphere));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(unitSphere, unitSphere));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(sphere1, aabb));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(frustum, unitBox));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(unitSphere, point));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(sphere1, point1));
        EXPECT_TRUE(AZ::ShapeIntersection::Contains(frustum, point));

        EXPECT_FALSE(AZ::ShapeIntersection::Contains(frustum, sphere1));
        EXPECT_FALSE(AZ::ShapeIntersection::Contains(unitSphere, unitBox));
        EXPECT_FALSE(AZ::ShapeIntersection::Contains(frustum, aabb));
        EXPECT_FALSE(AZ::ShapeIntersection::Contains(unitSphere, point1));
        EXPECT_FALSE(AZ::ShapeIntersection::Contains(frustum, point1));

        {
            AZ::Sphere s(AZ::Vector3(0.0f, -4.4f, 0.0f), 0.5f);
            AZ::Sphere s1(AZ::Vector3(0.0f, -5.1f, 0.0f), 0.5f);
            AZ::Sphere s2(AZ::Vector3(0.0f, -5.6f, 0.0f), 0.5f);

            EXPECT_TRUE(AZ::ShapeIntersection::Classify(near, s) == AZ::ShapeIntersection::InFront);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(near, s1) == AZ::ShapeIntersection::Intersects);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(near, s2) == AZ::ShapeIntersection::Behind);

            EXPECT_TRUE(AZ::ShapeIntersection::Classify(frustum, s) == AZ::ShapeIntersection::Inside);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(frustum, s1) == AZ::ShapeIntersection::Touches);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(frustum, s2) == AZ::ShapeIntersection::Outside);
        }

        AZ::Vector3 axisX = AZ::Vector3::CreateAxisX();
        AZ::Vector3 axisY = AZ::Vector3::CreateAxisY();
        AZ::Vector3 axisZ = AZ::Vector3::CreateAxisZ();
        {
            AZ::Obb obb = AZ::Obb::CreateFromPositionAndAxes(AZ::Vector3(0.0f, -3.9f, 0.0f),
                axisX, 1.0f,
                axisY, 1.0f,
                axisZ, 1.0f);
            AZ::Obb obb1 = AZ::Obb::CreateFromPositionAndAxes(AZ::Vector3(0.0f, -5.1f, 0.0f),
                axisX, 1.0f,
                axisY, 1.0f,
                axisZ, 1.0f);
            AZ::Obb obb2 = AZ::Obb::CreateFromPositionAndAxes(AZ::Vector3(0.0f, -6.1f, 0.0f),
                axisX, 1.0f,
                axisY, 1.0f,
                axisZ, 1.0f);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(near, obb) == AZ::ShapeIntersection::InFront);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(near, obb1) == AZ::ShapeIntersection::Intersects);
            EXPECT_TRUE(AZ::ShapeIntersection::Classify(near, obb2) == AZ::ShapeIntersection::Behind);
        }

        //Test a bunch of different rotations with the OBBs
        AZ::Vector3 rotationAxis = AZ::Vector3(1.0f, 1.0f, 1.0f);
        rotationAxis.Normalize();
        float rotation = 0.0f;
        for(int i = 0; i < 50; ++i, rotation += (2.0f*3.1415/50.0f))
        {
            AZ::Vector3 x(1.0f, 0.0f, 0.0f);
            AZ::Vector3 y(0.0f, 1.0f, 0.0f);
            AZ::Vector3 z(0.0f, 0.0f, 1.0f);            
            AZ::Quaternion rot = AZ::Quaternion::CreateFromAxisAngle(rotationAxis, AZ::VectorFloat(rotation));
            x = rot * x;
            y = rot * y;
            z = rot * z;

            AZ::Obb obb = AZ::Obb::CreateFromPositionAndAxes(AZ::Vector3(0.7f, 0.7f, 0.7f),
                x, 1.0f,
                y, 1.0f,
                z, 1.0f);
            AZ::Obb obb1 = AZ::Obb::CreateFromPositionAndAxes(AZ::Vector3(6.8f, 6.8f, 6.8f),
                x, 1.0f,
                y, 1.0f,
                z, 1.0f);

            EXPECT_TRUE(AZ::ShapeIntersection::Overlaps(frustum, obb));
            EXPECT_FALSE(AZ::ShapeIntersection::Overlaps(frustum, obb1));
        }
    }
}
