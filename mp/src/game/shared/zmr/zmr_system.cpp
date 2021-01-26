#include "cbase.h"
#include "GameEventListener.h"
#include <ctime>
#ifdef GAME_DLL
#include "networkstringtable_gamedll.h"
#endif

#ifdef CLIENT_DLL
#include "physpropclientside.h"
#include "c_te_legacytempents.h"
#include "cdll_client_int.h"
#include "c_soundscape.h"

#include "cl_ents/c_cliententitysystem.h"

#include <engine/IEngineSound.h>


#include "c_zmr_zmvision.h"
#include "c_zmr_util.h"


#ifdef _WIN32
#define _WINREG_
#undef ReadConsoleInput
#undef INVALID_HANDLE_VALUE
#undef GetCommandLine
#include <Windows.h>
#endif

#else
#include "zmr_rejoindata.h"
#endif

#include "zmr_shareddefs.h"
#include "zmr_web.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef CLIENT_DLL
ConVar zm_cl_roundrestart_flashtaskbar( "zm_cl_roundrestart_flashtaskbar", "1", 0, "Flash the taskbar icon (Windows) whenever round restarts. 1 = Only when window is not active, 2 = Always" );
ConVar zm_cl_roundrestart_sound( "zm_cl_roundrestart_sound", "1", 0, "Play a sound whenever round restarts. (Windows) 1 = Only when window is not active, 2 = Always" );
#else // CLIENT_DLL

#ifndef ZMR_STEAM
ConVar zm_sv_checkversion( "zm_sv_checkversion", "1", 0, "Is the game version checked every map change?" );
#endif // ZMR_STEAM

#endif // CLIENT_DLL



class CZMSystem : public CAutoGameSystem, public CGameEventListener
{
public:
    CZMSystem() : CAutoGameSystem( "ZMSystem" )
    {
    }

    virtual void PostInit() OVERRIDE;


    virtual void LevelShutdownPreEntity() OVERRIDE;
    virtual void LevelInitPostEntity() OVERRIDE;


#ifndef CLIENT_DLL
    void CheckSpecialDates();
#else
    void PrintRoundEndMessage( ZMRoundEndReason_t reason );

    void RoundRestartEffect();
#endif


    virtual void FireGameEvent( IGameEvent* pEvent ) OVERRIDE;
};

void CZMSystem::PostInit()
{
#ifdef CLIENT_DLL
    //
    // https://developer.valvesoftware.com/wiki/Env_projectedtexture/fixes
    //
    ConVarRef r_flashlightscissor( "r_flashlightscissor" );
    r_flashlightscissor.SetValue( "0" );


    ListenForGameEvent( "round_end_post" );
    ListenForGameEvent( "round_restart_post" );
    ListenForGameEvent( "player_spawn" );
#endif

    // Server Steam API hasn't been initialized yet.
#if defined( CLIENT_DLL ) && !defined( _DEBUG ) && !defined( ZMR_STEAM )
    g_pZMWeb->QueryVersionNumber();
#endif
}

#ifndef CLIENT_DLL
ConVar zm_sv_happyzombies_usedate( "zm_sv_happyzombies_usedate", "1", FCVAR_NOTIFY, "Special dates bring happy zombies :)" );

void CZMSystem::CheckSpecialDates()
{
    if ( !zm_sv_happyzombies_usedate.GetBool() )
        return;


    time_t curtime;
    time( &curtime );
    tm* t = localtime( &curtime );

    HappyZombieEvent_t iEvent = HZEVENT_INVALID;

    // Christmas
    if ( t->tm_mon == 11 )
    {
        iEvent = HZEVENT_CHRISTMAS;
    }
    // Hulkamania
    else if ( t->tm_mon == 5 )
    {
        iEvent = HZEVENT_HULKAMANIA;
    }
    // Halloween
    else if ( t->tm_mon == 10 && t->tm_mday > 23 )
    {
        iEvent = HZEVENT_HALLOWEEN;
    }


    if ( iEvent != HZEVENT_INVALID )
    {
        ConVarRef( "zm_sv_happyzombies" ).SetValue( (int)iEvent );
        DevMsg( "Happy zombies activated by date!\n" );
    }
}
#endif

void CZMSystem::LevelShutdownPreEntity()
{
#ifndef CLIENT_DLL
    GetZMRejoinSystem()->OnLevelShutdown();
#endif
}

void CZMSystem::LevelInitPostEntity()
{
#ifndef CLIENT_DLL
    if ( engine->IsDedicatedServer() )
    {
        // HACK!!! After the first map change, the api hasn't been initialized yet.
        if ( steamgameserverapicontext && !steamgameserverapicontext->SteamHTTP() )
        {
            steamgameserverapicontext->Init();
        }
        
#if !defined( _DEBUG ) && !defined( ZMR_STEAM )
        if ( zm_sv_checkversion.GetBool() )
        {
            g_pZMWeb->QueryVersionNumber();
        }
#endif
    }

    CheckSpecialDates();
#else
    g_ZMVision.TurnOff();
#endif
}


void CZMSystem::FireGameEvent( IGameEvent* pEvent )
{
#ifdef CLIENT_DLL
    if ( Q_strcmp( pEvent->GetName(), "round_end_post" ) == 0 )
    {
        DevMsg( "Client received round end event!\n" );
        

        ZMRoundEndReason_t reason = (ZMRoundEndReason_t)pEvent->GetInt( "reason", ZMROUND_GAMEBEGIN );

        PrintRoundEndMessage( reason );
    }
    else if ( Q_strcmp( pEvent->GetName(), "round_restart_post" ) == 0 )
    {
        DevMsg( "Client received round restart event!\n" );


        // Read client-side entities from map and recreate em.
        C_PhysPropClientside::RecreateAll();
        g_ClientEntitySystem.RecreateAll();


        tempents->Clear();
        
        // Stop sounds.
        enginesound->StopAllSounds( true );
        Soundscape_OnStopAllSounds();


        // Clear decals.
        engine->ClientCmd( "r_cleardecals" );
        

        // Remove client ragdolls since they don't like getting removed.
        C_ClientRagdoll* pRagdoll;
        for ( C_BaseEntity* pEnt = ClientEntityList().FirstBaseEntity(); pEnt; pEnt = ClientEntityList().NextBaseEntity( pEnt ) )
        {
            pRagdoll = dynamic_cast<C_ClientRagdoll*>( pEnt );
            if ( pRagdoll )
            {
                // This will make them fade out.
                pRagdoll->SUB_Remove();
            }
        }

        RoundRestartEffect();
    }
    else if ( Q_strcmp( pEvent->GetName(), "player_spawn" ) == 0 )
    {
        // Tell the player they've spawned.
        C_ZMPlayer* pPlayer = ToZMPlayer( UTIL_PlayerByUserId( pEvent->GetInt( "userid", -1 ) ) );
        if ( pPlayer )
        {
            pPlayer->OnSpawn();
        }
    }
#endif
}

#ifdef CLIENT_DLL
void CZMSystem::PrintRoundEndMessage( ZMRoundEndReason_t reason )
{
    const char* pMsg = nullptr;

    switch ( reason )
    {
    case ZMROUND_HUMANDEAD : pMsg = "#ZMRoundEndHumanDead"; break;
    case ZMROUND_HUMANLOSE : pMsg = "#ZMRoundEndHumanLose"; break;
    case ZMROUND_HUMANWIN : pMsg = "#ZMRoundEndHumanWin"; break;
    case ZMROUND_ZMSUBMIT : pMsg = "#ZMRoundEndSubmit"; break;
    case ZMROUND_GAMEBEGIN : pMsg = "#ZMRoundEndGameBegin"; break;
    case ZMROUND_VOTERESTART : pMsg = "#ZMRoundEndVoteRestart"; break;
    default : break;
    }

    if ( pMsg )
    {
        ZMClientUtil::PrintNotify( pMsg, ZMCHATNOTIFY_NORMAL );
    }
}

void CZMSystem::RoundRestartEffect()
{
    bool bActive = engine->IsActiveApp();



    switch ( zm_cl_roundrestart_sound.GetInt() )
    {
    case 1 :
        if ( bActive )
            break;
    case 2 :
#ifdef _WIN32
        PlaySound( (LPCTSTR)SND_ALIAS_SYSTEMEXCLAMATION, NULL, SND_ALIAS_ID | SND_ASYNC );
#endif
        break;
    default :
        break;
    }

    switch ( zm_cl_roundrestart_flashtaskbar.GetInt() )
    {
    case 1 : 
        if ( bActive )
            break;
    case 2 :
        engine->FlashWindow();
        break;
    default :
        break;
    }
}
#endif


#ifdef GAME_DLL
void ZMCreateStringTables()
{
    extern INetworkStringTable* g_pZMCustomPlyModels;
    g_pZMCustomPlyModels = networkstringtable->CreateStringTable( "ZMCustomModels", 32 );
}
#endif

CZMSystem g_ZMSystem;
