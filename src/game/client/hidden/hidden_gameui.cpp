//========= Hidden: Source =====================================================//
//
// Purpose: Tidies GameUI dialogs laid out by Beta 4b's files, which are
//			mounted from the player's install, so this works at runtime instead
//			of patching them: hides controls SDK 2013's GameUI no longer fills
//			or handles, and the ones it added to the options tabs that Beta 4b's
//			layouts never place (they'd pile up in the top left corner).
//
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "ienginevgui.h"
#include "filesystem.h"
#include "tier1/utldict.h"
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

struct HiddenGameUIControl_t
{
	const char *pszDialog;
	const char *pszControl;
};

static const HiddenGameUIControl_t s_HiddenControls[] =
{
	// SDK 2013 fills the commentary combo only for games with commentary.
	{ "OptionsSubAudio", "Commentary" },
	{ "OptionsSubAudio", "CommentaryLabel" },
	// The 2006 player list had an add-friend button; SDK 2013's has no such control or string.
	{ "PlayerListDialog", "AddFriendButton" },
};

class CHiddenGameUIFixups : public Panel
{
	DECLARE_CLASS_SIMPLE( CHiddenGameUIFixups, Panel );

public:
	CHiddenGameUIFixups() : BaseClass( NULL, "HiddenGameUIFixups" )
	{
		SetVisible( false );
		ivgui()->AddTickSignal( GetVPanel(), 250 );
	}

	virtual ~CHiddenGameUIFixups()
	{
		for ( int i = m_Layouts.First(); i != m_Layouts.InvalidIndex(); i = m_Layouts.Next( i ) )
		{
			if ( m_Layouts[i] )
				m_Layouts[i]->deleteThis();
		}
	}

	virtual void OnTick( void )
	{
		VPANEL root = enginevgui->GetPanel( PANEL_GAMEUIDLL );
		if ( root )
			HideControls( root );
	}

private:
	void HideControls( VPANEL panel )
	{
		// GameUI leaves most dialogs unnamed; their class (COptionsSubMouse) names their layout file.
		const char *pszClass = ipanel()->GetClassName( panel );
		const char *pszName = ( pszClass && pszClass[0] == 'C' ) ? pszClass + 1 : "";
		for ( int i = 0; i < ARRAYSIZE( s_HiddenControls ); i++ )
		{
			if ( Q_stricmp( pszName, s_HiddenControls[i].pszDialog ) )
				continue;

			for ( int j = 0; j < ipanel()->GetChildCount( panel ); j++ )
			{
				VPANEL child = ipanel()->GetChild( panel, j );
				if ( !Q_stricmp( ipanel()->GetName( child ), s_HiddenControls[i].pszControl ) && ipanel()->IsVisible( child ) )
					ipanel()->SetVisible( child, false );
			}
		}

		// An options tab: hide the controls its layout file doesn't mention.
		if ( !Q_strnicmp( pszName, "OptionsSub", 10 ) )
		{
			KeyValues *pLayout = GetLayout( pszName );
			for ( int j = 0; pLayout && j < ipanel()->GetChildCount( panel ); j++ )
			{
				VPANEL child = ipanel()->GetChild( panel, j );
				const char *pszChild = ipanel()->GetName( child );
				if ( pszChild[0] && !pLayout->FindKey( pszChild ) && ipanel()->IsVisible( child ) )
					ipanel()->SetVisible( child, false );
			}
		}

		// Only open dialogs matter.
		for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
		{
			VPANEL child = ipanel()->GetChild( panel, i );
			if ( ipanel()->IsVisible( child ) )
				HideControls( child );
		}
	}

	// The layout file GameUI loaded for a dialog (resource/<name>.res), or NULL if there's none.
	KeyValues *GetLayout( const char *pszDialog )
	{
		int i = m_Layouts.Find( pszDialog );
		if ( i == m_Layouts.InvalidIndex() )
		{
			KeyValues *pLayout = new KeyValues( pszDialog );
			if ( !pLayout->LoadFromFile( g_pFullFileSystem, VarArgs( "resource/%s.res", pszDialog ), "GAME" ) )
			{
				pLayout->deleteThis();
				pLayout = NULL;
			}
			i = m_Layouts.Insert( pszDialog, pLayout );
		}
		return m_Layouts[i];
	}

	CUtlDict< KeyValues *, int > m_Layouts;
};

class CHiddenGameUISystem : public CAutoGameSystem
{
public:
	CHiddenGameUISystem() : CAutoGameSystem( "CHiddenGameUISystem" ), m_pFixups( NULL ) {}

	virtual void PostInit( void ) { m_pFixups = new CHiddenGameUIFixups(); }
	virtual void Shutdown( void )
	{
		if ( m_pFixups )
			m_pFixups->MarkForDeletion();
		m_pFixups = NULL;
	}

private:
	CHiddenGameUIFixups *m_pFixups;
};

static CHiddenGameUISystem g_HiddenGameUISystem;
