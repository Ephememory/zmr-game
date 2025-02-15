#pragma once


//
abstract_class CZMBaseSoftCol
{
public:
    CZMBaseSoftCol( CBaseEntity* pOrigin, CBaseEntity* pOther )
    {
        hOrigin.Set( pOrigin );
        hOther.Set( pOther );
        vecLastDir = Vector2D( 1.0f, 1.0f );
    }
    virtual ~CZMBaseSoftCol() {}

    bool Equals( const CBaseEntity* pOrigin, const CBaseEntity* pOther )
    {
        return hOrigin.Get() == pOrigin && hOther.Get() == pOther;
    }


    enum SoftColRes_t
    {
        COLRES_NONE = 0,
        COLRES_APPLIED,

        COLRES_INVALID
    };



    SoftColRes_t PerformCollision() { return COLRES_NONE; }


protected:
    EHANDLE hOrigin;
    EHANDLE hOther;
    Vector2D vecLastDir;
};

#ifdef GAME_DLL
class CZMZombieSoftCol : public CZMBaseSoftCol
{
public:
    CZMZombieSoftCol( CBaseEntity* pOrigin, CBaseEntity* pOther ) : CZMBaseSoftCol( pOrigin, pOther ) {}

    SoftColRes_t PerformCollision();
};
#endif

class CZMPlayerSoftCol : public CZMBaseSoftCol
{
public:
    CZMPlayerSoftCol( CBaseEntity* pOrigin, CBaseEntity* pOther ) : CZMBaseSoftCol( pOrigin, pOther ) { m_bOriginMovedLast = false; }

    SoftColRes_t PerformCollision( CUserCmd* pCmd );

protected:
    bool m_bOriginMovedLast;
};
//


//
class CZMSoftCollisions : 
#ifdef GAME_DLL
    public CAutoGameSystemPerFrame
#else
    // IMPORTANT: We need to call update manually through local player, in prediction, to get results.
    public CAutoGameSystem
#endif
{
public:
#ifdef GAME_DLL
    virtual void FrameUpdatePostEntityThink() OVERRIDE;
#endif

    
#ifdef GAME_DLL
    void OnZombieCollide( CBaseEntity* pOrigin, CBaseEntity* pOther );
#else
    void OnPlayerCollide( CBaseEntity* pOrigin, CBaseEntity* pOther );
    void PerformPlayerSoftCollisions( CUserCmd* pCmd );
#endif

protected:
#ifdef GAME_DLL
    void PerformZombieSoftCollisions();
    void ClearZombieCollisions();
    CUtlVector<CZMZombieSoftCol> m_vZombieCollisions;
#else
    void ClearPlayerCollisions();
    CUtlVector<CZMPlayerSoftCol> m_vPlayerCollisions;
#endif
};
//


extern CZMSoftCollisions* GetZMSoftCollisions();
