//========= Hidden: Source =====================================================//
//
// Purpose: The main menu's "Tutorials" window: Beta 4b's CMyPanel, the SDK
//			wiki's example panel put to use. The menu button runs
//			"engine ToggleMyPanel"; the window (resource/UI/MyPanel.res) shows
//			both sides' portraits with Hidden and IRIS buttons that load
//			htr_tutorial and mtr_tutorial. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "ienginevgui.h"
#include "filesystem.h"
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define HIDDEN_TUTORIAL_RES	"resource/UI/MyPanel.res"

static ConVar cl_showmypanel( "cl_showmypanel", "0", FCVAR_CLIENTDLL, "Sets the state of myPanel <state>" );

CON_COMMAND( ToggleMyPanel, "Toggles myPanel on or off" )
{
	cl_showmypanel.SetValue( !cl_showmypanel.GetBool() );
}

class CHiddenTutorialPanel : public Frame
{
	DECLARE_CLASS_SIMPLE( CHiddenTutorialPanel, Frame );

public:
	CHiddenTutorialPanel( VPANEL parent ) : BaseClass( NULL, "MyPanel" ), m_iResX( 225 ), m_iResY( 75 ), m_iResWide( 300 ), m_iResTall( 300 )
	{
		SetParent( parent );
		SetKeyBoardInputEnabled( true );
		SetMouseInputEnabled( true );
		SetMinimizeButtonVisible( false );
		SetMaximizeButtonVisible( false );
		SetCloseButtonVisible( false );
		SetSizeable( false );
		SetMoveable( false );
		SetVisible( false );
		SetScheme( scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );

		// Deviation: proportional, so it keeps Beta 4b's size relative to the screen at any resolution
		// (Beta 4b's was a fixed 300 pixels). As with our other menus, the frame takes its own .res
		// entry first; the build group only lays out the children.
		SetProportional( true );
		KeyValues *pResource = new KeyValues( HIDDEN_TUTORIAL_RES );
		if ( pResource->LoadFromFile( g_pFullFileSystem, HIDDEN_TUTORIAL_RES, "GAME" ) )
		{
			KeyValues *pFrame = pResource->FindKey( "MyPanel" );
			if ( pFrame )
			{
				m_iResX = pFrame->GetInt( "xpos", m_iResX );
				m_iResY = pFrame->GetInt( "ypos", m_iResY );
				m_iResWide = pFrame->GetInt( "wide", m_iResWide );
				m_iResTall = pFrame->GetInt( "tall", m_iResTall );
			}
		}
		pResource->deleteThis();

		ApplyResBounds();
		LoadControlSettings( HIDDEN_TUTORIAL_RES );

		ivgui()->AddTickSignal( GetVPanel(), 100 );
	}

	// Shown while cl_showmypanel is set, as in Beta 4b.
	virtual void OnTick( void )
	{
		BaseClass::OnTick();

		const bool bShow = cl_showmypanel.GetBool();
		if ( bShow != IsVisible() )
		{
			if ( bShow )
				Activate();
			else
				SetVisible( false );
		}
	}

	virtual void OnCommand( const char *pszCommand )
	{
		if ( !Q_stricmp( pszCommand, "loadmarinetut" ) )
			engine->ClientCmd_Unrestricted( "map mtr_tutorial\n" );
		else if ( !Q_stricmp( pszCommand, "loadhiddentut" ) )
			engine->ClientCmd_Unrestricted( "map htr_tutorial\n" );
		else if ( Q_stricmp( pszCommand, "turnoff" ) )
		{
			BaseClass::OnCommand( pszCommand );
			return;
		}

		cl_showmypanel.SetValue( 0 );
	}

protected:
	virtual void PerformLayout( void )
	{
		ApplyResBounds();
		BaseClass::PerformLayout();
	}

private:
	void ApplyResBounds( void )
	{
		HScheme hScheme = GetScheme();
		SetBounds( scheme()->GetProportionalScaledValueEx( hScheme, m_iResX ), scheme()->GetProportionalScaledValueEx( hScheme, m_iResY ),
			scheme()->GetProportionalScaledValueEx( hScheme, m_iResWide ), scheme()->GetProportionalScaledValueEx( hScheme, m_iResTall ) );
	}

	int m_iResX, m_iResY, m_iResWide, m_iResTall;	// the frame's own .res entry, in 640x480 units
};

// Made once the client is up, on GameUI's panel so it shows over the main menu.
class CHiddenTutorialPanelSystem : public CAutoGameSystem
{
public:
	CHiddenTutorialPanelSystem() : CAutoGameSystem( "CHiddenTutorialPanelSystem" ), m_pPanel( NULL ) {}

	virtual void PostInit( void ) { m_pPanel = new CHiddenTutorialPanel( enginevgui->GetPanel( PANEL_GAMEUIDLL ) ); }
	virtual void Shutdown( void )
	{
		if ( m_pPanel )
			m_pPanel->MarkForDeletion();
		m_pPanel = NULL;
	}

private:
	CHiddenTutorialPanel *m_pPanel;
};

static CHiddenTutorialPanelSystem g_HiddenTutorialPanelSystem;
