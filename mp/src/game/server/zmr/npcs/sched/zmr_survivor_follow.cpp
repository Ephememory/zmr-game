#include "cbase.h"

#include "zmr_survivor_follow.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CSurvivorFollowSchedule::CSurvivorFollowSchedule()
{
}

CSurvivorFollowSchedule::~CSurvivorFollowSchedule()
{
}

void CSurvivorFollowSchedule::OnStart()
{
    m_NextFollowTarget.Start( 2.0f );
}

void CSurvivorFollowSchedule::OnContinue()
{
    m_Path.Invalidate();
}

void CSurvivorFollowSchedule::OnUpdate()
{
    CZMPlayerBot* pOuter = GetOuter();

    if ( !pOuter->IsHuman() )
    {
        return;
    }


    auto* pFollow = m_hFollowTarget.Get();


    if ( (pFollow && !IsValidFollowTarget( pFollow )) || m_NextFollowTarget.IsElapsed() )
    {
        NextFollow();

        pFollow = m_hFollowTarget.Get();
    }


    bool bBusy = pOuter->IsBusy() == NPCR::RES_YES;

    if ( m_Path.IsValid() && pFollow && !bBusy && ShouldMoveCloser( pFollow ) )
    {
        m_Path.Update( pOuter, pFollow, m_PathCost );
    }
}

void CSurvivorFollowSchedule::OnSpawn()
{
    m_Path.Invalidate();

    m_NextFollowTarget.Start( 0.5f );
}

void CSurvivorFollowSchedule::OnHeardSound( CSound* pSound )
{
}

//NPCR::QueryResult_t CSurvivorFollowSchedule::IsBusy() const
//{
//    return m_Path.IsValid() ? NPCR::RES_YES : NPCR::RES_NONE;
//}

NPCR::QueryResult_t CSurvivorFollowSchedule::ShouldChase( CBaseEntity* pEnemy ) const
{
    auto* pFollow = m_hFollowTarget.Get();
    return pFollow && m_Path.IsValid() && ShouldMoveCloser( pFollow ) ? NPCR::RES_NO : NPCR::RES_NONE;
}

//void CSurvivorFollowSchedule::OnMoveSuccess( NPCR::CBaseNavPath* pPath )
//{
//        
//}

bool CSurvivorFollowSchedule::IsValidFollowTarget( CBasePlayer* pPlayer, bool bCheckLoop ) const
{
    if ( pPlayer->GetTeamNumber() != ZMTEAM_HUMAN || !pPlayer->IsAlive() )
    {
        return false;
    }

    auto* pOuter = GetOuter();

    // Make sure our following chain isn't circular.

    auto* pLoop = pPlayer;
    do
    {
        if ( pLoop->IsBot() )
        {
            auto* pBot = static_cast<CZMPlayerBot*>( pLoop );
            auto* pTheirTarget = pBot->GetFollowTarget();
            if ( pTheirTarget == pOuter ) // Don't follow a bot that is following us, lul.
            {
                return false;
            }

            if ( bCheckLoop )
                pLoop = pTheirTarget;
            else
                break;
        }
        else
        {
            break;
        }
    }
    while ( pLoop != nullptr );

    return true;
}

void CSurvivorFollowSchedule::NextFollow()
{
    //auto* pOuter = GetOuter();

    float flNextCheck = 1.0f;


    auto* pLastFollow = m_hFollowTarget.Get();
    bool bWasValid = pLastFollow ? IsValidFollowTarget( pLastFollow, true ) : false;

    auto* pFollow = FindSurvivorToFollow();

    if ( (pLastFollow != pFollow || !m_Path.IsValid()) && pFollow )
    {
        StartFollow( pFollow );

        //float dist = pOuter->GetAbsOrigin().DistTo( pFollow->GetAbsOrigin() );
            
        flNextCheck = 15.0f;
    }
    else if ( !bWasValid )
    {
        m_hFollowTarget.Set( nullptr );
    }

    m_NextFollowTarget.Start( flNextCheck );
}

void CSurvivorFollowSchedule::StartFollow( CBasePlayer* pFollow )
{
    auto* pOuter = GetOuter();


    m_hFollowTarget.Set( pFollow );
    pOuter->SetFollowTarget( pFollow );

    m_Path.SetGoalTolerance( 0.0f );
    m_Path.Compute( pOuter, pFollow, m_PathCost );
}

bool CSurvivorFollowSchedule::ShouldMoveCloser( CBasePlayer* pFollow ) const
{
    auto* pOuter = GetOuter();

    float flDistSqr = pOuter->GetAbsOrigin().DistToSqr( pFollow->GetAbsOrigin() );
        
    //if 
    {
        return ( flDistSqr > (128.0f * 128.0f));
    }
}

CBasePlayer* CSurvivorFollowSchedule::FindSurvivorToFollow( CBasePlayer* pIgnore, bool bAllowBot ) const
{
    auto* pOuter = GetOuter();

    Vector mypos = pOuter->GetAbsOrigin();

    CBasePlayer* pClosest = nullptr;
    float flClosestDist = FLT_MAX;

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        auto* pPlayer = static_cast<CBasePlayer*>( UTIL_EntityByIndex( i ) );

        if ( !pPlayer ) continue;

        if ( pPlayer == pOuter ) continue;

        if ( !bAllowBot && pPlayer->IsBot() ) continue;

        if ( !IsValidFollowTarget( pPlayer, true ) ) continue;


        float dist = pPlayer->GetAbsOrigin().DistToSqr( mypos );
        if ( dist < flClosestDist )
        {
            flClosestDist = dist;
            pClosest = pPlayer;
        }
    }

    if ( !bAllowBot )
    {
        // Try bot one.
        return FindSurvivorToFollow( pIgnore, true );
    }

    return pClosest;
}
