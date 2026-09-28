//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's menus: the team menu (class, character, "choose Hidden"),
//			the loadout menu and the marines' radio menu, laid out by their
//			Resource/UI .res files. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include <game/client/iviewport.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include "baseviewport.h"
#include "c_hidden_player.h"
#include "hidden_gamerules.h"
#include "hidden_classimage.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// The engine only loads resource/<game dir>_<language>.txt (hidden2013_english.txt, HL2MP's strings);
// Beta 4b's own strings (#HDN_*: the keyboard options, menus) are in hidden_<language>.txt.
class CHiddenLocalization : public CAutoGameSystem
{
public:
	CHiddenLocalization() : CAutoGameSystem( "CHiddenLocalization" ) {}

	virtual bool Init( void )
	{
		g_pVGuiLocalize->AddFile( "resource/hidden_%language%.txt", "GAME", true );
		return true;
	}
};

static CHiddenLocalization s_HiddenLocalization;

#define PANEL_HIDDEN_TEAM		"team_menu"
#define PANEL_HIDDEN_WEAPON		"weapon_menu"
#define PANEL_HIDDEN_RADIO		"radio_menu"

//-----------------------------------------------------------------------------
// A full-screen menu from a .res file (laid out for 640x480) whose buttons
// talk to the menu itself. The frame is named after its own entry in the .res
// file ("team", "radio"); the viewport knows it by its panel name.
//-----------------------------------------------------------------------------
class CHiddenMenu : public Frame, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CHiddenMenu, Frame );

public:
	CHiddenMenu( IViewPort *pViewPort, const char *pszName, const char *pszResName, const char *pszResFile ) :
		Frame( NULL, pszResName ), m_pViewPort( pViewPort ), m_pszPanelName( pszName ),
		m_iResX( 0 ), m_iResY( 0 ), m_iResWide( 640 ), m_iResTall( 480 ), m_bCenter( false )
	{
		SetScheme( "ClientScheme" );
		SetMoveable( false );
		SetSizeable( false );
		SetTitleBarVisible( false );
		SetProportional( true );

		// The build group lays out the children but leaves the frame itself at Frame's default size
		// (a scrap in the corner). Size the frame from its own entry first: children pinned to a
		// corner (pinCorner) are placed relative to it, and resizing it afterwards would push them
		// off-screen. PerformLayout keeps it at that size.
		KeyValues *pResource = new KeyValues( pszResFile );
		if ( pResource->LoadFromFile( g_pFullFileSystem, pszResFile, "GAME" ) )
		{
			KeyValues *pFrame = pResource->FindKey( pszResName );
			if ( pFrame )
			{
				m_iResX = pFrame->GetInt( "xpos" );
				m_iResY = pFrame->GetInt( "ypos" );
				m_iResWide = pFrame->GetInt( "wide", 640 );
				m_iResTall = pFrame->GetInt( "tall", 480 );
			}
		}
		pResource->deleteThis();

		ApplyResBounds();
		LoadControlSettings( pszResFile );

		InvalidateLayout();
	}

	virtual const char *GetName( void ) { return m_pszPanelName; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_MENUCONTROLS; }
	virtual VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( VPANEL parent ) { BaseClass::SetParent( parent ); }

	virtual void ShowPanel( bool bShow )
	{
		if ( BaseClass::IsVisible() == bShow )
			return;

		if ( bShow )
		{
			OnShow();
			Activate();
			SetMouseInputEnabled( true );
		}
		else
		{
			SetVisible( false );
			SetMouseInputEnabled( false );
		}

		m_pViewPort->ShowBackGround( false );
	}

protected:
	virtual void OnShow( void ) {}

	virtual void PerformLayout( void )
	{
		ApplyResBounds();
		BaseClass::PerformLayout();
	}

	void ApplyResBounds( void )
	{
		HScheme hScheme = GetScheme();
		int x = scheme()->GetProportionalScaledValueEx( hScheme, m_iResX );
		const int wide = scheme()->GetProportionalScaledValueEx( hScheme, m_iResWide );

		// Proportional layouts scale with the screen's height, so on a screen wider than 4:3 the
		// 640-wide layout leaves a gap on the right. Centred menus split it.
		if ( m_bCenter )
		{
			int iScreenWide, iScreenTall;
			surface()->GetScreenSize( iScreenWide, iScreenTall );
			x += MAX( 0, ( iScreenWide - scheme()->GetProportionalScaledValueEx( hScheme, 640 ) ) / 2 );
		}

		SetBounds( x, scheme()->GetProportionalScaledValueEx( hScheme, m_iResY ),
			wide, scheme()->GetProportionalScaledValueEx( hScheme, m_iResTall ) );
	}

	void Close( void ) { m_pViewPort->ShowPanel( this, false ); }

	// Moves a tick image to a spot given in the .res file's 640x480 layout.
	void PlaceImage( ImagePanel *pImage, int x, int y )
	{
		if ( !pImage )
			return;

		pImage->SetPos( scheme()->GetProportionalScaledValueEx( GetScheme(), x ), scheme()->GetProportionalScaledValueEx( GetScheme(), y ) );
		pImage->SetVisible( true );
	}

	static void ShowImage( ImagePanel *pImage, const char *pszImage )
	{
		if ( !pImage )
			return;

		pImage->SetVisible( true );
		pImage->SetImage( pszImage );
	}

	IViewPort *m_pViewPort;
	const char *m_pszPanelName;
	int m_iResX, m_iResY, m_iResWide, m_iResTall;	// the frame's own .res entry, in 640x480 units
	bool m_bCenter;	// centred on screens wider than 4:3
};

//-----------------------------------------------------------------------------
// The team menu (classmenu): hover a class or character to see it, click to
// pick; "enter" sends the picks (-2 keeps the current one) and opens the
// loadout. Taken characters are struck out, the player's own too.
//-----------------------------------------------------------------------------
static const struct
{
	const char *pszButton;
	const char *pszPortrait;
	const char *pszInfo;
} s_Characters[HIDDEN_NUM_CHARACTERS] =
{
	{ "MEM7BUTTON", "selection/wakefield", "selection/colininfo" },
	{ "MEM5BUTTON", "selection/sven", "selection/sveninfo" },
	{ "MEM3BUTTON", "selection/kuti", "selection/kutiinfo" },
	{ "MEM6BUTTON", "selection/swope", "selection/swopeinfo" },
	{ "MEM1BUTTON", "selection/choi", "selection/choiinfo" },
	{ "MEM4BUTTON", "selection/lewis", "selection/lewisinfo" },
	{ "MEM8BUTTON", "selection/wallace", "selection/duncaninfo" },
	{ "MEM2BUTTON", "selection/gruber", "selection/gruberinfo" },
	{ "MEM9BUTTON", "selection/kasim", "selection/kasiminfo" },
};

// The class and character just picked in the team menu (-2 to keep the current one), for the weapon
// menu that opens straight after, before the server has them.
static int s_iPickedClass = -2;
static int s_iPickedCharacter = -2;

// The menus' 3D preview shows this marine: a pick if there is one, else the player's own.
static void ShowPreviewMarine( int iCharacter, int iClass )
{
	C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
	if ( iCharacter < 0 && pPlayer )
		iCharacter = pPlayer->GetCharacter();
	if ( iClass < 0 && pPlayer )
		iClass = pPlayer->GetPlayerClass();

	HiddenClassImage_SetMarine( iCharacter, iClass );
}

class CHiddenTeamMenu : public CHiddenMenu
{
	DECLARE_CLASS_SIMPLE( CHiddenTeamMenu, CHiddenMenu );

public:
	CHiddenTeamMenu( IViewPort *pViewPort ) : CHiddenMenu( pViewPort, PANEL_HIDDEN_TEAM, "team", "Resource/UI/Teammenu.res" ),
		m_iClass( -2 ), m_iCharacter( -2 )
	{
		m_bCenter = true;
		m_pAssault = FindControl<Button>( "AssaultButton" );
		m_pSupport = FindControl<Button>( "SupportButton" );
		m_pPortrait = FindControl<ImagePanel>( "Marine_Pic" );
		m_pInfo = FindControl<ImagePanel>( "Marine_Info" );
		m_pClassTick = FindControl<ImagePanel>( "Tick" );
		m_pCharacterTick = FindControl<ImagePanel>( "Tick_02" );
		m_pReady = FindControl<ImagePanel>( "CROSS" );
		m_pChooseHidden = FindControl<ImagePanel>( "CROSS1" );

		for ( int i = 0; i < HIDDEN_NUM_CHARACTERS; i++ )
		{
			m_pCharacters[i] = FindControl<Button>( s_Characters[i].pszButton );
			// Each strike lies over the button with the same number (STRIKE_07 over MEM7BUTTON).
			m_pStrikes[i] = FindControl<ImagePanel>( VarArgs( "STRIKE_0%c", s_Characters[i].pszButton[3] ) );
			if ( m_pStrikes[i] )
				m_pStrikes[i]->SetMouseInputEnabled( false );
		}
	}

protected:
	virtual void OnShow( void )
	{
		m_iClass = m_iCharacter = -2;
		if ( m_pClassTick )
			m_pClassTick->SetVisible( false );
		if ( m_pCharacterTick )
			m_pCharacterTick->SetVisible( false );

		UpdateReady();

		C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
		ShowImage( m_pChooseHidden, ( pPlayer && pPlayer->GetNoHidden() ) ? "selection/tick" : "selection/cross" );
	}

	virtual void OnThink( void )
	{
		BaseClass::OnThink();

		CHiddenRules *pRules = HiddenRules();
		for ( int i = 0; i < HIDDEN_NUM_CHARACTERS; i++ )
		{
			if ( !m_pCharacters[i] )
				continue;

			// Someone has this one, the player included (Beta 4b made no exception). Beta 4b only
			// dims the first strike, Wakefield's; the others show at full alpha.
			if ( pRules && pRules->IsCharacterTaken( i ) )
			{
				if ( m_pStrikes[i] )
				{
					m_pStrikes[i]->SetVisible( true );
					if ( i == 0 )
						m_pStrikes[i]->SetAlpha( 127 );
				}
				m_pCharacters[i]->SetEnabled( false );
				continue;
			}

			if ( m_pCharacters[i]->IsCursorOver() )
			{
				ShowImage( m_pPortrait, s_Characters[i].pszPortrait );
				ShowImage( m_pInfo, s_Characters[i].pszInfo );
			}

			m_pCharacters[i]->SetEnabled( true );
			if ( m_pStrikes[i] )
				m_pStrikes[i]->SetVisible( false );
		}

		if ( m_pAssault && m_pAssault->IsCursorOver() )
			ShowImage( m_pInfo, "selection/assaultinfo" );
		if ( m_pSupport && m_pSupport->IsCursorOver() )
			ShowImage( m_pInfo, "selection/supportinfo" );

		// The preview shows whoever is hovered, else the picks.
		int iCharacter = m_iCharacter;
		for ( int i = 0; i < HIDDEN_NUM_CHARACTERS; i++ )
		{
			if ( m_pCharacters[i] && m_pCharacters[i]->IsEnabled() && m_pCharacters[i]->IsCursorOver() )
				iCharacter = i;
		}

		int iClass = m_iClass;
		if ( m_pAssault && m_pAssault->IsCursorOver() )
			iClass = HIDDEN_CLASS_ASSAULT;
		else if ( m_pSupport && m_pSupport->IsCursorOver() )
			iClass = HIDDEN_CLASS_SUPPORT;

		ShowPreviewMarine( iCharacter, iClass );
	}

	virtual void OnCommand( const char *command )
	{
		if ( !Q_strnicmp( command, "changeclass ", 12 ) )
		{
			m_iClass = atoi( command + 12 );
			MarkButton( m_pClassTick, m_iClass == 0 ? m_pAssault : m_pSupport );
			UpdateReady();
		}
		else if ( !Q_strnicmp( command, "changemarine ", 13 ) )
		{
			m_iCharacter = atoi( command + 13 );
			if ( m_iCharacter >= 0 && m_iCharacter < HIDDEN_NUM_CHARACTERS )
				MarkButton( m_pCharacterTick, m_pCharacters[m_iCharacter] );
			UpdateReady();
		}
		else if ( !Q_strnicmp( command, "chooseh", 7 ) )
		{
			// The box (CROSS1, on randbutton) is ticked while this player forfeits the Hidden selection.
			C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
			const bool bNoHiddenNow = pPlayer && pPlayer->GetNoHidden();
			engine->ClientCmd( bNoHiddenNow ? "choosehidden 0" : "choosehidden 1" );
			ShowImage( m_pChooseHidden, bNoHiddenNow ? "selection/cross" : "selection/tick" );
		}
		else if ( !Q_stricmp( command, "spectate" ) )
		{
			engine->ClientCmd( "spectate" );
			Close();
		}
		else if ( !Q_stricmp( command, "enter" ) )
		{
			engine->ClientCmd( VarArgs( "changeclass %d", m_iClass ) );
			engine->ClientCmd( VarArgs( "changemarine %d", m_iCharacter ) );
			s_iPickedClass = m_iClass;
			s_iPickedCharacter = m_iCharacter;
			Close();
			m_pViewPort->ShowPanel( PANEL_HIDDEN_WEAPON, true );
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

private:
	// CROSS turns into a tick once both a class and a character are picked. Beta 4b only swaps its
	// image: the .res file hides it, so it's never seen.
	void UpdateReady( void )
	{
		if ( m_pReady )
			m_pReady->SetImage( ( m_iClass >= 0 && m_iCharacter >= 0 ) ? "selection/tick" : "selection/cross" );
	}

	// The tick goes just left of the picked button.
	void MarkButton( ImagePanel *pTick, Button *pButton )
	{
		if ( !pTick || !pButton )
			return;

		int x, y;
		pButton->GetPos( x, y );
		pTick->SetPos( x - pTick->GetWide(), y + ( pButton->GetTall() - pTick->GetTall() ) / 2 );
		pTick->SetVisible( true );
	}

	int m_iClass;
	int m_iCharacter;

	Button *m_pAssault;
	Button *m_pSupport;
	Button *m_pCharacters[HIDDEN_NUM_CHARACTERS];
	ImagePanel *m_pStrikes[HIDDEN_NUM_CHARACTERS];
	ImagePanel *m_pPortrait;
	ImagePanel *m_pInfo;
	ImagePanel *m_pClassTick;
	ImagePanel *m_pCharacterTick;
	ImagePanel *m_pReady;
	ImagePanel *m_pChooseHidden;
};

//-----------------------------------------------------------------------------
// The loadout (weaponmenu): pick a primary, secondary and equipment (ticks at
// Beta 4b's spots); "enter" sends them, 9 (random) for anything not picked,
// and joins.
//-----------------------------------------------------------------------------
static const struct
{
	const char *pszButton;
	const char *pszCommand;
	const char *pszInfo;
	int x, y;
} s_Loadout[] =
{
	{ "riflebutton", "primary 0", "selection/fn2000loadout", 23, 118 },
	{ "smgbutton", "primary 1", "selection/P90Loadout", 23, 140 },
	{ "shotgunbutton", "primary 2", "selection/ShotLoadout", 23, 159 },
	{ "paintbutton", "primary 3", "selection/fn303loadout", 24, 181 },
	{ "pistolbutton1", "secondary 0", "selection/57loadout", 28, 224 },
	{ "pistolbutton2", "secondary 1", "selection/fnp9loadout", 27, 246 },
	{ "equipbutton1", "equip 0", "selection/laminfo", 29, 296 },
	{ "equipbutton2", "equip 1", "selection/flashlightinfo", 31, 316 },
	{ "equipbutton3", "equip 2", "selection/laginfo", 32, 339 },
	{ "equipbutton4", "equip 3", "selection/tripinfo", 34, 360 },
	{ "equipbutton5", "equip 4", "selection/adrenalineinfo", 36, 380 },
};

class CHiddenWeaponMenu : public CHiddenMenu
{
	DECLARE_CLASS_SIMPLE( CHiddenWeaponMenu, CHiddenMenu );

public:
	CHiddenWeaponMenu( IViewPort *pViewPort ) : CHiddenMenu( pViewPort, PANEL_HIDDEN_WEAPON, "team", "Resource/UI/Weaponmenu.res" )
	{
		m_bCenter = true;
		for ( int i = 0; i < ARRAYSIZE( s_Loadout ); i++ )
			m_pButtons[i] = FindControl<Button>( s_Loadout[i].pszButton );

		m_pInfo = FindControl<ImagePanel>( "WeaponInfo" );
		m_pTicks[0] = FindControl<ImagePanel>( "PrimaryTick" );
		m_pTicks[1] = FindControl<ImagePanel>( "SecondaryTick" );
		m_pTicks[2] = FindControl<ImagePanel>( "EquipmentTick" );

		// Crossings-out over the FN2000 (STRIKE1) and the shotgun (STRIKE), which support marines
		// can't carry. They mustn't take the buttons' clicks.
		m_pStrikes[0] = FindControl<ImagePanel>( "STRIKE1" );
		m_pStrikes[1] = FindControl<ImagePanel>( "STRIKE" );
		for ( int i = 0; i < ARRAYSIZE( m_pStrikes ); i++ )
		{
			if ( m_pStrikes[i] )
				m_pStrikes[i]->SetMouseInputEnabled( false );
		}
	}

protected:
	virtual void OnShow( void )
	{
		Q_strncpy( m_szPicks[0], "primary 9", sizeof( m_szPicks[0] ) );
		Q_strncpy( m_szPicks[1], "secondary 9", sizeof( m_szPicks[1] ) );
		Q_strncpy( m_szPicks[2], "equip 9", sizeof( m_szPicks[2] ) );
		for ( int i = 0; i < ARRAYSIZE( m_pTicks ); i++ )
		{
			if ( m_pTicks[i] )
				m_pTicks[i]->SetVisible( false );
		}

		int iClass = s_iPickedClass;
		C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
		if ( iClass < 0 && pPlayer )
			iClass = pPlayer->GetPlayerClass();

		ShowPreviewMarine( s_iPickedCharacter, iClass );

		const bool bSupport = ( iClass == HIDDEN_CLASS_SUPPORT );
		for ( int i = 0; i < ARRAYSIZE( m_pStrikes ); i++ )
		{
			if ( m_pStrikes[i] )
				m_pStrikes[i]->SetVisible( bSupport );
		}
		for ( int i = 0; i < ARRAYSIZE( s_Loadout ); i++ )
		{
			if ( m_pButtons[i] && ( !Q_stricmp( s_Loadout[i].pszCommand, "primary 0" ) || !Q_stricmp( s_Loadout[i].pszCommand, "primary 2" ) ) )
				m_pButtons[i]->SetEnabled( !bSupport );
		}
	}

	virtual void OnThink( void )
	{
		BaseClass::OnThink();

		for ( int i = 0; i < ARRAYSIZE( s_Loadout ); i++ )
		{
			if ( m_pButtons[i] && m_pButtons[i]->IsCursorOver() )
				ShowImage( m_pInfo, s_Loadout[i].pszInfo );
		}
	}

	virtual void OnCommand( const char *command )
	{
		for ( int i = 0; i < ARRAYSIZE( s_Loadout ); i++ )
		{
			if ( Q_stricmp( command, s_Loadout[i].pszCommand ) )
				continue;

			const int iGroup = !Q_strnicmp( command, "primary", 7 ) ? 0 : !Q_strnicmp( command, "secondary", 9 ) ? 1 : 2;
			Q_strncpy( m_szPicks[iGroup], command, sizeof( m_szPicks[iGroup] ) );
			PlaceImage( m_pTicks[iGroup], s_Loadout[i].x, s_Loadout[i].y );
			return;
		}

		if ( !Q_stricmp( command, "enter" ) )
		{
			for ( int i = 0; i < ARRAYSIZE( m_szPicks ); i++ )
				engine->ClientCmd( m_szPicks[i] );
			engine->ClientCmd( "enter" );
			Close();
			return;
		}

		BaseClass::OnCommand( command );
	}

private:
	Button *m_pButtons[ARRAYSIZE( s_Loadout )];
	ImagePanel *m_pInfo;
	ImagePanel *m_pTicks[3];	// primary, secondary, equipment
	ImagePanel *m_pStrikes[2];	// FN2000, shotgun
	char m_szPicks[3][16];
};

//-----------------------------------------------------------------------------
// The marines' radio menu (+radiomenu, while held): click a call.
//-----------------------------------------------------------------------------
class CHiddenRadioMenu : public CHiddenMenu
{
	DECLARE_CLASS_SIMPLE( CHiddenRadioMenu, CHiddenMenu );

public:
	CHiddenRadioMenu( IViewPort *pViewPort ) : CHiddenMenu( pViewPort, PANEL_HIDDEN_RADIO, "radio", "Resource/UI/Radiomenu_marine.res" ) {}

protected:
	virtual void OnCommand( const char *command )
	{
		if ( !Q_strnicmp( command, "radio ", 6 ) )
		{
			engine->ClientCmd( command );
			Close();
			return;
		}

		BaseClass::OnCommand( command );
	}
};

//-----------------------------------------------------------------------------
// Registration and the commands that open them.
//-----------------------------------------------------------------------------
void CreateHiddenViewportPanels( CBaseViewport *pViewport )
{
	pViewport->AddNewPanel( new CHiddenTeamMenu( pViewport ), "CHiddenTeamMenu" );
	pViewport->AddNewPanel( new CHiddenWeaponMenu( pViewport ), "CHiddenWeaponMenu" );
	pViewport->AddNewPanel( new CHiddenRadioMenu( pViewport ), "CHiddenRadioMenu" );
}

CON_COMMAND( classmenu, "Shows the personnel selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_HIDDEN_TEAM, true );
}

// Closing the message of the day runs this (SDK 2013 only lets the server pick from fixed commands).
CON_COMMAND( chooseteam, "Shows the personnel selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_HIDDEN_TEAM, true );
}

CON_COMMAND( weaponmenu, "Shows the weapon selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_HIDDEN_WEAPON, true );
}

static void RadioMenu( bool bShow )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( bShow && ( !pPlayer || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_IRIS ) )
		return;

	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_HIDDEN_RADIO, bShow );
}

static ConCommand startradiomenu( "+radiomenu", []( const CCommand & ) { RadioMenu( true ); }, "Shows the radio menu while held" );
static ConCommand endradiomenu( "-radiomenu", []( const CCommand & ) { RadioMenu( false ); } );
