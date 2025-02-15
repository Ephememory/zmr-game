//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#include "optionssubkeyboard.h"
#include "engineinterface.h"
#include "vcontrolslistpanel.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ScrollBar.h"

#include "vgui/Cursor.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui/ISystem.h"
#include "vgui/IInput.h"

#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "IGameUIFuncs.h"
#include "vstdlib/IKeyValuesSystem.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;



//-----------------------------------------------------------------------------
// Purpose: advanced keyboard settings dialog
//-----------------------------------------------------------------------------
class COptionsSubKeyboardAdvancedDlg : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( COptionsSubKeyboardAdvancedDlg, vgui::Frame );
public:
	COptionsSubKeyboardAdvancedDlg( vgui::VPANEL hParent ) : BaseClass( NULL, NULL )
	{
		// parent is ignored, since we want look like we're steal focus from the parent (we'll become modal below)

		SetTitle("#GameUI_KeyboardAdvanced_Title", true);
		SetSize( 280, 140 );
		LoadControlSettings( "resource/OptionsSubKeyboardAdvancedDlg.res" );
		MoveToCenterOfScreen();
		SetSizeable( false );
		SetDeleteSelfOnClose( true );
	}

	virtual void Activate()
	{
		BaseClass::Activate();

		input()->SetAppModalSurface(GetVPanel());
	}

	virtual void OnResetData()
	{
		// reset the data
		ConVarRef con_enable( "con_enable" );
		if ( con_enable.IsValid() )
		{
			SetControlInt("ConsoleCheck", con_enable.GetBool() ? 1 : 0);
		}

		ConVarRef hud_fastswitch( "hud_fastswitch", true );
		if ( hud_fastswitch.IsValid() )
		{
			SetControlInt("FastSwitchCheck", hud_fastswitch.GetBool() ? 1 : 0);
		}
	}

	virtual void OnApplyChanges()
	{
		// apply data
		ConVarRef con_enable( "con_enable" );
		con_enable.SetValue( GetControlInt( "ConsoleCheck", 0 ) );

		ConVarRef hud_fastswitch( "hud_fastswitch", true );
		hud_fastswitch.SetValue( GetControlInt( "FastSwitchCheck", 0 ) );
	}

	virtual void OnCommand( const char *command )
	{
		if ( !stricmp(command, "OK") )
		{
			// apply the data
			OnApplyChanges();
			Close();
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

	void OnKeyCodeTyped(KeyCode code)
	{
		// force ourselves to be closed if the escape key it pressed
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodeTyped(code);
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubKeyboard::COptionsSubKeyboard(vgui::Panel *parent) : PropertyPage(parent, "OptionsSubKeyboard" )
{
#ifdef SWARM_DLL
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	SetScheme(scheme);
#endif


	m_nSplitScreenUser = 0;

	// For joystick buttons, controls which user are binding/unbinding
	if ( !IsX360() )
	{
		//HACK HACK:  Probably the entire gameui needs to have a splitscrene context for which player the settings apply to, but this is only
		// on the PC...
		static ConVarRef in_forceuser( "in_forceuser" );

		if ( in_forceuser.IsValid() )
		{
			m_nSplitScreenUser = clamp( in_forceuser.GetInt(), 0, 1 );
		}
		else
		{
			//m_nSplitScreenUser = MAX( 0, engine->GetActiveSplitScreenPlayerSlot() );
		}
	}

	// create the key bindings list
	CreateKeyBindingList();
	// Parse default descriptions
	ParseActionDescriptions();
	
	m_pSetBindingButton = new Button(this, "ChangeKeyButton", "");
	m_pClearBindingButton = new Button(this, "ClearKeyButton", "");

	LoadControlSettings("Resource/OptionsSubKeyboard.res");

	m_pSetBindingButton->SetEnabled(false);
	m_pClearBindingButton->SetEnabled(false);
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubKeyboard::~COptionsSubKeyboard()
{
}

//-----------------------------------------------------------------------------
// Purpose: reloads current keybinding
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnResetData()
{
	// Populate list based on current settings
	FillInCurrentBindings();
	if ( IsVisible() )
	{
		m_pKeyBindList->SetSelectedItem(0);
	}

	if ( m_OptionsSubKeyboardAdvancedDlg.Get() != nullptr )
	{
		m_OptionsSubKeyboardAdvancedDlg.Get()->OnResetData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: saves out keybinding changes
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnApplyChanges()
{
	ApplyAllBindings();

	if ( m_OptionsSubKeyboardAdvancedDlg.Get() != nullptr )
	{
		m_OptionsSubKeyboardAdvancedDlg.Get()->OnApplyChanges();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create key bindings list control
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::CreateKeyBindingList()
{
	// Create the control
	m_pKeyBindList = new VControlsListPanel(this, "listpanel_keybindlist");
    //m_pKeyBindList->GetScrollBar()->UseImages( nullptr, nullptr, nullptr, nullptr );
	//m_pKeyBindList->GetScrollBar()->UseImages( "scroll_up", "scroll_down", "scroll_line", "scroll_box" );
}

//-----------------------------------------------------------------------------
// Purpose: binds double-clicking or hitting enter in the keybind list to changing the key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnKeyCodeTyped(vgui::KeyCode code)
{
	if (code == KEY_ENTER)
	{
		OnCommand("ChangeKey");
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: command handler
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnCommand( const char *command )
{
	if ( !stricmp( command, "Defaults" )  )
	{
		// open a box asking if we want to restore defaults
		QueryBox *box = new QueryBox("#GameUI_KeyboardSettings", "#GameUI_KeyboardSettingsText");
		box->AddActionSignalTarget(this);
		box->SetOKCommand(new KeyValues("Command", "command", "DefaultsOK"));
		box->DoModal();
	}
	else if ( !stricmp(command, "DefaultsOK"))
	{
		// Restore defaults from default keybindings file
		FillInDefaultBindings();
		m_pKeyBindList->RequestFocus();
	}
	else if ( !m_pKeyBindList->IsCapturing() && !stricmp( command, "ChangeKey" ) )
	{
		m_pKeyBindList->StartCaptureMode(dc_blank);
	}
	else if ( !m_pKeyBindList->IsCapturing() && !stricmp( command, "ClearKey" ) )
	{
		// OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_DELETE, CL4DBasePanel::GetSingleton().GetLastActiveUserId() ) );
		OnKeyCodePressed( KEY_DELETE ); // <<< PC only code, no need for joystick management
        m_pKeyBindList->RequestFocus();
	}
	else if ( !stricmp(command, "Advanced") )
	{
		OpenKeyboardAdvancedDialog();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

const char *UTIL_Parse( const char *data, char *token, int sizeofToken )
{
	data = engine->ParseFile( data, token, sizeofToken );
	return data;
}
static char *UTIL_CopyString( const char *in )
{
	int len = strlen( in ) + 1;
	char *out = new char[ len ];
	Q_strncpy( out, in, len );
	return out;
}

char *UTIL_va(char *format, ...)
{
	va_list		argptr;
	static char	string[4][1024];
	static int	curstring = 0;
	
	curstring = ( curstring + 1 ) % 4;

	va_start (argptr, format);
	Q_vsnprintf( string[curstring], 1024, format, argptr );
	va_end (argptr);

	return string[curstring];  
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ParseActionDescriptions( void )
{
	char szBinding[256];
	char szDescription[256];

	KeyValues *item;

	// Load the default keys list
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( "scripts/kb_act.lst", NULL, buf ) )
		return;

	const char *data = (const char*)buf.Base();

	int sectionIndex = 0;
	char token[512];
	while ( 1 )
	{
		data = UTIL_Parse( data, token, sizeof(token) );
		// Done.
		if ( strlen( token ) <= 0 )  
			break;

		Q_strncpy( szBinding, token, sizeof( szBinding ) );

		data = UTIL_Parse( data, token, sizeof(token) );
		if ( strlen(token) <= 0 )
		{
			break;
		}

		Q_strncpy(szDescription, token, sizeof( szDescription ) );

		// Skip '======' rows
		if ( szDescription[ 0 ] != '=' )
		{
			// Flag as special header row if binding is "blank"
			if (!stricmp(szBinding, "blank"))
			{
				// add header item
				int nColumn1 = 286;
				int nColumn2 = 128;
				if ( IsProportional() )
				{
					nColumn1 = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), nColumn1 );
					nColumn2 = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), nColumn2 );
				}
				m_pKeyBindList->AddSection(++sectionIndex, szDescription);
				m_pKeyBindList->AddColumnToSection(sectionIndex, "Action", szDescription, SectionedListPanel::COLUMN_BRIGHT, nColumn1 );
				m_pKeyBindList->AddColumnToSection(sectionIndex, "Key", "#GameUI_KeyButton", SectionedListPanel::COLUMN_BRIGHT, nColumn2 );
			}
			else
			{
				// Create a new: blank item
				item = new KeyValues( "Item" );
				
				// fill in data
				item->SetString("Action", szDescription);
				item->SetString("Binding", szBinding);
				item->SetString("Key", "");

				// Add to list
				m_pKeyBindList->AddItem(sectionIndex, item);
				item->deleteThis();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Search current data set for item which has the specified binding string
// Input  : *binding - string to find
// Output : KeyValues or NULL on failure
//-----------------------------------------------------------------------------
KeyValues *COptionsSubKeyboard::GetItemForBinding( const char *binding )
{
	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString("Binding");

	// Loop through all items
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		KeyValues *bindingItem = item->FindKey(bindingSymbol);
		const char *bindString = bindingItem->GetString();

		// Check the "Binding" key
		if (!stricmp(bindString, binding))
			return item;
	}
	// Didn't find it
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Bind the specified keyname to the specified item
// Input  : *item - Item to which to add the key
//			*keyname - The key to be added
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::AddBinding( KeyValues *item, const char *keyname )
{
	// See if it's already there as a binding
	if ( !stricmp( item->GetString( "Key", "" ), keyname ) )
		return;


    ButtonCode_t buttoncode = g_pInputSystem->StringToButtonCode( keyname );


	// Make sure we don't accidentally unbind this.
	{
		int index = m_KeysToUnbind.Find( buttoncode );
		if ( index != -1 )
		{
			m_KeysToUnbind.Remove( index );
		}
	}



	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString( "Binding" );
	static int keySymbol = KeyValuesSystem()->GetSymbolForString( "Key" );

	// Make sure it doesn't live anywhere
    auto myKeyTeam = CZMTeamKeysConfig::GetCommandType( item->GetString( bindingSymbol, "" ) );

	RemoveKeyFromBindItems( myKeyTeam, keyname );

	const char *binding = item->GetString( bindingSymbol, "" );

	// Loop through all the key bindings and set all entries that have
	// the same binding. This allows us to have multiple entries pointing 
	// to the same binding.
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *curitem = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !curitem )
			continue;

		const char *curbinding = curitem->GetString( bindingSymbol, "" );

		if (!stricmp(curbinding, binding))
		{
			ButtonCode_t oldcode = g_pInputSystem->StringToButtonCode( curitem->GetString( keySymbol ) );

			curitem->SetString( "Key", keyname );
			m_pKeyBindList->InvalidateItem(i);

            // We need to unbind the old key
            
            if ( m_KeysToUnbind.Find( oldcode ) == -1 && oldcode > KEY_NONE && oldcode != buttoncode )
                m_KeysToUnbind.AddToTail( oldcode );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove all keys from list
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ClearBindItems( void )
{
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// Reset keys
		item->SetString( "Key", "" );

		m_pKeyBindList->InvalidateItem(i);
	}

	m_pKeyBindList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Remove all instances of the specified key from bindings
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::RemoveKeyFromBindItems( ZMKeyTeam_t bindTeam, const char *key )
{
	Assert( key && *key );
	if ( !key || !(*key) )
		return;



	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString( "Binding" );
	static int keySymbol = KeyValuesSystem()->GetSymbolForString( "Key" );


	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// If it's bound to the primary: then remove it
		if ( !stricmp( key, item->GetString( keySymbol, "" ) ) )
		{
			bool bClearEntry = true;


			auto myKeyTeam = CZMTeamKeysConfig::GetCommandType( item->GetString( bindingSymbol, "" ) );

			if ( !CZMTeamKeysConfig::IsKeyConflicted( myKeyTeam, bindTeam ) )
			{
				bClearEntry = false;
			}


			if ( bClearEntry )
			{
				item->SetString( "Key", "" );
				m_pKeyBindList->InvalidateItem(i);
			}
		}
	}


	// Make sure the display is up to date
	m_pKeyBindList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Ask the engine for all bindings and set up the list
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::FillInCurrentBindings( void )
{
	// reset the unbind list
	// we only unbind keys used by the normal config items (not custom binds)
	m_KeysToUnbind.RemoveAll();

	// Clear any current settings
	ClearBindItems();

	bool bJoystick = false;
	ConVarRef var( "joystick" );
	if ( var.IsValid() )
	{
		bJoystick = var.GetBool();
	}



	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString( "Binding" );


	zmkeydatalist_t zmkeys, survivorkeys, speckeys;
	CZMTeamKeysConfig::LoadConfigByTeam( KEYTEAM_ZM, zmkeys );
	CZMTeamKeysConfig::LoadConfigByTeam( KEYTEAM_SURVIVOR, survivorkeys );
	CZMTeamKeysConfig::LoadConfigByTeam( KEYTEAM_SPEC, speckeys );



	for ( int i = 0; i < m_pKeyBindList->GetItemCount(); i++ )
	{
		auto* itemData = m_pKeyBindList->GetItemData( m_pKeyBindList->GetItemIDFromRow( i ) );
		if ( !itemData )
			continue;


		const char* binding = itemData->GetString( bindingSymbol );

		auto bindTeam = CZMTeamKeysConfig::GetCommandType( binding );


		ButtonCode_t bindingCode = gameuifuncs->GetButtonCodeForBind( binding );


		if ( bindingCode <= KEY_NONE )
		{
			if ( bindTeam != KEYTEAM_NEUTRAL )
			{
				zmkeydata_t* pKey = nullptr;
			

                switch ( bindTeam )
                {
                case KEYTEAM_ZM :
                    pKey = CZMTeamKeysConfig::FindKeyDataFromList( binding, zmkeys );
                    break;
                case KEYTEAM_SURVIVOR :
                    pKey = CZMTeamKeysConfig::FindKeyDataFromList( binding, survivorkeys );
                    break;
                case KEYTEAM_SPEC :
                    pKey = CZMTeamKeysConfig::FindKeyDataFromList( binding, speckeys );
                    break;
                }

				if ( pKey )
					bindingCode = pKey->key;
			}
		}


		const char* keyname =
			bindingCode > KEY_NONE ? g_pInputSystem->ButtonCodeToString( bindingCode ) : "";

        if ( bindingCode > KEY_NONE )
        {
            RemoveKeyFromBindItems( bindTeam, keyname );
        }

		itemData->SetString( "Key", keyname );
	}


	zmkeys.PurgeAndDeleteElements();
	survivorkeys.PurgeAndDeleteElements();
	speckeys.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to bind a key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::BindKey( ButtonCode_t bc, const char *binding )
{
	char const *pszKeyName = g_pInputSystem->ButtonCodeToString( bc );
	Assert( pszKeyName );
	if ( !pszKeyName || !*pszKeyName )
		return;

	//int nSlot = GetJoystickForCode( bc );
	engine->ClientCmd_Unrestricted( UTIL_va( "bind \"%s\" \"%s\"\n", pszKeyName, binding ) );
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to unbind a key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::UnbindKey( ButtonCode_t bc )
{
	char const *pszKeyName = g_pInputSystem->ButtonCodeToString( bc );
	Assert( pszKeyName );
	if ( !pszKeyName || !*pszKeyName )
		return;

	//int nSlot = GetJoystickForCode( bc );
	engine->ClientCmd_Unrestricted( UTIL_va( "unbind \"%s\"\n", pszKeyName ) );
}

//-----------------------------------------------------------------------------
// Purpose: Go through list and bind specified keys to actions
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ApplyAllBindings( void )
{
	// unbind everything that the user unbound
	for (int i = 0; i < m_KeysToUnbind.Count(); i++)
	{
		ButtonCode_t bc = m_KeysToUnbind[ i ];
		UnbindKey( bc );
	}
	m_KeysToUnbind.RemoveAll();


	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString( "Binding" );
	static int keySymbol = KeyValuesSystem()->GetSymbolForString( "Key" );




    zmkeydatalist_t zmkeys, survivorkeys, speckeys;

    // Loads the keys from configs as well to make sure we don't accidentally remove some bind.
    CZMTeamKeysConfig::LoadConfigByTeam( KEYTEAM_ZM, zmkeys );
    CZMTeamKeysConfig::LoadConfigByTeam( KEYTEAM_SURVIVOR, survivorkeys );
    CZMTeamKeysConfig::LoadConfigByTeam( KEYTEAM_SPEC, speckeys );

	zmkeydata_t keydata;


	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// See if it has a binding
		const char *binding = item->GetString( bindingSymbol, "" );
		if ( !binding || !binding[ 0 ] )
			continue;

		const char *keyname;
		
		// Check main binding
		keyname = item->GetString( keySymbol, "" );


        Q_strncpy( keydata.cmd, binding, sizeof( keydata.cmd ) );
        keydata.key = g_pInputSystem->StringToButtonCode( keyname );


        bool bKeyIsBound = keydata.key > BUTTON_CODE_NONE;


        zmkeydatalist_t* pKeyList = nullptr;

        ZMKeyTeam_t keyteam = CZMTeamKeysConfig::GetCommandType( binding );

        switch ( keyteam )
        {
        case KEYTEAM_ZM :
            pKeyList = &zmkeys;
            break;
        case KEYTEAM_SURVIVOR :
            pKeyList = &survivorkeys;
            break;
        case KEYTEAM_SPEC :
            pKeyList = &speckeys;
        default :
            // It's a neutral command, just execute it right now.
            if ( bKeyIsBound )
            {
                BindKey( keydata.key, binding );
            }

            //engine->ClientCmd_Unrestricted( VarArgs( "bind \"%s\" \"%s\"", keyname, cmd ) );
            break;
        }
        

        if ( pKeyList )
        {
            auto* pKey = CZMTeamKeysConfig::FindKeyDataFromList( binding, *pKeyList );

            // We don't have a key to bind this to, just remove it.
            if ( !bKeyIsBound )
            {
                if ( pKey )
                {
                    pKeyList->FindAndRemove( pKey );
                    delete pKey;
                }

                continue;
            }

            auto* pOldKey = CZMTeamKeysConfig::FindKeyDataFromListByKey( keydata.key, *pKeyList );

            if ( pOldKey )
            {
                if ( pKey == pOldKey )
                    pKey = nullptr;

                pKeyList->FindAndRemove( pOldKey );
                delete pOldKey;
            }

            if ( pKey )
            {
                pKey->key = keydata.key;
            }
            else
            {
                pKeyList->AddToTail( new zmkeydata_t( keydata ) );
            }
        }
	}


    CZMTeamKeysConfig::SaveConfig( KEYCONFIG_ZM, zmkeys );
    CZMTeamKeysConfig::SaveConfig( KEYCONFIG_SURVIVOR, survivorkeys );
    CZMTeamKeysConfig::SaveConfig( KEYCONFIG_SPEC, speckeys );

    zmkeys.PurgeAndDeleteElements();
    survivorkeys.PurgeAndDeleteElements();
    speckeys.PurgeAndDeleteElements();


	// Now exec their custom bindings
	engine->ClientCmd_Unrestricted( "exec userconfig.cfg\nhost_writeconfig\n" );

    // Forcefully execute at least one of the configs.
    // This is for the stock options menu, so it gets updated.
    CZMTeamKeysConfig::ExecuteTeamConfig( true );
}

//-----------------------------------------------------------------------------
// Purpose: Read in defaults from game's default config file and populate list 
//			using those defaults
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::FillInDefaultBindings( void )
{
	FileHandle_t fh = g_pFullFileSystem->Open( "cfg/config_default.cfg", "rb" );
	if (fh == FILESYSTEM_INVALID_HANDLE)
		return;

	// L4D: also unbind other keys
	engine->ClientCmd_Unrestricted( "unbindall\n" );

	int size = g_pFullFileSystem->Size(fh) + 1;
	CUtlBuffer buf( 0, size, CUtlBuffer::TEXT_BUFFER );
	g_pFullFileSystem->Read( buf.Base(), size, fh );
	g_pFullFileSystem->Close(fh);

	// NULL terminate!
	((char*)buf.Base())[ size - 1 ] = '\0';

	// Clear out all current bindings
	ClearBindItems();

	const char *data = (const char*)buf.Base();

	// loop through all the binding
	while ( data != NULL )
	{
		char cmd[64];
		data = UTIL_Parse( data, cmd, sizeof(cmd) );
		if ( cmd[ 0 ] == '\0' )
			break;

		if ( !Q_stricmp(cmd, "bind") ||
		     !Q_stricmp(cmd, "cmd2 bind") )
		{
			// FIXME:  If we ever support > 2 player splitscreen this will need to be reworked.
			int nJoyStick = 0;
			if ( !stricmp(cmd, "cmd2 bind") )
			{
				nJoyStick = 1;
			}

			// Key name
			char szKeyName[256];
			data = UTIL_Parse( data, szKeyName, sizeof(szKeyName) );
			if ( szKeyName[ 0 ] == '\0' )
				break; // Error

			char szBinding[256];
			data = UTIL_Parse( data, szBinding, sizeof(szBinding) );
			if ( szKeyName[ 0 ] == '\0' )  
				break; // Error

			// Skip it if it's a bind for the other slit
			if ( nJoyStick != m_nSplitScreenUser )
				continue;

			// Find item
			KeyValues *item = GetItemForBinding( szBinding );
			if ( item )
			{
				// Bind it
				AddBinding( item, szKeyName );
			}
		}
		else
		{
			// L4D: Use Defaults also resets cvars listed in config_default.cfg
			ConVarRef var( cmd );
			if ( var.IsValid() )
			{
				char szValue[256] = "";
				data = UTIL_Parse( data, szValue, sizeof(szValue) );
				var.SetValue( szValue );
			}
		}
	}
	
	PostActionSignal(new KeyValues("ApplyButtonEnable"));



    // Loads the keys from default config
    zmkeydatalist_t keylists[3];

    CZMTeamKeysConfig::LoadDefaultConfigByTeam( KEYTEAM_ZM, keylists[0] );
    CZMTeamKeysConfig::LoadDefaultConfigByTeam( KEYTEAM_SURVIVOR, keylists[1] );
    CZMTeamKeysConfig::LoadDefaultConfigByTeam( KEYTEAM_SPEC, keylists[2] );


    for ( int i = 0; i < 3; i++ )
    {
        for ( int j = 0; j < keylists[i].Count(); j++ )
        {
            auto* item = GetItemForBinding( keylists[i][j]->cmd );
            if ( item )
            {
                AddBinding( item, g_pInputSystem->ButtonCodeToString( keylists[i][j]->key ) );
            }
        }

        keylists[i].PurgeAndDeleteElements();
    }



	// Make sure escape key is always valid
    KeyValues* item = GetItemForBinding( "cancelselect" );
    if ( item )
    {
        // Bind it
        AddBinding( item, "ESCAPE" );
    }
}

//-----------------------------------------------------------------------------
// Purpose: User clicked on item: remember where last active row/column was
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ItemSelected(int itemID)
{
	m_pKeyBindList->SetItemOfInterest(itemID);

	if (m_pKeyBindList->IsItemIDValid(itemID))
	{
		// find the details, see if we should be enabled/clear/whatever
		m_pSetBindingButton->SetEnabled(true);

		KeyValues *kv = m_pKeyBindList->GetItemData(itemID);
		if (kv)
		{
			const char *key = kv->GetString("Key", NULL);
			if (key && *key)
			{
				m_pClearBindingButton->SetEnabled(true);
			}
			else
			{
				m_pClearBindingButton->SetEnabled(false);
			}

			if (kv->GetInt("Header"))
			{
				m_pSetBindingButton->SetEnabled(false);
			}
		}
	}
	else
	{
		m_pSetBindingButton->SetEnabled(false);
		m_pClearBindingButton->SetEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the capture has finished
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::Finish( ButtonCode_t code )
{
	int r = m_pKeyBindList->GetItemOfInterest();

	// Retrieve clicked row and column
	m_pKeyBindList->EndCaptureMode( dc_arrow );

	// Find item for this row
	KeyValues *item = m_pKeyBindList->GetItemData(r);
	if ( item )
	{
		// Handle keys: but never rebind the escape key
		// Esc just exits bind mode silently
		if ( code != BUTTON_CODE_NONE && code != KEY_ESCAPE && code != BUTTON_CODE_INVALID )
		{
			// Bind the named key
			AddBinding( item, g_pInputSystem->ButtonCodeToString( code ) );
			PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );	
		}

		m_pKeyBindList->InvalidateItem(r);
	}

	m_pSetBindingButton->SetEnabled(true);
	m_pClearBindingButton->SetEnabled(true);
}

//-----------------------------------------------------------------------------
// Purpose: Scans for captured key presses
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnThink()
{
	BaseClass::OnThink();


	if ( m_pKeyBindList->IsCapturing() )
	{
		ButtonCode_t code = BUTTON_CODE_INVALID;
		if ( engine->CheckDoneKeyTrapping( code ) )
		{
			Finish( code );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check for enter key and go into keybinding mode if it was pressed
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnKeyCodePressed(vgui::KeyCode code)
{
	// Enter key pressed and not already trapping next key/button press
	if ( !m_pKeyBindList->IsCapturing() )
	{
		// Grab which item was set as interesting
		int r = m_pKeyBindList->GetItemOfInterest();

		// Check that it's visible
		int x, y, w, h;
		bool visible = m_pKeyBindList->GetCellBounds(r, 1, x, y, w, h);
		if (visible)
		{
			if ( KEY_DELETE == code )
			{
                static int keySymbol = KeyValuesSystem()->GetSymbolForString( "Key" );


				// find the current binding and remove it
				KeyValues *kv = m_pKeyBindList->GetItemData(r);


                // Make sure we truly unbind this button.
                {
                    ButtonCode_t code = g_pInputSystem->StringToButtonCode( kv->GetString( keySymbol ) );

                    if ( m_KeysToUnbind.Find( code ) == -1 )
                        m_KeysToUnbind.AddToTail( code );
                }
                
                
				kv->SetString( "Key", "" );


				m_pClearBindingButton->SetEnabled(false);
				m_pKeyBindList->InvalidateItem(r);
				PostActionSignal(new KeyValues("ApplyButtonEnable"));

				// message handled, don't pass on
				return;
			}
		}
	}

	// Allow base class to process message instead
	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Open advanced keyboard options
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OpenKeyboardAdvancedDialog()
{
	if (!m_OptionsSubKeyboardAdvancedDlg.Get())
	{
		m_OptionsSubKeyboardAdvancedDlg = new COptionsSubKeyboardAdvancedDlg(GetVParent());
        m_OptionsSubKeyboardAdvancedDlg->OnResetData();
	}
	m_OptionsSubKeyboardAdvancedDlg->Activate();
}
