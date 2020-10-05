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

// Description : World environment interface


#include "StdAfx.h"
#include "ParticleEnviron.h"
#include "ParticleEmitter.h"
#include "VisAreas.h"

#ifdef LY_TERRAIN_RUNTIME
#include <Terrain/Bus/TerrainProviderBus.h>
#endif
#include <Terrain/Bus/LegacyTerrainBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzFramework/Terrain/TerrainDataRequestBus.h>
#include <AzFramework/Physics/SystemBus.h>
#include <AzFramework/Physics/WindBus.h>
#include <AzFramework/Physics/World.h>
#include <MathConversion.h>

#if !PARTICLES_USE_CRY_PHYSICS
#include <CryPhysicsDeprecation.h>
#include <AzFramework/Physics/Casts.h>
#include <AzFramework/Physics/CollisionBus.h>
#include <AzFramework/Physics/MaterialBus.h>
#include <AzFramework/Physics/RigidBody.h>
#include <AzFramework/Physics/Shape.h>
#include <AzFramework/Physics/SystemBus.h>
#include <AzFramework/Physics/World.h>
#include "MathConversion.h"
#endif

//////////////////////////////////////////////////////////////////////////
// SPhysEnviron implementation.
//////////////////////////////////////////////////////////////////////////

void SPhysEnviron::Clear()
{
    m_UniformForces = SPhysForces(ZERO);
    m_tUnderWater = false;
    m_nNonUniformFlags = 0;
    m_nNonCachedFlags = 0;
    m_NonUniformAreas.resize(0);
}

void SPhysEnviron::FreeMemory()
{
    Clear();
    stl::free_container(m_NonUniformAreas);
}

void SPhysEnviron::OnPhysAreaChange()
{
    // Require re-querying of world physics areas
    m_nNonUniformFlags &= ~EFF_LOADED;
}

void SPhysEnviron::GetWorldPhysAreas(uint32 nFlags, bool bNonUniformAreas)
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    // Recycle shared SArea objects
    SmartPtrArray<SArea> aPrevAreas = m_NonUniformAreas;

    Clear();

    // Mark areas as queried.
    m_nNonUniformFlags |= EFF_LOADED;

    AZ::Aabb terrainAabb = AZ::Aabb::CreateFromPoint(AZ::Vector3::CreateZero());
    AzFramework::Terrain::TerrainDataRequestBus::BroadcastResult(terrainAabb, &AzFramework::Terrain::TerrainDataRequests::GetTerrainAabb);
    Vec3 vWorldSize(terrainAabb.GetWidth(), terrainAabb.GetHeight(), terrainAabb.GetDepth());

#if PARTICLES_USE_CRY_PHYSICS
    // Atomic iteration.
    for (IPhysicalEntity* pArea = 0; pArea = GetPhysicalWorld()->GetNextArea(pArea); )
    {
        // Initial check for areas of interest.
        int nAreaFlags = 0;
        Plane plWater = m_UniformForces.plWater;

        pe_params_area parea;
        if (!pArea->GetParams(&parea))
        {
            continue;
        }

        // Determine which forces are in area.
        if (!is_unused(parea.gravity))
        {
            nAreaFlags |= ENV_GRAVITY;
        }

        pe_params_buoyancy pbuoy;
        if (pArea->GetParams(&pbuoy))
        {
            if (pbuoy.iMedium == 1)
            {
                nAreaFlags |= ENV_WIND;
            }
            else if (pbuoy.iMedium == 0 && !is_unused(pbuoy.waterPlane.n))
            {
                plWater.SetPlane(pbuoy.waterPlane.n, pbuoy.waterPlane.origin);
                nAreaFlags |= ENV_WATER;
            }
        }

        // Skip if unneeded gravity or wind.
        // All emitters require m_tUnderwater intersection test.
        nAreaFlags &= nFlags;
        if (nAreaFlags == 0)
        {
            continue;
        }

        // Query area for intersection and forces, uniform only.
        pe_status_area sarea;
        sarea.ctr.zero();
        sarea.size = vWorldSize;
        sarea.bUniformOnly = true;
        int nStatus = pArea->GetStatus(&sarea);
        if (!nStatus)
        {
            continue;
        }

        if (nAreaFlags & ENV_WATER)
        {
            m_tUnderWater = ETrinary();
        }

        pe_status_pos spos;
        Matrix33 matArea;
        spos.pMtx3x3 = &matArea;
        if (!pArea->GetStatus(&spos))
        {
            continue;
        }

        // Get correct force directions.
        if (parea.bUniform == 2)
        {
            if (nAreaFlags & ENV_GRAVITY)
            {
                parea.gravity = spos.q * parea.gravity;
            }
            if (nAreaFlags & ENV_WIND)
            {
                pbuoy.waterFlow = spos.q * pbuoy.waterFlow;
            }
        }

        SPhysForces area_forces;
        area_forces.vAccel = nAreaFlags & ENV_GRAVITY ? parea.gravity : Vec3(0.f);
        area_forces.vWind = nAreaFlags & ENV_WIND ? pbuoy.waterFlow : Vec3(0.f);
        area_forces.plWater = plWater;

        if (nStatus < 0)
        {
            if (bNonUniformAreas)
            {
                // Add to NonUniformAreas list, re-using previous SAreas
                _smart_ptr<SArea> pAreaRec;
                for_array (i, aPrevAreas)
                {
                    if (aPrevAreas[i].m_pArea == pArea)
                    {
                        pAreaRec = &aPrevAreas[i];
                        aPrevAreas.erase(i);
                        break;
                    }
                }

                if (!pAreaRec)
                {
                    pAreaRec = new SArea;
                    pAreaRec->m_pArea = pArea;
                }

                SArea& area = *pAreaRec;
                m_NonUniformAreas.push_back(pAreaRec);

                area.m_pEnviron = this;
                area.m_nFlags = nAreaFlags;
                area.m_pLock = sarea.pLockUpdate;
                area.m_bbArea.min = spos.BBox[0] + spos.pos;
                area.m_bbArea.max = spos.BBox[1] + spos.pos;
                area.m_Forces = area_forces;

                if (nAreaFlags & ENV_WATER)
                {
                    // Expand water test area
                    area.m_bbArea.min.z = -fHUGE;
                }

                pe_params_foreign_data pfd;
                if (pArea->GetParams(&pfd))
                {
                    if (pfd.iForeignFlags & PFF_OUTDOOR_AREA)
                    {
                        area.m_bOutdoorOnly = true;
                    }
                }

                area.m_bCacheForce = false;

                if (nAreaFlags & (ENV_GRAVITY | ENV_WIND))
                {
                    // Test whether we can cache force information for quick evaluation.
                    int geom_type;
                    if (parea.pGeom && ((geom_type = parea.pGeom->GetType()) == GEOM_BOX || geom_type == GEOM_SPHERE))
                    {
                        area.m_bRadial = !parea.bUniform;
                        area.m_nGeomShape = geom_type;
                        area.m_fFalloffScale = div_min(1.f, 1.f - parea.falloff0, fHUGE);

                        // Construct world-to-unit-sphere transform.
                        area.m_vCenter = spos.pos;
                        area.m_matToLocal = (matArea * Matrix33::CreateScale(area.m_bbArea.GetSize() * 0.5f)).GetInverted();
                        area.m_bCacheForce = true;
                    }
                    if (!area.m_bCacheForce)
                    {
                        m_nNonCachedFlags |= nAreaFlags & (ENV_GRAVITY | ENV_WIND);
                    }
                }
            }
            m_nNonUniformFlags |= nAreaFlags;
        }
        else
        {
            // Uniform area.
            m_UniformForces.Add(area_forces, nAreaFlags);
        }
    }
#else // AZPhysics

    // Gravity
    if (Physics::World* world = SPhysEnviron::GetAZPhysicsWorld())
    {
        m_UniformForces.vAccel = AZVec3ToLYVec3(world->GetGravity());
    }

    // Global wind
    const Physics::WindRequests* windRequests = AZ::Interface<Physics::WindRequests>::Get();
    if (windRequests)
    {
        const AZ::Vector3 wind = windRequests->GetGlobalWind();
        m_UniformForces.vWind = AZVec3ToLYVec3(wind);
    }

#endif // PARTICLES_USE_CRY_PHYSICS
}

void SPhysEnviron::GetPhysAreas(SPhysEnviron const& envSource, AABB const& box, bool bIndoors, uint32 nFlags, bool bNonUniformAreas, const CParticleEmitter* pEmitterSkip)
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    Clear();

    // Mark areas as queried.
    m_nNonUniformFlags |= EFF_LOADED;

    m_UniformForces.vAccel = envSource.m_UniformForces.vAccel;
    if (!bIndoors)
    {
        m_UniformForces.vWind = envSource.m_UniformForces.vWind;
        m_UniformForces.plWater = envSource.m_UniformForces.plWater;

        // Set status of box with respect to water plane.
        float fDistMin, fDistMax;
        Distance::AABB_Plane(&fDistMin, &fDistMax, box, m_UniformForces.plWater);
        m_tUnderWater = fDistMin > 0.f ? ETrinary(false)
            : fDistMax < 0.f ? ETrinary(true)
            : ETrinary();

#if !PARTICLES_USE_CRY_PHYSICS
        // Local wind
        const Physics::WindRequests* windRequests = AZ::Interface<Physics::WindRequests>::Get();
        if (windRequests)
        {
            const AZ::Vector3 center = LYVec3ToAZVec3(box.GetCenter());
            const AZ::Vector3 halfExtents = LYVec3ToAZVec3(box.GetSize()) * 0.5f;
            const AZ::Aabb aabb = AZ::Aabb::CreateCenterHalfExtents(center, halfExtents);
            const AZ::Vector3 wind = windRequests->GetWind(aabb);

            m_UniformForces.vWind += AZVec3ToLYVec3(wind);
        }
#endif // PARTICLES_USE_CRY_PHYSICS
    }

    if (bNonUniformAreas && envSource.m_nNonUniformFlags & nFlags && !box.IsReset())
    {
#if PARTICLES_USE_CRY_PHYSICS
        pe_status_area sarea;
        sarea.ctr = box.GetCenter();
        sarea.size = box.GetSize() * 0.5f;
        sarea.vel.zero();
        sarea.bUniformOnly = true;        
#endif // PARTICLES_USE_CRY_PHYSICS

        for_array (i, envSource.m_NonUniformAreas)
        {
            SArea& area = envSource.m_NonUniformAreas[i];
            if (area.m_nFlags & nFlags)
            {
                // Query area for intersection and forces, uniform only.
                if (!bIndoors || !area.m_bOutdoorOnly)
                {
                    // Test bb intersection.
                    if (area.m_bbArea.IsIntersectBox(box))
                    {
                        if (pEmitterSkip)
                        {
#if PARTICLES_USE_CRY_PHYSICS
                            pe_params_foreign_data fd;
                            if (area.m_pArea->GetParams(&fd))
                            {
                                if (fd.pForeignData == pEmitterSkip)
                                {
                                    continue;
                                }
                            }
#else // AZPhysics
                            // Provide skipping for this emitter
                            CRY_PHYSICS_REPLACEMENT_ASSERT()
#endif // PARTICLES_USE_CRY_PHYSICS
                        }

#if PARTICLES_USE_CRY_PHYSICS
                        int nStatus = area.m_pArea->GetStatus(&sarea);

                        if (nStatus)
                        {
                            // Update underwater status.
                            if ((area.m_nFlags & ENV_WATER) && m_tUnderWater * false)
                            {
                                // Check box underwater status.
                                float fDistMin, fDistMax;
                                Distance::AABB_Plane(&fDistMin, &fDistMax, box, area.m_Forces.plWater);
                                if (fDistMax < 0.f)
                                {
                                    // Underwater.
                                    m_tUnderWater = nStatus < 0 ? ETrinary() : ETrinary(true);
                                }
                                else if (fDistMin < 0.f)
                                {
                                    // Partially underwater.
                                    m_tUnderWater = ETrinary();
                                }
                            }
                            if (area.m_nFlags & nFlags)
                            {
                                if (nStatus < 0)
                                {
                                    // Store in local nonuniform area list.
                                    m_NonUniformAreas.push_back(&area);
                                    m_nNonUniformFlags |= area.m_nFlags;
                                    if (!area.m_bCacheForce)
                                    {
                                        m_nNonCachedFlags |= area.m_nFlags & (ENV_GRAVITY | ENV_WIND);
                                    }
                                }
                                else
                                {
                                    // Uniform within this volume.
                                    m_UniformForces.Add(area.m_Forces, area.m_nFlags);
                                }
                            }
                        }
#else // AZPhysics
                        // Update m_tUnderWater undewater
                        // Add the to list of forces
                        CRY_PHYSICS_REPLACEMENT_ASSERT();
#endif // PARTICLES_USE_CRY_PHYSICS
                    }
                }
            }
        }
    }
}

void SPhysEnviron::GetNonUniformForces(SPhysForces& forces, Vec3 const& vPos, uint32 nFlags) const
{
    for_array (i, m_NonUniformAreas)
    {
        if (m_NonUniformAreas[i].m_nFlags & nFlags)
        {
            m_NonUniformAreas[i].GetForces(forces, vPos, nFlags);
        }
    }
}

bool SPhysEnviron::PhysicsCollision(CryParticleRayHit& hit, Vec3 const& vStart, Vec3 const& vEnd, float fRadius, uint32 nEnvFlags, CryParticleHitEntity* pTestEntity)
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    bool bHit = false;
    Vec3 vMove = vEnd - vStart;
    float fMoveSqr = vMove.GetLengthSquared();
    if (fMoveSqr == 0.f)
    {
        return false;
    }
    float fInvMove = isqrt_tpl(fMoveSqr);

    // Extend movement by particle radius.
    float fRadiusRel = fRadius * fInvMove;
    vMove *= 1.f + fRadiusRel;

    float fMoveNorm = 0.f;
    ZeroStruct(hit);
    hit.dist = 1.f;

    // Collide terrain first (if set as separately colliding).
    // NEW-TERRAIN LY-103227:  Need to make particle collisions work with new terrain system
    // NEW-TERRAIN LY-101543:  Need to replace specific terrain calls with abstracted API
    if ((nEnvFlags & ~ENV_COLLIDE_PHYSICS & ENV_TERRAIN) && !pTestEntity)
    {
        auto legacyTerrain = LegacyTerrain::LegacyTerrainDataRequestBus::FindFirstHandler();
        if (legacyTerrain)
        {
            nEnvFlags &= ~ENV_TERRAIN;
            LegacyTerrain::SRayTrace rt;
            if (legacyTerrain->RayTrace(vStart, vStart + vMove, &rt))
            {
                if ((fMoveNorm = rt.hitNormal * vMove) < 0.f)
                {
                    bHit = true;
                    hit.dist = rt.t;
                    hit.pt = rt.hitPoint;
                    hit.n = rt.hitNormal;
#if PARTICLES_USE_CRY_PHYSICS
                    if (rt.material)
                    {
                        hit.surface_idx = rt.material->GetSurfaceTypeId();
                    }
                    else
                    {
                        hit.surface_idx = 0;
                    }
#else                    
                    if (rt.material)
                    {
                        AZStd::weak_ptr<Physics::Material> hitMaterial;
                        Physics::PhysicsMaterialRequestBus::BroadcastResult(hitMaterial,
                            &Physics::PhysicsMaterialRequests::GetMaterialByName, rt.material->GetName());
                        hit.material = hitMaterial.lock().get();
                    }
                    if (!hit.material)
                    {
                        if (Physics::System* system = AZ::Interface<Physics::System>::Get())
                        {
                            hit.material = system->GetDefaultMaterial().get();
                        }
                    }
#endif // PARTICLES_USE_CRY_PHYSICS
                    hit.bTerrain = true;
                }
            }
        }
    }

    if (pTestEntity)
    {
#if PARTICLES_USE_CRY_PHYSICS
        // Test specified entity only.
        bHit = GetPhysicalWorld()->RayTraceEntity(pTestEntity, vStart, vMove, &hit)
            && hit.n * vMove < 0.f;
#else // AZPhysics

        Physics::RayCastRequest request;
        request.m_start = LYVec3ToAZVec3(vStart);
        request.m_distance = vMove.GetLength();
        request.m_direction = LYVec3ToAZVec3(vMove) / request.m_distance;

        Physics::RayCastHit result = pTestEntity->RayCast(request);
        if (result)
        {
            hit.dist = result.m_distance;
            hit.pCollider = result.m_body;
            hit.material = result.m_material;
            hit.n = AZVec3ToLYVec3(result.m_normal);
            hit.pt = AZVec3ToLYVec3(result.m_position);
            bHit = hit.n * vMove < 0.f;
        }
#endif // PARTICLES_USE_CRY_PHYSICS
    }
    else if (hit.dist > 0.f)
    {
        int ent_collide = 0;
        if (nEnvFlags & ENV_TERRAIN)
        {
            ent_collide |= ent_terrain;
        }
        if (nEnvFlags & ENV_STATIC_ENT)
        {
            ent_collide |= ent_static;
        }
        if (nEnvFlags & ENV_DYNAMIC_ENT)
        {
            ent_collide |= ent_rigid | ent_sleeping_rigid | ent_living | ent_independent;
        }

        // Only check collision if there any collision flags are enabled
        IF (ent_collide, false)
        {
#if PARTICLES_USE_CRY_PHYSICS
            // rwi_ flags copied from similar code in CParticleEntity.
            ray_hit hit_loc;
            if (GetPhysicalWorld()->RayWorldIntersection(
                    vStart, vMove * hit.dist,
                    ent_collide | ent_no_ondemand_activation,
                    sf_max_pierceable | (geom_colltype_ray | geom_colltype13) << rwi_colltype_bit | rwi_colltype_any | rwi_ignore_noncolliding,
                    &hit_loc, 1) > 0
                )
            {
                if ((fMoveNorm = hit_loc.n * vMove) < 0.f)
                {
                    bHit = true;
                    hit = hit_loc;
                    hit.dist *= fInvMove;
                }
            }
#else // AZPhysics

            const Physics::World* world = GetAZPhysicsWorld();
            if (world)
            {
                int collideFlags = nEnvFlags & ENV_COLLIDE_PHYSICS;
                if (collideFlags)
                {
                    Physics::RayCastRequest request;
                    switch (collideFlags)
                    {
                    case ENV_STATIC_ENT:
                        request.m_queryType = Physics::QueryType::Static;
                        break;
                    case ENV_DYNAMIC_ENT:
                        request.m_queryType = Physics::QueryType::Dynamic;
                        break;
                    case ENV_COLLIDE_PHYSICS:
                        request.m_queryType = Physics::QueryType::StaticAndDynamic;
                        break;
                    }

                    request.m_start = LYVec3ToAZVec3(vStart);
                    request.m_distance = vMove.GetLength();
                    request.m_direction = LYVec3ToAZVec3(vMove) / request.m_distance;
                    Physics::RayCastHit result;
                    Physics::WorldRequestBus::EventResult(result, world->GetWorldId(), &Physics::World::RayCast, request);
                    if (result)
                    {
                        fMoveNorm = AZVec3ToLYVec3(result.m_normal) * vMove;
                        if (fMoveNorm < 0.f)
                        {
                            hit.dist = result.m_distance * fInvMove;
                            hit.pCollider = result.m_body;
                            hit.material = result.m_material;
                            hit.n = AZVec3ToLYVec3(result.m_normal);
                            hit.pt = AZVec3ToLYVec3(result.m_position);
                            bHit = true;
                        }
                    }
                }
            }
#endif // PARTICLES_USE_CRY_PHYSICS
        }
    }

    if (bHit)
    {
        hit.pt += hit.n * fRadius;
        hit.dist -= div_min(fRadius, -fMoveNorm, 1.f);
        hit.dist = clamp_tpl(hit.dist, 0.f, 1.f);
    }
    return bHit;
}

void SPhysEnviron::SArea::GetForcesPhys(SPhysForces& forces, Vec3 const& vPos) const
{
    FUNCTION_PROFILER_SYS(PARTICLE);

#if PARTICLES_USE_CRY_PHYSICS
    pe_status_area sarea;
    sarea.ctr = vPos;
    if (m_pArea->GetStatus(&sarea))
    {
        if (!is_unused(sarea.gravity))
        {
            forces.vAccel = sarea.gravity;
        }
        if (sarea.pb.iMedium == 1 && !is_unused(sarea.pb.waterFlow))
        {
            forces.vWind += sarea.pb.waterFlow;
        }
    }
#else // AZPhysics
    AZ::Vector3 gravity = AZ::Vector3(0.f, 0.f, 0.f);
    if (Physics::World* world = SPhysEnviron::GetAZPhysicsWorld())
    {
        gravity = world->GetGravity();
    }
    forces.vAccel = AZVec3ToLYVec3(gravity);

    // Retrieve wind
    CRY_PHYSICS_REPLACEMENT_ASSERT();
#endif // PARTICLES_USE_CRY_PHYSICS
}

void SPhysEnviron::SArea::GetForces(SPhysForces& forces, Vec3 const& vPos, uint32 nFlags) const
{
    nFlags &= m_nFlags;

    if (nFlags & (ENV_GRAVITY | ENV_WIND))
    {
        if (!m_bCacheForce)
        {
            GetForcesPhys(forces, vPos);
        }
        else
        {
#ifdef DEBUG_FORCES
            // Compare locally computed results to physics system.
            // Occasionally fails because of threaded physics area updates.
            SPhysForces forcesPhys = forces;
            GetForcesPhys(forcesPhys, vPos);
#endif

            Vec3 vPosRel = vPos - m_vCenter;

            // Update with current position.
            if (!m_pEnviron->IsCurrent())
            {
#if PARTICLES_USE_CRY_PHYSICS
                pe_status_pos spos;
                if (m_pArea->GetStatus(&spos))
                {
                    vPosRel = vPos - spos.pos;
                }
#else // AZPhysics
                // Retrieve distance to the force area
                CRY_PHYSICS_REPLACEMENT_ASSERT();
#endif // PARTICLES_USE_CRY_PHYSICS
            }

            Vec3 vDist = m_matToLocal * vPosRel;
            
            // We want to avoid any Nan in the case vDist is zero.
            float fDist = 0;
            if ((vDist*vDist) != 0)
            {
                fDist = m_nGeomShape == GEOM_BOX ? max(max(abs(vDist.x), abs(vDist.y)), abs(vDist.z))
                    : vDist.GetLengthFast();
            }

            if (fDist <= 1.f)
            {
                float fStrength = min((1.f - fDist) * m_fFalloffScale, 1.f);

                if (m_bRadial && fDist > 0.f)
                {
                    // Normalize vOffset for force scaling.
                    fStrength *= isqrt_fast_tpl(vPosRel.GetLengthSquared());
                    if (m_nFlags & ENV_GRAVITY)
                    {
                        forces.vAccel = vPosRel * (m_Forces.vAccel.z * fStrength);
                    }
                    if (m_nFlags & ENV_WIND)
                    {
                        forces.vWind += vPosRel * (m_Forces.vWind.z * fStrength);
                    }
                }
                else
                {
                    if (m_nFlags & ENV_GRAVITY)
                    {
                        forces.vAccel = m_Forces.vAccel * fStrength;
                    }
                    if (m_nFlags & ENV_WIND)
                    {
                        forces.vWind += m_Forces.vWind * fStrength;
                    }
                }
            }

#ifdef DEBUG_FORCES
            assert(forcesPhys.vAccel.IsEquivalent(forces.vAccel));
            assert(forcesPhys.vWind.IsEquivalent(forces.vWind));
#endif
        }
    }

    if (nFlags & ENV_WATER)
    {
        GetWaterPlane(forces.plWater, vPos, forces.plWater.DistFromPlane(vPos));
    }
}

float SPhysEnviron::SArea::GetWaterPlane(Plane& plane, Vec3 const& vPos, float fMaxDist) const
{
#if PARTICLES_USE_CRY_PHYSICS
    float fDist = m_Forces.plWater.DistFromPlane(vPos);
    if (fDist < fMaxDist)
    {
        pe_status_contains_point st;
        st.pt = vPos - m_Forces.plWater.n * fDist;
        if (m_pArea->GetStatus(&st))
        {
            plane = m_Forces.plWater;
            return fDist;
        }
    }
#else // AZPhysics
    // Retrieve distance and water plane
    CRY_PHYSICS_REPLACEMENT_ASSERT();
#endif // PARTICLES_USE_CRY_PHYSICS
    return fMaxDist;
}

float SPhysEnviron::GetNonUniformWaterPlane(Plane& plWater, Vec3 const& vPos, float fMaxDist) const
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    for_array (i, m_NonUniformAreas)
    {
        const SArea& area = m_NonUniformAreas[i];
        if (area.m_nFlags & ENV_WATER)
        {
            fMaxDist = area.GetWaterPlane(plWater, vPos, fMaxDist);
        }
    }

    return fMaxDist;
}

#if !PARTICLES_USE_CRY_PHYSICS // AzPhysics

Physics::World* SPhysEnviron::GetAZPhysicsWorld()
{
    AZStd::shared_ptr<Physics::World> worldsharedPtr;
    Physics::DefaultWorldBus::BroadcastResult(worldsharedPtr, &Physics::DefaultWorldRequests::GetDefaultWorld);

#ifndef _RELEASE
    if (gEnv->IsEditing() && !gEnv->IsEditorSimulationMode())
    {
        Physics::EditorWorldBus::BroadcastResult(worldsharedPtr, &Physics::EditorWorldRequests::GetEditorWorld);
    }
#endif
    return worldsharedPtr.get();
}

CryParticleHitEntity* SPhysEnviron::GetPhysicalEntityFromEntityId(AZ::EntityId entityId)
{
    CryParticleHitEntity* ret = nullptr;
    Physics::WorldBodyRequestBus::EventResult(ret, entityId, &Physics::WorldBodyRequests::GetWorldBody);
    return ret;
}

#endif // !PARTICLES_USE_CRY_PHYSICS // AZPhysics

/*
    Emitters render either entirely inside or entirely outside VisAreas (due to rendering architecture).
        Emitter origin in vis area:
            Params.VisibleInside == false? Don't render
            else entire emitter in vis area: we're good
            else (intersecting): Test each particle for dist, shrink / fade-out near boundary
        Emitter origin outside vis areas:
            Params.VisibleInside == true ? Don't render
            else entirely outside vis areas: we're good
            else (intersecting): Test each particle for dist, shrink / fade-out near boundary
*/

void SVisEnviron::Update(Vec3 const& vOrigin, AABB const& bb)
{
    if (!m_bValid)
    {
        // Determine emitter's vis area, by origin.
        FUNCTION_PROFILER_SYS(PARTICLE);

        m_pVisArea = Get3DEngine()->GetVisAreaFromPos(vOrigin);
        m_pVisNodeCache = 0;

        // See whether it crosses boundaries.
        if (m_pVisArea)
        {
            m_bCrossesVisArea = m_pVisArea->GetAABBox()->IsIntersectBox(bb);
        }
        else
        {
            m_bCrossesVisArea = Get3DEngine()->IntersectsVisAreas(bb, &m_pVisNodeCache);
        }

        m_bValid = true;
    }
}

IVisArea* SVisEnviron::GetClipVisArea(IVisArea* pVisAreaCam, AABB const& bb) const
{
    if (m_bCrossesVisArea)
    {
        // Clip only against cam area.
        if (pVisAreaCam != m_pVisArea)
        {
            if (pVisAreaCam)
            {
                if (pVisAreaCam->GetAABBox()->IsIntersectBox(bb))
                {
                    if (!m_pVisArea || !pVisAreaCam->FindVisArea(m_pVisArea, 3, true))
                    {
                        return pVisAreaCam;
                    }
                }
            }
            else if (m_pVisArea)
            {
                return m_pVisArea;
            }
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// SEmitGeom functions

#include "ICryAnimation.h"
#include "GeomQuery.h"

SEmitGeom::SEmitGeom(const GeomRef& geom, EGeomType eAttachType)
{
    // Remove references to unneeded objects.
    if (eAttachType == GeomType_Physics)
    {
        m_pMeshObj = 0;
        Set(geom.m_pPhysEnt);
#if PARTICLES_USE_CRY_PHYSICS
        if (geom.m_pChar)
        {
            // Substitute articulated physics if found.
            if (ISkeletonPose* pPose = geom.m_pChar->GetISkeletonPose())
            {
                IPhysicalEntity* pPhys = pPose->GetCharacterPhysics();
                if (!pPhys)
                {
                    pPhys = pPose->GetCharacterPhysics(0);
                }
                if (pPhys)
                {
                    Set(pPhys);
                }
            }
        }
#endif // PARTICLES_USE_CRY_PHYSICS
    }
    else
    {
        m_pPhysEnt = 0;
        Set(geom.m_pStatObj, geom.m_pChar);
    }
}

void SEmitGeom::AddRef() const
{
    if (ICharacterInstance* pChar = GetCharacter())
    {
        pChar->AddRef();
    }
    else if (IStatObj* pStatObj = GetStatObj())
    {
        pStatObj->AddRef();
    }
#if PARTICLES_USE_CRY_PHYSICS
    if (m_pPhysEnt)
    {
        m_pPhysEnt->AddRef();
    }
#endif
}

void SEmitGeom::Release() const
{
    if (ICharacterInstance* pChar = GetCharacter())
    {
        pChar->Release();
    }
    else if (IStatObj* pStatObj = GetStatObj())
    {
        pStatObj->Release();
    }
#if PARTICLES_USE_CRY_PHYSICS
    if (m_pPhysEnt)
    {
        m_pPhysEnt->Release();
    }
#endif
}

void SEmitGeom::GetAABB(AABB& bb, QuatTS const& tLoc) const
{
    bb.Reset();
    if (m_pMeshObj)
    {
        if (ICharacterInstance* pChar = GetCharacter())
        {
            bb = pChar->GetAABB();
        }
        else if (IStatObj* pStatObj = GetStatObj())
        {
            bb = pStatObj->GetAABB();
        }
        if (IsCentered())
        {
            bb.Move(-bb.GetCenter());
        }
        bb.SetTransformedAABB(Matrix34(tLoc), bb);
    }
    else if (m_pPhysEnt)
    {
#if PARTICLES_USE_CRY_PHYSICS
        pe_status_pos pos;
        if (m_pPhysEnt->GetStatus(&pos))
        {
            // Box is already axis-aligned, but not offset.
            bb = AABB(pos.pos + pos.BBox[0], pos.pos + pos.BBox[1]);
        }
#else // AZPhysics
        bb = AZAabbToLyAABB(m_pPhysEnt->GetAabb());
#endif // PARTICLES_USE_CRY_PHYSICS
    }
}

float SEmitGeom::GetExtent(EGeomForm eForm) const
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    if (ICharacterInstance* pChar = GetCharacter())
    {
        return pChar->GetExtent(eForm);
    }
    else if (IStatObj* pStatObj = GetStatObj())
    {
        return pStatObj->GetExtent(eForm);
    }
    else if (m_pPhysEnt)
    {
#if PARTICLES_USE_CRY_PHYSICS
        // status_extent query caches extent in geo.
        pe_status_extent se;
        se.eForm = eForm;
        m_pPhysEnt->GetStatus(&se);
        return se.extent;
#else // AZPhysics
        return m_pPhysEnt->GetAabb().GetExtents().GetMaxElement();
#endif // PARTICLES_USE_CRY_PHYSICS
    }
    return 0.f;
}

void SEmitGeom::GetRandomPos(PosNorm& ran, EGeomForm eForm, QuatTS const& tWorld) const
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    if (ICharacterInstance* pChar = GetCharacter())
    {
        pChar->GetRandomPos(ran, eForm);
        if (IsCentered())
        {
            ran.vPos -= pChar->GetAABB().GetCenter();
        }
        ran <<= tWorld;
    }
    else if (IStatObj* pStatObj = GetStatObj())
    {
        pStatObj->GetRandomPos(ran, eForm);
        if (IsCentered())
        {
            ran.vPos -= pStatObj->GetAABB().GetCenter();
        }
        ran <<= tWorld;
    }
    else if (m_pPhysEnt)
    {
#if PARTICLES_USE_CRY_PHYSICS
        pe_status_random sr;
        sr.eForm = eForm;
        m_pPhysEnt->GetStatus(&sr);
        ran = sr.ran;
#else // AZPhysics
        auto getRandomPosLocalAZPhysics = [this](PosNorm& ran, EGeomForm eForm)
        {
            // Retrieve a random shape and calculate a random point inside it.
            // Note: shape is assumed to be convex
            if (size_t shapeCount = m_pPhysEnt->GetShapeCount())
            {
                size_t randShapeIndex = cry_random(size_t(0), shapeCount - 1);
                AZStd::shared_ptr<Physics::Shape> shape = m_pPhysEnt->GetShape(randShapeIndex);
                AZStd::vector<AZ::Vector3> vertices;
                AZStd::vector<AZ::u32> indices;
                shape->GetGeometry(vertices, indices);
                if (vertices.size() > 0 && indices.size() > 0)
                {
                    // Random vertex
                    if (eForm == GeomForm_Vertices)
                    {
                        size_t randVertexIdx = cry_random(size_t(0), vertices.size() - 1);
                        ran.vPos = AZVec3ToLYVec3(vertices[randVertexIdx]);
                        //ran.vNorm = ran.vPos.GetNormalized();
                        return;
                    }

                    // Random triangle start (0, 3, 6, 9,...)
                    size_t randTriangleStartIdx = cry_random(size_t(0), (indices.size() / 3) - 1) * 3;

                    auto calcRandPointBetweenPoints = [](const AZ::Vector3& a, const AZ::Vector3& b)
                    {
                        float t = cry_random(0.f, 1.f);
                        AZ::Vector3 dir = b - a;
                        return a + (dir * t);
                    };

                    const AZ::Vector3 randTriangle[] =
                    {
                        vertices[randTriangleStartIdx + 0],
                        vertices[randTriangleStartIdx + 1],
                        vertices[randTriangleStartIdx + 2]
                    };

                    // Random point inside a triangle edge
                    if (eForm == GeomForm_Edges)
                    {
                        const int edges[3][2] = { {0, 1}, {0, 2}, {1, 2} };
                        const int (&edge)[2] = edges[cry_random(0, 2)];
                        AZ::Vector3 randInsideEdge = calcRandPointBetweenPoints(randTriangle[edge[0]], randTriangle[edge[1]]);
                        ran.vPos = AZVec3ToLYVec3(randInsideEdge);
                        //ran.vNorm = ran.vPos.GetNormalized();
                        return;
                    }

                    // For calculating a point inside triangle, we pick two random points on two edges
                    // and then a random point between them.
                    AZ::Vector3 randPointInTriAB = calcRandPointBetweenPoints(randTriangle[0], randTriangle[1]);
                    AZ::Vector3 randPointInTriAC = calcRandPointBetweenPoints(randTriangle[0], randTriangle[2]);

                    AZ::Vector3 randPointInTriangle = calcRandPointBetweenPoints(randPointInTriAB, randPointInTriAC);

                    // Random point on triangle surface
                    if (eForm == GeomForm_Surface)
                    {
                        ran.vPos = AZVec3ToLYVec3(randPointInTriangle);
                        return;
                    }

                    // Calculate a random point on a line from the random triangle point towards the shape center
                    AZ::Vector3 randPointInShape = calcRandPointBetweenPoints(randPointInTriangle, shape->GetAabbLocal().GetCenter());

                    // Random point inside volume
                    if (eForm == GeomForm_Volume)
                    {
                        ran.vPos = AZVec3ToLYVec3(randPointInShape);
                        return;
                    }
                }
            }
        };

        getRandomPosLocalAZPhysics(ran, eForm);
        ran <<= tWorld;
#endif // PARTICLES_USE_CRY_PHYSICS
    }

#ifdef _DEBUG
    AABB bb;
    GetAABB(bb, tWorld);
    assert(bb.IsOverlapSphereBounds(ran.vPos, bb.GetRadius() * 0.010f));
#endif
}
