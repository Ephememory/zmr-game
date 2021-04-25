#include "cbase.h"
#include "npcevent.h"
#include "activitylist.h"
#include "eventlist.h"

#include "zmr_gamerules.h"
#include "npcs/zmr_zombieanimstate.h"
#include "npcs/zmr_zombiebase.h"
#include "zmr_banshee_path.h"
#include "zmr_banshee.h"
#include "npcs/sched/zmr_zombie_banshee_ceil_ambush.h"
#include "npcs/sched/zmr_zombie_banshee_leap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar zm_sv_debug_bansheerangeattack( "zm_sv_debug_bansheerangeattack", "0" );


#define ZOMBIE_MODEL        "models/zombie/zm_fast.mdl"


ConVar zm_sv_banshee_leapdist_min( "zm_sv_banshee_leapdist_min", "200", FCVAR_NOTIFY, "The minimum distance Banshees will perform a leap." );
ConVar zm_sv_banshee_leapdist_max( "zm_sv_banshee_leapdist_max", "300", FCVAR_NOTIFY, "The maximum distance Banshees will perform a leap." );

ConVar zm_sv_banshee_ceilambush_detectrange( "zm_sv_banshee_ceilambush_detectrange", "256", FCVAR_NOTIFY, "How far from the ambush point do the Banshees check for victims?", true, 0.0f, false, 0.0f );
ConVar zm_sv_banshee_ceilambush_maxheight( "zm_sv_banshee_ceilambush_maxheight", "375", FCVAR_NOTIFY, "The maximum height for a ceiling ambush", true, 0.0f, false, 0.0f );

extern ConVar zm_sk_banshee_melee_continuous_limit;


LINK_ENTITY_TO_CLASS( npc_fastzombie, CZMBanshee );
PRECACHE_REGISTER( npc_fastzombie );




extern ConVar zm_sk_banshee_dmg_claw;
extern ConVar zm_sk_banshee_health;



// Banshee motor
CZMBansheeMotor::CZMBansheeMotor( CZMBaseZombie* pOuter ) : CZMBaseZombieMotor( pOuter )
{
    m_bIsInNavJump = false;
}

float CZMBansheeMotor::GetHullHeight() const
{
    return CZMBanshee::GetBansheeHullHeight();
}

void CZMBansheeMotor::OnLandedGround( CBaseEntity* pGround )
{
    m_bIsInNavJump = false;

    BaseClass::OnLandedGround( pGround );
}

void CZMBansheeMotor::NavJump( const Vector& vecGoal, float flOverrideHeight )
{
    m_bIsInNavJump = true;
    BaseClass::NavJump( vecGoal, flOverrideHeight );
}

bool CZMBansheeMotor::ShouldAdjustVelocity() const
{
    // We don't want to adjust velocity while nav jumping.
    // This allows banshees to hit a wall while in air and still reach their goal.
    return !m_bIsInNavJump && BaseClass::ShouldAdjustVelocity();
}




IMPLEMENT_SERVERCLASS_ST( CZMBanshee, DT_ZM_Banshee )
END_SEND_TABLE()

CZMBanshee::CZMBanshee()
{
    SetZombieClass( ZMCLASS_BANSHEE );
    CZMRules::IncPopCount( GetZombieClass() );


    m_flNextLeapAttack = 0.0f;
    m_nMeleeAttacks = 0;


    m_pLeapSched = new CBansheeLeapSchedule;
    m_pCeilAmbushSched = new CBansheeCeilAmbushSchedule;
}

CZMBanshee::~CZMBanshee()
{
    delete m_pLeapSched;
    delete m_pCeilAmbushSched;
}

void CZMBanshee::Precache()
{
    if ( !IsPrecacheAllowed() )
        return;


    PrecacheScriptSound( "NPC_FastZombie.LeapAttack" );
    PrecacheScriptSound( "NPC_FastZombie.FootstepRight" );
    PrecacheScriptSound( "NPC_FastZombie.FootstepLeft" );
    PrecacheScriptSound( "NPC_FastZombie.AttackHit" );
    PrecacheScriptSound( "NPC_FastZombie.AttackMiss" );
    PrecacheScriptSound( "NPC_FastZombie.LeapAttack" );
    PrecacheScriptSound( "NPC_FastZombie.Attack" );
    PrecacheScriptSound( "NPC_FastZombie.Idle" );
    PrecacheScriptSound( "NPC_FastZombie.AlertFar" );
    PrecacheScriptSound( "NPC_FastZombie.AlertNear" );
    PrecacheScriptSound( "NPC_FastZombie.GallopLeft" );
    PrecacheScriptSound( "NPC_FastZombie.GallopRight" );
    PrecacheScriptSound( "NPC_FastZombie.Scream" );
    PrecacheScriptSound( "NPC_FastZombie.RangeAttack" );
    PrecacheScriptSound( "NPC_FastZombie.Frenzy" );
    PrecacheScriptSound( "NPC_FastZombie.NoSound" );
    PrecacheScriptSound( "NPC_FastZombie.Die" );

    PrecacheScriptSound( "NPC_FastZombie.Gurgle" );

    PrecacheScriptSound( "NPC_FastZombie.Moan1" );


    BaseClass::Precache();
}

void CZMBanshee::Spawn()
{
    SetMaxHealth( zm_sk_banshee_health.GetInt() );


    BaseClass::Spawn();
}

CZMBansheeMotor* CZMBanshee::GetBansheeMotor() const
{
    return static_cast<CZMBansheeMotor*>( GetMotor() );
}

NPCR::CPathCostGroundOnly* CZMBanshee::GetPathCost() const
{
    static NPCR::CPathCostGroundOnly* cost = nullptr;
    if ( !cost )
    {
        //const float MAX_JUMP_RISE       = 512.0f; // 220.0f
        //const float MAX_JUMP_DISTANCE   = 512.0f;
        //const float MAX_JUMP_DROP       = 1024.0f; // 384.0f

        cost = new NPCR::CPathCostGroundOnly;
        cost->SetMaxHeightChange( GetMaxNavJumpHeight() );
        cost->SetAbsMaxGap( GetMaxNavJumpDist() );
        //cost->SetMaxWalkGap( 32.0f ); // Our hull diameter.
    }

    return cost;
}

NPCR::CFollowNavPath* CZMBanshee::GetFollowPath() const
{
    return new CZMBansheeFollowPath;
}

void CZMBanshee::OnNavJump()
{
    //SetActivity( ACT_JUMP );

    BaseClass::OnNavJump();
}

NPCR::QueryResult_t CZMBanshee::ShouldChase( CBaseEntity* pEnemy ) const
{
    if ( GetBansheeMotor()->IsInNavJump() )
        return NPCR::RES_NO;

    return BaseClass::ShouldChase( pEnemy );
}

bool CZMBanshee::IsAttacking() const
{
    Activity act = GetActivity();

    if ( act == ACT_FASTZOMBIE_BIG_SLASH )
        return true;
    if ( act == ACT_FASTZOMBIE_FRENZY )
        return true;

    return BaseClass::IsAttacking();
}

void CZMBanshee::StartCeilingAmbush( CZMPlayer* pCommander )
{
    GetCommandQueue()->QueueCommand( new CZMCommandCeilingAmbush( pCommander ) );
    OnQueuedCommand( pCommander, COMMAND_CEILINGAMBUSH );
}

bool CZMBanshee::LeapAttack( const QAngle& angPunch, const Vector& vecPunchVel, float flDamage )
{
    Vector mins, maxs;
    GetAttackHull( mins, maxs );

    Vector dir = GetVel().Normalized();

    CUtlVector<CBaseEntity*> vHitEnts;
    bool bHit = MeleeAttackTrace( mins, maxs, GetClawAttackRange(), flDamage, DMG_SLASH, &vHitEnts, &dir );

    FOR_EACH_VEC( vHitEnts, i )
    {
        CBasePlayer* pPlayer = ToBasePlayer( vHitEnts[i] );
        if ( pPlayer )
        {
            pPlayer->ViewPunch( angPunch );
            pPlayer->VelocityPunch( vecPunchVel );
        }
    }


    ClawImpactSound( bHit );

    return bHit;
}

bool CZMBanshee::HasConditionsForRangeAttack( CBaseEntity* pEnemy ) const
{
    if ( gpGlobals->curtime < GetNextLeapAttack() )
        return false;

    if ( GetNextMove() > gpGlobals->curtime )
        return false;

    if ( !GetMotor()->IsOnGround() )
        return false;

    const Vector vecStart = WorldSpaceCenter();
    const Vector vecEnd = pEnemy->WorldSpaceCenter();

    // Close enough?
    float minleapsqr = zm_sv_banshee_leapdist_min.GetFloat();
    minleapsqr *= minleapsqr;
    float maxleapsqr = zm_sv_banshee_leapdist_max.GetFloat();
    maxleapsqr *= maxleapsqr;
    float distEnemy = vecStart.DistToSqr( vecEnd );
    if ( distEnemy < minleapsqr || distEnemy > maxleapsqr )
    {
        return false;
    }


    bool bDebugging = zm_sv_debug_bansheerangeattack.GetBool();

    // We need to be able to see em.
    trace_t tr;
    UTIL_TraceLine( vecStart, vecEnd, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

    bool bCanSee = tr.fraction == 1.0f || (tr.m_pEnt && tr.m_pEnt->IsPlayer());

    if ( bDebugging )
    {
        NDebugOverlay::Line( vecStart, vecEnd, (!bCanSee) ? 255 : 0, bCanSee ? 255 : 0, 0, true, 0.1f );
    }

    if ( bCanSee )
    {
        // Do a hull trace so it makes sense for us to leap towards the enemy.
        Vector mins, maxs;
        maxs.x = maxs.y = GetMotor()->GetHullWidth() / 2.0f;
        mins.x = mins.y = -maxs.x;
        mins.z = 0.0f;
        maxs.z = GetMotor()->GetHullHeight() - 4.0f;

        Vector vecBoxStart = GetAbsOrigin();
        vecBoxStart.z += 4.0f;

        Vector dir = vecEnd - vecStart;
        float dist = dir.NormalizeInPlace() * 0.5f;
        dist = MAX( 16.0f, dist );

        Vector vecBoxEnd = vecBoxStart + dir * dist;

        UTIL_TraceHull( vecBoxStart, vecBoxEnd, mins, maxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

        bool ret = tr.fraction == 1.0f;

        if ( bDebugging )
        {
            NDebugOverlay::SweptBox( vecBoxStart, vecBoxEnd, mins, maxs, vec3_angle, (!ret) ? 255 : 0, ret ? 255 : 0, 0, 0, 0.1f );
        }

        return ret;
    }

    return false;
}

NPCR::CSchedule<CZMBaseZombie>* CZMBanshee::GetRangeAttackSchedule() const
{
    return m_pLeapSched;
}

void CZMBanshee::HandleAnimEvent( animevent_t* pEvent )
{
    //extern int AE_FASTZOMBIE_CLIMB_LEFT;
    //extern int AE_FASTZOMBIE_CLIMB_RIGHT;

    //if ( pEvent->event == AE_FASTZOMBIE_CLIMB_LEFT || pEvent->event == AE_FASTZOMBIE_CLIMB_RIGHT )
    //{
        //if( ++m_iClimbCount % 3 == 0 )
        //{
			//ENVELOPE_CONTROLLER.SoundChangePitch( m_pLayer2, random->RandomFloat( 100, 150 ), 0.0 );
            //ENVELOPE_CONTROLLER.SoundPlayEnvelope( m_pLayer2, SOUNDCTRL_CHANGE_VOLUME, envFastZombieVolumeClimb, ARRAYSIZE(envFastZombieVolumeClimb) );
        //}

    //    return;
    //}

    if ( pEvent->event == AE_FASTZOMBIE_GALLOP_LEFT )
    {
        return;
    }

    if ( pEvent->event == AE_FASTZOMBIE_GALLOP_RIGHT )
    {
        return;
    }
    
    if (pEvent->event == AE_ZOMBIE_ATTACK_RIGHT
    ||  pEvent->event == AE_ZOMBIE_ATTACK_LEFT)
    {
        Vector right;
        AngleVectors( GetLocalAngles(), NULL, &right, NULL );
        right = right * -50;
        QAngle viewpunch( -3, -5, -3 );

        ClawAttack( GetClawAttackRange(), zm_sk_banshee_dmg_claw.GetInt(), viewpunch, right );
        return;
    }

    BaseClass::HandleAnimEvent( pEvent );
}

NPCR::CSchedule<CZMBaseZombie>* CZMBanshee::OverrideCombatSchedule() const
{
    CZMCommandBase* pCommand = GetCommandQueue()->NextCommand();
    if ( pCommand && pCommand->GetCommandType() == COMMAND_CEILINGAMBUSH )
    {
        GetCommandQueue()->RemoveCommand( pCommand );
        return m_pCeilAmbushSched;
    }

    return BaseClass::OverrideCombatSchedule();
}

void CZMBanshee::OnAnimActivityFinished( Activity completedActivity )
{
    if ( completedActivity == ACT_MELEE_ATTACK1 )
    {
        bool bCanAttack = GetEnemy() && HasConditionsForClawAttack( GetEnemy() );
        
        int attacklimit = zm_sk_banshee_melee_continuous_limit.GetInt();
        //
        // If we loop, we're the "fast" attack
        // for this attack, limit how many times we can attack.
        //
        // Otherwise, just let us through.
        //
        if ( !SequenceLoops() || !bCanAttack || ++m_nMeleeAttacks >= attacklimit )
        {
            DoAnimationEvent( ZOMBIEANIMEVENT_BANSHEEANIM, ACT_FASTZOMBIE_FRENZY );

            float delay = SequenceDuration();
            SetNextAttack( gpGlobals->curtime + delay );
            SetNextMove( gpGlobals->curtime + delay );


            EmitSound( "NPC_FastZombie.Frenzy" );

            m_nMeleeAttacks = 0;
        }
    }
    else if ( completedActivity == ACT_FASTZOMBIE_FRENZY )
    {
        bool bCanAttack = GetEnemy() && HasConditionsForClawAttack( GetEnemy() );
        if ( bCanAttack )
        {
            DoAnimationEvent( ZOMBIEANIMEVENT_BANSHEEANIM, ACT_FASTZOMBIE_BIG_SLASH );

            float delay = SequenceDuration();
            SetNextAttack( gpGlobals->curtime + delay );
            SetNextMove( gpGlobals->curtime + delay );
        }
    }

    BaseClass::OnAnimActivityFinished( completedActivity );
}

bool CZMBanshee::ShouldPlayIdleSound() const
{
    //return BaseClass::ShouldPlayIdleSound() && random->RandomInt( 0, 99 ) == 0;
    return false;
}

float CZMBanshee::IdleSound()
{
    // Banshee idle sound doesn't seem to work.
    //EmitSound( "NPC_FastZombie.Idle" );
    return 1.0f;
}

#define ALERT_SOUND_NEAR_DIST       512.0f

void CZMBanshee::AlertSound()
{
    // Apparently banshee doesn't use these at all.
    // And these sounds are weird
    /*
    if ( GetEnemy() )
    {
        EmitSound( GetEnemy()->GetAbsOrigin().DistToSqr( GetAbsOrigin() ) < (ALERT_SOUND_NEAR_DIST*ALERT_SOUND_NEAR_DIST) ?
            "NPC_FastZombie.AlertNear" :
            "NPC_FastZombie.AlertFar" );
    }
    */
}

void CZMBanshee::DeathSound()
{
    EmitSound( "NPC_FastZombie.Die" );
    g_flLastZombieSound = gpGlobals->curtime;
}

void CZMBanshee::ClawImpactSound( bool bHit )
{
    EmitSound( bHit ? "NPC_FastZombie.AttackHit" : "NPC_FastZombie.AttackMiss" );
    g_flLastZombieSound = gpGlobals->curtime;
}

void CZMBanshee::LeapAttackSound()
{
    EmitSound( "NPC_FastZombie.LeapAttack" );
    g_flLastZombieSound = gpGlobals->curtime;
}
