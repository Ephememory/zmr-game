#include "cbase.h"
#include "in_buttons.h"
#include "nav_mesh.h"

#include "npcr_motor_player.h"
#include "npcr_manager.h"
#include "npcr_player.h"


#ifdef NPCR_BOT_CMDS
template<typename ForEachFunc>
void ForTargetedBots( const char* comp, ForEachFunc f )
{
    NPCR::g_NPCManager.ForEachNPC( [ comp, f ]( NPCR::CBaseNPC* pNPC )
    {
        bool passes = false;

        auto* pChar = pNPC->GetCharacter();

        if ( comp && *comp )
        {
            auto* pPlayerBot = ToBasePlayer( pChar );
            if ( pPlayerBot )
            {
			    passes = Q_stricmp( pPlayerBot->GetPlayerName(), comp ) != -1;
            }
            else
            {
                passes = Q_stricmp( STRING( pChar->GetEntityName() ), comp ) != -1;
            }
        }
        else
        {
            passes = true;
        }

        if ( passes )
        {
            f( pNPC );
        }

        return false;
    } );
}



ConVar bot_attack( "bot_attack", "0" );
ConVar bot_mimic( "bot_mimic", "0" );
ConVar bot_mimic_yaw_offset( "bot_mimic_yaw_offset", "0" );

CON_COMMAND( bot_sendcmd, "Sends command to all bot players" )
{
    if ( !UTIL_IsCommandIssuedByServerAdmin() )
        return;


    NPCR::g_NPCManager.ForEachNPC( [ &args ]( NPCR::CBaseNPC* pNPC )
    {
        CBasePlayer* pPlayer = ToBasePlayer( pNPC->GetCharacter() );
        if ( pPlayer )
        {
            if ( !pPlayer->ClientCommand( args ) )
            {
                engine->ClientCommand( pPlayer->edict(), "%s", args.ArgS() );
            }
        }

        return false;
    } );
}
#endif

CON_COMMAND( bot_teleport, "Teleport given bots to your crosshair." )
{
    if ( !UTIL_IsCommandIssuedByServerAdmin() )
        return;

    auto* pPlayer = UTIL_GetCommandClient();
    if( !pPlayer )
        return;


    CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );
    Vector fwd;
    trace_t tr;

    pPlayer->EyeVectors( &fwd );

    UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + fwd * MAX_TRACE_LENGTH, MASK_SOLID, &filter, &tr );
    


    const char* comp = args.Arg( 2 );

    ForTargetedBots( comp, [ &tr ]( NPCR::CBaseNPC* pNPC )
    {
        pNPC->GetCharacter()->Teleport( &tr.endpos, nullptr, nullptr );
    } );
}

CON_COMMAND( bot_goto_selected, "Tells given bots to move to specific nav area." )
{
    if ( !UTIL_IsCommandIssuedByServerAdmin() )
        return;

    auto* pPlayer = UTIL_GetCommandClient();
    if( !pPlayer )
        return;


    auto* pArea = TheNavMesh->GetSelectedArea();

    if ( !pArea )
    {
        CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );
        Vector fwd;
        trace_t tr;

        pPlayer->EyeVectors( &fwd );

        UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + fwd * MAX_TRACE_LENGTH, MASK_SOLID, &filter, &tr );
    
        pArea = TheNavMesh->GetNavArea( tr.endpos );
    }


    if ( !pArea )
    {
        Warning( "Couldn't find a nav mesh area!" );
        return;
    }


    const char* comp = args.Arg( 2 );

    ForTargetedBots( comp, [ pArea ]( NPCR::CBaseNPC* pNPC )
    {
        pNPC->OnForcedMove( pArea );
    } );
}

ConVar bot_mimic_pitch_offset( "bot_mimic_pitch_offset", "0" );
// A specific target?
ConVar bot_mimic_target( "bot_mimic_target", "0", 0, "A specific bot that will mimic the actions" );


NPCR::CPlayerCmdHandler::CPlayerCmdHandler( CBaseCombatCharacter* pNPC ) : NPCR::CBaseNPC( pNPC )
{
}

NPCR::CPlayerCmdHandler::~CPlayerCmdHandler()
{
}

NPCR::CPlayerMotor* NPCR::CPlayerCmdHandler::GetPlayerMotor() const
{
    return static_cast<CPlayerMotor*>( CBaseNPC::GetMotor() );
}

NPCR::CBaseMotor* NPCR::CPlayerCmdHandler::CreateMotor()
{
    return new CPlayerMotor( this );
}

DEFINE_BTN( NPCR::CPlayerCmdHandler, PressFire1 )
DEFINE_BTN( NPCR::CPlayerCmdHandler, PressFire2 )
DEFINE_BTN( NPCR::CPlayerCmdHandler, PressDuck )
DEFINE_BTN( NPCR::CPlayerCmdHandler, PressReload )
DEFINE_BTN( NPCR::CPlayerCmdHandler, PressUse )

void NPCR::CPlayerCmdHandler::BuildPlayerCmd( CUserCmd& cmd )
{
    HANDLE_BTN( PressFire1, IN_ATTACK, cmd )
    HANDLE_BTN( PressFire2, IN_ATTACK2, cmd )
    HANDLE_BTN( PressDuck, IN_DUCK, cmd )
    HANDLE_BTN( PressReload, IN_RELOAD, cmd )
    HANDLE_BTN( PressUse, IN_USE, cmd )


    // Movement
    // ZMRTODO: Build the movement direction here instead of in the motor?
    // The bot angles here vs in the motor::Update() may be drastically different.
    CPlayerMotor* pMotor = GetPlayerMotor();

    Vector dir = pMotor->GetMoveDir();
    cmd.forwardmove = dir.x * 400.0f;
    cmd.sidemove = dir.y * 400.0f;
    cmd.upmove = dir.z * 400.0f;

    // Also build the the buttons from move in case some thirdparty plugins used it.
    if ( cmd.forwardmove > 0.0f )
        cmd.buttons |= IN_FORWARD;
    else if ( cmd.forwardmove < 0.0f )
        cmd.buttons |= IN_BACK;

    if ( cmd.sidemove > 0.0f )
        cmd.buttons |= IN_MOVELEFT;
    else if ( cmd.sidemove < 0.0f )
        cmd.buttons |= IN_MOVERIGHT;


    cmd.viewangles = GetEyeAngles();
}
