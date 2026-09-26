//========= Hidden: Source =====================================================//
//
// Purpose: Hides controls in Beta 4b's GameUI dialog layouts that SDK 2013's
//			GameUI no longer fills or handles, so they'd show up empty or dead.
//			The layouts are Beta 4b's own files, mounted from the player's
//			install, so this finds the controls at runtime instead of patching
//			them.
//
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "ienginevgui.h"
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

	virtual void OnTick( void )
	{
		VPANEL root = enginevgui->GetPanel( PANEL_GAMEUIDLL );
		if ( root )
			HideControls( root );
	}

private:
	void HideControls( VPANEL panel )
	{
		const char *pszName = ipanel()->GetName( panel );
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

		// Only open dialogs matter.
		for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
		{
			VPANEL child = ipanel()->GetChild( panel, i );
			if ( ipanel()->IsVisible( child ) )
				HideControls( child );
		}
	}
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
