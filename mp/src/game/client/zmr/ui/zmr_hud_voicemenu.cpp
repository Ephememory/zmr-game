#include "cbase.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "input.h"
#include "view.h"
#include "filesystem.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IPanel.h>

#include <vgui_controls/TextEntry.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/AnimationController.h>
#include "hudelement.h"


#include "zmr_framepanel.h"

#include "c_zmr_player.h"
#include "zmr_voicelines.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Similar to AngleNormalize
float AngleNormalizeRadians( float angle )
{
    angle = fmodf( angle, M_PI_F*2 );
    if ( angle > M_PI_F )
        angle -= M_PI_F*2;
    if ( angle < -M_PI_F )
        angle += M_PI_F*2;
    return angle;
}


// The index player may pass in +zm_voicemenu 1
enum
{
    RADIALINDEX_INVALID = -1,


    RADIALINDEX_TEAM,
    // Current not used.
    RADIALINDEX_USELESS,

    RADIALINDEX_SURVIVOR_MAX,



    // In case we ever add a ZM one.
    RADIALINDEX_ZM = RADIALINDEX_SURVIVOR_MAX,


    RADIALINDEX_ZM_MAX
};


struct ZMVoiceMenuData_t
{
    ZMVoiceMenuData_t( const char* localization, int index )
    {
        Q_strncpy( m_szLocalization, localization, sizeof( m_szLocalization ) );
        m_wszCachedTxt[0] = NULL;
        m_iIndex = index;
    }

    char m_szLocalization[64];
    wchar_t m_wszCachedTxt[64];
    int m_iIndex;
};

struct ZMVoiceMenu_t
{
    ~ZMVoiceMenu_t()
    {
        m_vItems.PurgeAndDeleteElements();
    }

    CUtlVector<ZMVoiceMenuData_t*> m_vItems;
    bool m_bOffset;
};


class CZMHudVoiceMenu : public CHudElement, public CZMFramePanel
{
public:
    DECLARE_CLASS_SIMPLE( CZMHudVoiceMenu, CZMFramePanel );

    CZMHudVoiceMenu( const char* pElementName );
    ~CZMHudVoiceMenu();


    virtual void Init() OVERRIDE;
    virtual void VidInit() OVERRIDE;
    virtual void Reset() OVERRIDE;


    virtual bool ShouldDraw() OVERRIDE { return IsVisible() && CHudElement::ShouldDraw(); }

    virtual void OnMousePressed( vgui::MouseCode code ) OVERRIDE;
    virtual void OnThink() OVERRIDE;
    virtual void Paint() OVERRIDE;


    virtual void SetVisible( bool state ) OVERRIDE;



    virtual ZMVoiceMenu_t* GetPreferredVoiceMenu( int radialIndex );

    void LoadMenus();

    
    void BeginFadeIn( ZMVoiceMenu_t* pMenu );
    void BeginFadeOut();
    void PerformVoice();

    float GetRadialSize();
    float GetStartingAngle( int nElements, bool offset = false ) const;

protected:
    void UpdateSelection();


    bool LoadMenuTo( ZMVoiceMenu_t& vMenu, const char* path );


    void PaintString( const vgui::HFont font, const Color& clr, int x, int y, const wchar_t* txt );

private:
    CPanelAnimationVar( vgui::HFont, m_hFontNormal, "NormalFont", "ZMHudVoiceMenuNormal" );
    CPanelAnimationVar( vgui::HFont, m_hFontSel, "SelectedFont", "ZMHudVoiceMenuSelected" );

    CPanelAnimationVarAliasType( float, m_flSize, "RadialSize", "200", "proportional_float" );
    CPanelAnimationVar( float, m_flTextPosScale, "TextPosScale", "0.7" );
    CPanelAnimationVar( float, m_flFadeInTime, "FadeInTime", "0.2" );
    CPanelAnimationVar( float, m_flFadeOutTime, "FadeOutTime", "0.2" );

    CPanelAnimationVar( Color, m_BgColor, "BgColor", "ZMHudBgColor" );


    ZMVoiceMenu_t m_MenuTeam;
    ZMVoiceMenu_t m_MenuUseless;
    ZMVoiceMenu_t* m_pCurrentMenu;


    int m_nTexBgId;


    int m_iCurSelection;

    float m_flInnerScale;


    float m_flFadeIn;
    float m_flFadeOut;
};


CZMHudVoiceMenu* g_pVoiceMenu = nullptr;






static void IN_ZM_VoiceMenu1( const CCommand& args )
{
    bool newstate = ( args.Arg( 0 )[0] == '+' ) ? true : false;

    int index = atoi( args.Arg( 1 ) );


    if ( g_pVoiceMenu )
    {
        ZMVoiceMenu_t* pMenu = g_pVoiceMenu->GetPreferredVoiceMenu( index );

        if ( pMenu )
        {
            if ( newstate )
            {
                g_pVoiceMenu->BeginFadeIn( pMenu );
            }
            else
            {
                g_pVoiceMenu->BeginFadeOut();

                g_pVoiceMenu->PerformVoice();
            }
        }
    }
}
ConCommand zm_voicemenu1_up( "+zm_voicemenu", IN_ZM_VoiceMenu1 );
ConCommand zm_voicemenu1_down( "-zm_voicemenu", IN_ZM_VoiceMenu1 );




using namespace vgui;

DECLARE_HUDELEMENT( CZMHudVoiceMenu );

CZMHudVoiceMenu::CZMHudVoiceMenu( const char* pElementName ) : CHudElement( pElementName ), CZMFramePanel( g_pClientMode->GetViewport(), "ZMHudVoiceMenu" )
{
    g_pVoiceMenu = this;


    SetPaintBackgroundEnabled( false );
    SetVisible( false );
    SetKeyBoardInputEnabled( false );



    m_nTexBgId = surface()->CreateNewTextureID();
    surface()->DrawSetTextureFile( m_nTexBgId, "zmr_effects/hud_bg_voicemenu", true, false );


    m_iCurSelection = -1;
    
    m_flInnerScale = 0.12f;

    m_pCurrentMenu = nullptr;
    LoadMenus();
}

CZMHudVoiceMenu::~CZMHudVoiceMenu()
{
}

void CZMHudVoiceMenu::LoadMenus()
{
    m_MenuTeam.m_vItems.PurgeAndDeleteElements();
    m_MenuUseless.m_vItems.PurgeAndDeleteElements();


    LoadMenuTo( m_MenuTeam, "resource/zmvoicemenu.txt" );
    LoadMenuTo( m_MenuUseless, "resource/zmvoicemenu2.txt" );
}

bool CZMHudVoiceMenu::LoadMenuTo( ZMVoiceMenu_t& vMenu, const char* path )
{
    KeyValues* kv = new KeyValues( "VoiceMenu" );
    if ( !kv->LoadFromFile( filesystem, path ) )
    {
        kv->deleteThis();
        return false;
    }


    vMenu.m_bOffset = kv->GetInt( "offset" );


    KeyValues* data = kv->GetFirstSubKey();
    do
    {
        int index = data->GetInt( "index", -1 );
        if ( index <= -1 )
            continue;

        const char* pLocal = data->GetString( "name" );
        if ( !pLocal || !(*pLocal) )
            continue;

        
        vMenu.m_vItems.AddToTail( new ZMVoiceMenuData_t( pLocal, index ) );
    }
    while ( (data = data->GetNextKey()) != nullptr );



    kv->deleteThis();

    return true;
}

void CZMHudVoiceMenu::Init()
{
    Reset();
}

void CZMHudVoiceMenu::VidInit()
{
    Reset();
}

void CZMHudVoiceMenu::Reset()
{
    // Center it
    SetPos( ScreenWidth() / 2 - GetWide() / 2, ScreenHeight() / 2 - GetTall() / 2 );
}

void CZMHudVoiceMenu::SetVisible( bool state )
{
    if ( IsVisible() == state )
        return;


    if ( state )
    {
        m_iCurSelection = -1;
    }

    BaseClass::SetVisible( state );
}

void CZMHudVoiceMenu::OnMousePressed( MouseCode code )
{
    if ( code == MOUSE_LEFT )
    {
        PerformVoice();
        BeginFadeOut();
    }
}

void CZMHudVoiceMenu::OnThink()
{
    UpdateSelection();
}

void CZMHudVoiceMenu::UpdateSelection()
{
    // We're fading out, ignore.
    if ( m_flFadeOut != 0.0f )
        return;



    m_iCurSelection = -1;


    int mx, my;
    ::input->GetFullscreenMousePos( &mx, &my );

    int cx = GetXPos() + GetWide() / 2;
    int cy = GetYPos() + GetTall() / 2;


    Vector2D v1( (float)(mx - cx), (float)(my - cy) );
    float length = v1.NormalizeInPlace();

    // The mouse is considered to be in the "center".
    if ( length < (m_flSize*m_flInnerScale) )
    {
        return;
    }


    if ( !m_pCurrentMenu )
        return;

    int count = m_pCurrentMenu->m_vItems.Count();
    
    float dir = GetStartingAngle( count, m_pCurrentMenu->m_bOffset );
    float jump = (M_PI_F*2) / count;

    for ( int i = 0; i < count; i++ )
    {
        Vector2D v2( cos( dir ), sin( dir ) );


        if ( v2.Dot( v1 ) > 0.9f )
        {
            m_iCurSelection = i;
            break;
        }


        dir = AngleNormalizeRadians( dir + jump );
    }
}

float CZMHudVoiceMenu::GetRadialSize()
{
    return MIN( m_flSize, (float)(GetWide() > GetTall() ? GetTall() : GetWide()) );
}

float CZMHudVoiceMenu::GetStartingAngle( int nElements, bool offset ) const
{
    float up = -(M_PI_F/2.0f);

    return offset ? ( up - (M_PI_F*2) / nElements / 2 ) : up;
}

void CZMHudVoiceMenu::BeginFadeIn( ZMVoiceMenu_t* pMenu )
{
    m_iCurSelection = -1;


    m_pCurrentMenu = pMenu;

    m_flFadeOut = 0.0f;
    m_flFadeIn = gpGlobals->curtime;

    SetVisible( true );
    SetMouseInputEnabled( true );

    // Center mouse
    int mx, my;
    mx = GetXPos() + GetWide() / 2;
    my = GetYPos() + GetTall() / 2;

    ::input->SetFullscreenMousePos( mx, my );
}

void CZMHudVoiceMenu::BeginFadeOut()
{
    // We've already faded out!
    if ( !IsVisible() )
        return;

    // We're in the process of fading out.
    if ( m_flFadeOut > gpGlobals->curtime )
        return;


    m_flFadeOut = gpGlobals->curtime + m_flFadeOutTime;
    m_flFadeIn = 0.0f;

    SetMouseInputEnabled( false );
}

void CZMHudVoiceMenu::PerformVoice()
{
    if ( !m_pCurrentMenu )
        return;

    if ( !m_pCurrentMenu->m_vItems.IsValidIndex( m_iCurSelection ) )
        return;


    ZMGetVoiceLines()->SendVoiceLine( m_pCurrentMenu->m_vItems[m_iCurSelection]->m_iIndex );
}

ZMVoiceMenu_t* CZMHudVoiceMenu::GetPreferredVoiceMenu( int radialIndex )
{
    C_ZMPlayer* pLocal = C_ZMPlayer::GetLocalPlayer();
    if ( !pLocal )
        return nullptr;


    if ( pLocal->IsHuman() )
    {
        switch ( radialIndex )
        {
        case RADIALINDEX_USELESS : return &m_MenuUseless;
        case RADIALINDEX_TEAM :
        default :
            return &m_MenuTeam;
        }
    }

    return nullptr;
}

void CZMHudVoiceMenu::Paint()
{
    int cx = GetWide() / 2;
    int cy = GetTall() / 2;


    
    float size = GetRadialSize();
    

    int alpha = 255;

    if ( m_flFadeIn != 0.0f )
    {
        alpha = (gpGlobals->curtime - m_flFadeIn) / m_flFadeInTime * 255;
        if ( alpha >= 255 )
            m_flFadeIn = 0.0f;
    }
    else if ( m_flFadeOut != 0.0f )
    {
        alpha = (m_flFadeOut - gpGlobals->curtime) / m_flFadeOutTime * 255;
        if ( alpha <= 0 )
        {
            SetVisible( false );
            m_flFadeOut = 0.0f;
        }
    }

    alpha = clamp( alpha, 0, 255 );


    // Draw background
    float halfsize = size / 2;
    Color bgclr = m_BgColor;
    if ( alpha > bgclr[4] )
        bgclr[4] = alpha;

    surface()->DrawSetColor( bgclr );
    surface()->DrawSetTexture( m_nTexBgId );
    surface()->DrawTexturedRect( cx - halfsize, cy - halfsize, cx + halfsize, cy + halfsize );



    size = halfsize * m_flTextPosScale;


    if ( !m_pCurrentMenu )
        return;

    int count = m_pCurrentMenu->m_vItems.Count();
    float dir = GetStartingAngle( count, m_pCurrentMenu->m_bOffset );
    float jump = (M_PI_F*2) / count;




    // Draw the text
    const Color clr( 255, 255, 255, alpha );



    for ( int i = 0; i < count; i++ )
    {
        ZMVoiceMenuData_t* pData = m_pCurrentMenu->m_vItems[i];

        float vx, vy;

        //float rad = DEG2RAD( dir );
        vx = cos( dir );
        vy = sin( dir );


        if ( pData->m_wszCachedTxt[0] == NULL )
        {
            Q_wcsncpy( pData->m_wszCachedTxt, g_pVGuiLocalize->Find( pData->m_szLocalization ), sizeof( pData->m_wszCachedTxt ) );
        }

        const wchar_t* txt = pData->m_wszCachedTxt;

        HFont font = m_iCurSelection == i ? m_hFontSel : m_hFontNormal;

        int w, h;
        surface()->GetTextSize( font, txt, w, h );


        float ex = cx + vx * size;
        float ey = cy + vy * size;

        PaintString( font, clr, ex - w / 2, ey - h / 2, txt );


        dir = AngleNormalizeRadians( dir + jump );
    }


    // Draw separation lines
    /*
    dir = -90.0f + jump / 2;
    for ( int i = 0; i < count; i++ )
    {
        float vx, vy;

        float rad = DEG2RAD( dir );
        vx = cos( rad );
        vy = sin( rad );


        float sx = cx + vx * (size*m_flInnerScale);
        float sy = cy + vy * (size*m_flInnerScale);

        float ex = cx + vx * size;
        float ey = cy + vy * size;



        const Color clr( 192, 192, 192, 64 );

        surface()->DrawSetColor( clr );
        surface()->DrawLine( sx, sy, ex, ey );


        dir = AngleNormalize( dir + jump );
    }
    */
}

void CZMHudVoiceMenu::PaintString( const HFont font, const Color& clr, int x, int y, const wchar_t* txt )
{
    surface()->DrawSetTextPos( x, y );
    surface()->DrawSetTextColor( clr );
    surface()->DrawSetTextFont( font );
	surface()->DrawUnicodeString( txt );
}


CON_COMMAND( zm_hud_reloadvoicemenu, "" )
{
    if ( g_pVoiceMenu )
    {
        g_pVoiceMenu->LoadMenus();
    }
}
