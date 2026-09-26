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
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

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

// SDK 2013 options worth keeping, placed in free space on Beta 4b's tabs (the tabs' own units,
// like their layout files). Every other control a Beta 4b layout doesn't place is hidden.
// Entries with a heading token are labels of ours, for controls whose headings SDK 2013's own
// layouts supply: Beta 4b style, "< Heading >".
struct HiddenGameUIPlacement_t
{
	const char *pszDialog;
	const char *pszControl;
	int x, y, wide, tall;
	const char *pszHeadingToken;
};

static const HiddenGameUIPlacement_t s_PlacedControls[] =
{
	// Mouse: raw input under the divider, acceleration as a row like sensitivity's, below the gamepad.
	{ "OptionsSubMouse", "MouseRaw", 28, 164, 148, 28 },
	{ "OptionsSubMouse", "MouseAccelerationCheckbox", 28, 276, 148, 28 },
	{ "OptionsSubMouse", "MouseAccelerationSlider", 182, 272, 186, 40 },
	{ "OptionsSubMouse", "MouseAccelerationLabel", 382, 276, 40, 24 },
	// Audio: muting when unfocused, in the hidden commentary combo's place.
	{ "OptionsSubAudio", "snd_mute_losefocus", 36, 194, 175, 24 },
	// Video: the third-party licence notices, left of Advanced.
	{ "OptionsSubVideo", "ThirdPartyVideoCredits", 36, 208, 180, 24 },
	// Multiplayer: which custom files to download from servers, under Advanced.
	{ "OptionsSubMultiplayer", "DownloadFilterCheck", 40, 275, 220, 24 },
	// Video, Advanced: motion blur and multicore rendering beside HDR, as a row like the others.
	{ "OptionsSubVideoAdvancedDlg", "HiddenMotionBlurLabel", 176, 234, 152, 24, "#GameUI_MotionBlur" },
	{ "OptionsSubVideoAdvancedDlg", "MotionBlur", 176, 258, 132, 24 },
	{ "OptionsSubVideoAdvancedDlg", "HiddenMulticoreLabel", 330, 234, 152, 24, "#GameUI_MulticoreRendering" },
	{ "OptionsSubVideoAdvancedDlg", "Multicore", 330, 258, 132, 24 },
};

static const HiddenGameUIPlacement_t *FindPlacement( const char *pszDialog, const char *pszControl )
{
	for ( int i = 0; i < ARRAYSIZE( s_PlacedControls ); i++ )
	{
		if ( !Q_stricmp( pszDialog, s_PlacedControls[i].pszDialog ) && !Q_stricmp( pszControl, s_PlacedControls[i].pszControl ) )
			return &s_PlacedControls[i];
	}
	return NULL;
}

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

	// Lists, for every options tab (open or not), the controls its layout file doesn't place.
	void DumpUnplaced( VPANEL panel )
	{
		const char *pszClass = ipanel()->GetClassName( panel );
		const char *pszName = ( pszClass && pszClass[0] == 'C' ) ? pszClass + 1 : "";
		if ( !Q_strnicmp( pszName, "OptionsSub", 10 ) )
		{
			KeyValues *pLayout = GetLayout( pszName );
			Msg( "%s (%s):\n", pszName, pLayout ? "Beta 4b layout" : "SDK 2013 layout, all placed" );
			for ( int j = 0; pLayout && j < ipanel()->GetChildCount( panel ); j++ )
			{
				VPANEL child = ipanel()->GetChild( panel, j );
				const char *pszChild = ipanel()->GetName( child );
				if ( pszChild[0] && !pLayout->FindKey( pszChild ) )
					Msg( "  %s (%s): %s\n", pszChild, ipanel()->GetClassName( child ), FindPlacement( pszName, pszChild ) ? "placed" : "hidden" );
			}
		}

		for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
			DumpUnplaced( ipanel()->GetChild( panel, i ) );
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

		// An options tab laid out by Beta 4b: place the controls we keep, hide the rest it doesn't mention.
		if ( !Q_strnicmp( pszName, "OptionsSub", 10 ) )
		{
			KeyValues *pLayout = GetLayout( pszName );
			if ( pLayout )
				AddHeadings( panel, pszName );
			for ( int j = 0; pLayout && j < ipanel()->GetChildCount( panel ); j++ )
			{
				VPANEL child = ipanel()->GetChild( panel, j );
				const char *pszChild = ipanel()->GetName( child );
				if ( !pszChild[0] || pLayout->FindKey( pszChild ) )
					continue;

				const HiddenGameUIPlacement_t *pPlace = FindPlacement( pszName, pszChild );
				if ( pPlace )
				{
					// In the tab's units: scaled with the screen, like its layout, when it's proportional.
					const bool bProportional = ipanel()->IsProportional( panel );
					const HScheme hScheme = ipanel()->GetScheme( panel );
					#define HIDDEN_SCALE( v ) ( bProportional ? scheme()->GetProportionalScaledValueEx( hScheme, v ) : v )
					const int px = HIDDEN_SCALE( pPlace->x ), py = HIDDEN_SCALE( pPlace->y );
					const int pw = HIDDEN_SCALE( pPlace->wide ), pt = HIDDEN_SCALE( pPlace->tall );
					#undef HIDDEN_SCALE

					int x, y, w, t;
					ipanel()->GetPos( child, x, y );
					ipanel()->GetSize( child, w, t );
					if ( x != px || y != py )
						ipanel()->SetPos( child, px, py );
					if ( w != pw || t != pt )
						ipanel()->SetSize( child, pw, pt );
				}
				else if ( ipanel()->IsVisible( child ) )
				{
					ipanel()->SetVisible( child, false );
				}
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

	// Our headings for placed controls, added once to each instance of the dialog.
	void AddHeadings( VPANEL panel, const char *pszDialog )
	{
		for ( int i = 0; i < ARRAYSIZE( s_PlacedControls ); i++ )
		{
			const HiddenGameUIPlacement_t &place = s_PlacedControls[i];
			if ( !place.pszHeadingToken || Q_stricmp( place.pszDialog, pszDialog ) || FindChild( panel, place.pszControl ) )
				continue;

			wchar_t wszHeading[128];
			const wchar_t *pwszText = g_pVGuiLocalize->Find( place.pszHeadingToken );
			V_snwprintf( wszHeading, ARRAYSIZE( wszHeading ), L"< %ls >", pwszText ? pwszText : L"" );

			Label *pLabel = new Label( NULL, place.pszControl, wszHeading );
			pLabel->SetParent( panel );
			pLabel->SetScheme( ipanel()->GetScheme( panel ) );
			pLabel->SetProportional( ipanel()->IsProportional( panel ) );
		}
	}

	static VPANEL FindChild( VPANEL panel, const char *pszName )
	{
		for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
		{
			VPANEL child = ipanel()->GetChild( panel, i );
			if ( !Q_stricmp( ipanel()->GetName( child ), pszName ) )
				return child;
		}
		return 0;
	}

	// Beta 4b's layout file for a dialog (resource/<name>.res), or NULL if the dialog uses SDK
	// 2013's own, which places all of its controls.
	KeyValues *GetLayout( const char *pszDialog )
	{
		int i = m_Layouts.Find( pszDialog );
		if ( i == m_Layouts.InvalidIndex() )
		{
			const char *pszFile = VarArgs( "resource/%s.res", pszDialog );
			// Beta 4b's are loose files outside SDK Base; SDK 2013's are under it, or in its VPKs.
			char szFull[MAX_PATH], szBase[MAX_PATH];
			const bool bBeta4b = g_pFullFileSystem->RelativePathToFullPath( pszFile, "GAME", szFull, sizeof( szFull ) ) &&
				!( g_pFullFileSystem->GetSearchPath( "BASE_PATH", false, szBase, sizeof( szBase ) ) &&
				   !V_strnicmp( szFull, szBase, V_strcspn( szBase, ";" ) ) );

			KeyValues *pLayout = new KeyValues( pszDialog );
			if ( !bBeta4b || !pLayout->LoadFromFile( g_pFullFileSystem, pszFile, "GAME" ) )
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

	virtual void PostInit( void )
	{
		m_pFixups = new CHiddenGameUIFixups();

		// Beta 4b's resource/gameui_<language>.txt shadows SDK 2013's, which has the strings for
		// everything GameUI gained since (raw input, the download filter, ...). Add SDK 2013's from
		// SDK Base's hl2 folder, then Beta 4b's again so its wording wins where both have a string.
		char szBase[MAX_PATH], szLanguage[64];
		if ( !g_pFullFileSystem->GetSearchPath( "BASE_PATH", false, szBase, sizeof( szBase ) ) )
			return;
		szBase[V_strcspn( szBase, ";" )] = 0;
		engine->GetUILanguage( szLanguage, sizeof( szLanguage ) );

		char szFile[MAX_PATH];
		V_snprintf( szFile, sizeof( szFile ), "%shl2/resource/gameui_%s.txt", szBase, szLanguage );
		V_FixSlashes( szFile );
		if ( !g_pFullFileSystem->FileExists( szFile ) )
		{
			V_snprintf( szFile, sizeof( szFile ), "%shl2/resource/gameui_english.txt", szBase );
			V_FixSlashes( szFile );
		}
		if ( g_pVGuiLocalize->AddFile( szFile ) )
			g_pVGuiLocalize->AddFile( "resource/gameui_%language%.txt", "GAME", true );
	}
	virtual void Shutdown( void )
	{
		if ( m_pFixups )
			m_pFixups->MarkForDeletion();
		m_pFixups = NULL;
	}

	CHiddenGameUIFixups *GetFixups( void ) { return m_pFixups; }

private:
	CHiddenGameUIFixups *m_pFixups;
};

static CHiddenGameUISystem g_HiddenGameUISystem;

CON_COMMAND( hdn_gameui_unplaced, "List the controls on each options tab that its Beta 4b layout doesn't place (open the options once first)" )
{
	VPANEL root = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	if ( root && g_HiddenGameUISystem.GetFixups() )
		g_HiddenGameUISystem.GetFixups()->DumpUnplaced( root );
}
