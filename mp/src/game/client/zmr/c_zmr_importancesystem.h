#pragma once


enum ZMImportance_t
{
    ZMIMPORTANCE_NONE = -1,

    ZMIMPORTANCE_DEV,
    ZMIMPORTANCE_VIP,
    ZMIMPORTANCE_PLAYTESTER,

    ZMIMPORTANCE_MAX
};


//
//
//
class C_ZMImportanceSystem : public CAutoGameSystem
{
public:
    struct ImportanceData_t
    {
        void Init( int userId );
        bool IsValid( int playerIndex ) const;

        int userId;
        ZMImportance_t importance;
    };


    C_ZMImportanceSystem();
    ~C_ZMImportanceSystem();


    virtual void PostInit() OVERRIDE;
    virtual void LevelInitPostEntity() OVERRIDE;



    void Reset();

    const char* GetPlayerImportanceName( int playerIndex );
    ZMImportance_t GetPlayerImportance( int playerIndex );

    bool IsCached( int playerIndex );

protected:
    bool LoadFromFile();
    int FindSteamIdIndex( uint64 steamId );


    bool ComputePlayerImportance( int playerIndex );

    static const char* ImportanceToName( ZMImportance_t index );
    static ZMImportance_t ImportanceNameToIndex( const char* name );

private:
    ImportanceData_t m_Importance[MAX_PLAYERS];


    // Sorted 64bit steam ids.
    CUtlVector<uint64> m_vSteamIdIndices;

    CUtlVector<ZMImportance_t> m_vPlayerData;
};

extern C_ZMImportanceSystem g_ZMImportanceSystem;
