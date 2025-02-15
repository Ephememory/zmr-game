#include "cbase.h"
#include "gamestringpool.h"


#include "c_zmr_shambler.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define ZOMBIE_SANTAHAT     "models/props/misc/zombie_santahat01.mdl"



IMPLEMENT_CLIENTCLASS_DT( C_ZMShambler, DT_ZM_Shambler, CZMShambler )
END_RECV_TABLE()


C_ZMShambler::C_ZMShambler()
{
    SetZombieClass( ZMCLASS_SHAMBLER );
}

C_ZMShambler::~C_ZMShambler()
{
}

void C_ZMShambler::FootstepSound( bool bRightFoot )
{
    PlayFootstepSound( bRightFoot ? "Zombie.FootstepRight" : "Zombie.FootstepLeft" );
}

void C_ZMShambler::FootscuffSound( bool bRightFoot )
{
    PlayFootstepSound( bRightFoot ? "Zombie.ScuffRight" : "Zombie.ScuffLeft" );
}

void C_ZMShambler::AttackSound()
{
    EmitSound( "Zombie.Attack" );
}

bool C_ZMShambler::CreateEventAccessories()
{
    bool res = BaseClass::CreateEventAccessories();

    if ( res )
        MakeHappy();

    return res;
}

bool C_ZMShambler::IsAffectedByEvent( HappyZombieEvent_t iEvent ) const
{
    return iEvent == HZEVENT_CHRISTMAS;
}

const char* C_ZMShambler::GetEventHatModel( HappyZombieEvent_t iEvent ) const
{
    return ZOMBIE_SANTAHAT;
}

void C_ZMShambler::MakeHappy()
{
    SetFlexWeightSafe( "smile", 1.0f );
    SetFlexWeightSafe( "jaw_clench", 1.0f );
    SetFlexWeightSafe( "right_inner_raiser", 1.0f );
    SetFlexWeightSafe( "right_outer_raiser", 1.0f );
    SetFlexWeightSafe( "left_lowerer", 1.0f );
}

LocalFlexController_t C_ZMShambler::GetFlexControllerNumByName( const char* pszName )
{
    for ( LocalFlexController_t i = (LocalFlexController_t)1; i < GetNumFlexControllers(); i++ )
    {
        if ( Q_strcmp( GetFlexControllerName( i ), pszName ) == 0 )
        {
            return i;
        }
    }

    return (LocalFlexController_t)0;
}

void C_ZMShambler::SetFlexWeightSafe( const char* pszName, float value )
{
    LocalFlexController_t cntrl = GetFlexControllerNumByName( pszName );

    if ( cntrl == (LocalFlexController_t)0 )
        return;


    SetFlexWeight( cntrl, value );
}
