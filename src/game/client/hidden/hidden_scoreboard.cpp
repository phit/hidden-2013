//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's scoreboard: the stock dialog on a vgui/hud/hdn_scoreboard
//			background, with fixed IRIS, Hidden and spectator sections, a points
//			column, and a "KiA" status only the dead can see. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "clientscoreboarddialog.h"
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/SectionedListPanel.h>
#include "igameresources.h"
#include "voice_status.h"
#include "hidden_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Column widths at 640x480.
#define HIDDEN_SB_NAME_WIDTH_IRIS	145		// the IRIS section also has a status column
#define HIDDEN_SB_NAME_WIDTH		205
#define HIDDEN_SB_STATUS_WIDTH		60
#define HIDDEN_SB_POINTS_WIDTH		70
#define HIDDEN_SB_DEATHS_WIDTH		65
#define HIDDEN_SB_PING_WIDTH		75

// Beta 4b's voice icons, by its speaker status.
enum HiddenVoiceStatus_t
{
	HIDDEN_VOICE_NEVERSPOKEN,
	HIDDEN_VOICE_NOTTALKING,
	HIDDEN_VOICE_TALKING,
	HIDDEN_VOICE_BANNED,

	HIDDEN_VOICE_COUNT
};

static const char *s_pszVoiceImages[HIDDEN_VOICE_COUNT] =
{
	"gfx/vgui/640_speaker1",
	"gfx/vgui/640_speaker2",
	"gfx/vgui/640_speaker3",
	"gfx/vgui/640_voiceblocked",
};

//-----------------------------------------------------------------------------
// SDK 2013's CVoiceStatus keeps who has ever spoken to itself, so remember it
// here for the "not talking" icon, every frame whether the scoreboard is up or not.
//-----------------------------------------------------------------------------
class CHiddenVoiceHistory : public CAutoGameSystemPerFrame
{
public:
	CHiddenVoiceHistory() : CAutoGameSystemPerFrame( "CHiddenVoiceHistory" ) {}

	virtual void LevelInitPreEntity() { m_Spoken.ClearAll(); }

	virtual void Update( float frametime )
	{
		CVoiceStatus *pVoice = GetClientVoiceMgr();
		if ( !pVoice )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients && i <= m_Spoken.GetNumBits(); i++ )
		{
			if ( pVoice->IsPlayerSpeaking( i ) )
				m_Spoken.Set( i - 1 );
		}
	}

	bool HasSpoken( int iPlayerIndex ) const
	{
		return iPlayerIndex >= 1 && iPlayerIndex <= m_Spoken.GetNumBits() && m_Spoken.IsBitSet( iPlayerIndex - 1 );
	}

private:
	CPlayerBitVec m_Spoken;
};

static CHiddenVoiceHistory s_VoiceHistory;

static int GetSpeakerStatus( int iPlayerIndex )
{
	CVoiceStatus *pVoice = GetClientVoiceMgr();
	if ( !pVoice )
		return HIDDEN_VOICE_NEVERSPOKEN;

	if ( pVoice->IsPlayerBlocked( iPlayerIndex ) )
		return HIDDEN_VOICE_BANNED;
	if ( pVoice->IsPlayerSpeaking( iPlayerIndex ) )
		return HIDDEN_VOICE_TALKING;
	if ( s_VoiceHistory.HasSpoken( iPlayerIndex ) )
		return HIDDEN_VOICE_NOTTALKING;
	return HIDDEN_VOICE_NEVERSPOKEN;
}

//-----------------------------------------------------------------------------
// The scoreboard
//-----------------------------------------------------------------------------
class CHiddenScoreBoardDialog : public CClientScoreBoardDialog
{
	DECLARE_CLASS_SIMPLE( CHiddenScoreBoardDialog, CClientScoreBoardDialog );

public:
	CHiddenScoreBoardDialog( IViewPort *pViewPort );

	virtual void FireGameEvent( IGameEvent *event );

	// Beta 4b showed no avatars.
	virtual bool ShowAvatars() { return false; }

protected:
	virtual bool GetPlayerScoreInfo( int playerIndex, KeyValues *outPlayerInfo );
	virtual void InitScoreboardSections();
	virtual void UpdatePlayerInfo();
	virtual void PostApplySchemeSettings( IScheme *pScheme );

	static bool StaticPlayerSortFunc( SectionedListPanel *list, int itemID1, int itemID2 );

private:
	void AddTeamSection( int iSection, int iNameWidth, bool bStatus );

	int m_iVoiceImages[HIDDEN_VOICE_COUNT];
};

CHiddenScoreBoardDialog::CHiddenScoreBoardDialog( IViewPort *pViewPort ) : CClientScoreBoardDialog( pViewPort )
{
	memset( m_iVoiceImages, 0, sizeof( m_iVoiceImages ) );

	// Beta 4b asked for a scheme that doesn't exist, which gets the default one.
	SetScheme( "ScoreScheme" );

	// The background covers the whole dialog, under the list.
	ImagePanel *pBackground = dynamic_cast<ImagePanel *>( FindChildByName( "SelectionBG" ) );
	if ( pBackground )
	{
		pBackground->SetSize( GetWide(), GetTall() );
		pBackground->SetZPos( 0 );
	}
	m_pPlayerList->SetZPos( 1 );

	// The scoreboard updates on these even while it's closed.
	ListenForGameEvent( "game_round_restart" );
	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_death" );
}

void CHiddenScoreBoardDialog::FireGameEvent( IGameEvent *event )
{
	const char *pszType = event->GetName();

	if ( !Q_strcmp( pszType, "server_spawn" ) )
	{
		// Beta 4b read engine->GetLevelName(), which isn't set yet when the event arrives here.
		char szMapName[MAX_MAP_NAME];
		Q_FileBase( event->GetString( "mapname" ), szMapName, sizeof( szMapName ) );

		Panel *pMapName = FindChildByName( "MapName" );
		if ( pMapName )
		{
			PostMessage( pMapName, new KeyValues( "SetText", "text", szMapName ) );
			pMapName->MoveToFront();
		}
	}
	else if ( !Q_strcmp( pszType, "game_round_restart" ) || !Q_strcmp( pszType, "player_connect" ) ||
		!Q_strcmp( pszType, "player_death" ) )
	{
		Update();
	}

	BaseClass::FireGameEvent( event );
}

void CHiddenScoreBoardDialog::PostApplySchemeSettings( IScheme *pScheme )
{
	// Added before the base class scales the image list to the resolution.
	for ( int i = 0; i < HIDDEN_VOICE_COUNT; i++ )
		m_iVoiceImages[i] = m_pImageList->AddImage( scheme()->GetImage( s_pszVoiceImages[i], true ) );

	BaseClass::PostApplySchemeSettings( pScheme );
}

void CHiddenScoreBoardDialog::AddTeamSection( int iSection, int iNameWidth, bool bStatus )
{
	HScheme hScheme = GetScheme();

	m_iSectionId = iSection;
	m_pPlayerList->AddSection( m_iSectionId, "", StaticPlayerSortFunc );
	m_pPlayerList->SetSectionAlwaysVisible( m_iSectionId );

	m_pPlayerList->AddColumnToSection( m_iSectionId, "name", "", 0, scheme()->GetProportionalScaledValueEx( hScheme, iNameWidth ) );
	if ( bStatus )
		m_pPlayerList->AddColumnToSection( m_iSectionId, "status", "", 0, scheme()->GetProportionalScaledValueEx( hScheme, HIDDEN_SB_STATUS_WIDTH ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "points", "", 0, scheme()->GetProportionalScaledValueEx( hScheme, HIDDEN_SB_POINTS_WIDTH ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", "", 0, scheme()->GetProportionalScaledValueEx( hScheme, HIDDEN_SB_DEATHS_WIDTH ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", "", 0, scheme()->GetProportionalScaledValueEx( hScheme, HIDDEN_SB_PING_WIDTH ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "voice", "", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, 0 );
}

// The background draws the headings, so the sections have none: an empty
// header section, then the IRIS, the Hidden and the spectators, always shown.
void CHiddenScoreBoardDialog::InitScoreboardSections()
{
	m_iSectionId = 0;
	m_pPlayerList->AddSection( m_iSectionId, "", StaticPlayerSortFunc );

	AddTeamSection( TEAM_IRIS, HIDDEN_SB_NAME_WIDTH_IRIS, true );
	AddTeamSection( TEAM_HIDDEN, HIDDEN_SB_NAME_WIDTH, false );
	AddTeamSection( TEAM_SPECTATOR, HIDDEN_SB_NAME_WIDTH, false );
}

bool CHiddenScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, KeyValues *kv )
{
	IGameResources *gr = GameResources();
	if ( !gr )
		return false;

	kv->SetInt( "deaths", gr->GetDeaths( playerIndex ) );
	kv->SetInt( "points", gr->GetPlayerScore( playerIndex ) );	// SDK 2013's GetFrags is a stub (666)
	kv->SetInt( "ping", gr->GetPing( playerIndex ) );
	kv->SetString( "name", gr->GetPlayerName( playerIndex ) );
	kv->SetInt( "playerIndex", playerIndex );
	kv->SetInt( "voice", m_iVoiceImages[GetSpeakerStatus( playerIndex )] );

	// Only spectators and the dead see who's been killed.
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && ( pLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR || !pLocalPlayer->IsAlive() ) )
	{
		if ( !gr->IsAlive( playerIndex ) || gr->GetTeam( playerIndex ) == TEAM_SPECTATOR )
			kv->SetString( "status", "KiA" );
	}

	return true;
}

void CHiddenScoreBoardDialog::UpdatePlayerInfo()
{
	m_iSectionId = 0;
	int selectedRow = -1;

	IGameResources *gr = GameResources();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( gr && gr->IsConnected( i ) )
		{
			KeyValues *playerData = new KeyValues( "data" );
			GetPlayerScoreInfo( i, playerData );

			char szName[MAX_PLAYER_NAME_LENGTH];
			UTIL_MakeSafeName( playerData->GetString( "name", "" ), szName, sizeof( szName ) );
			playerData->SetString( "name", szName );

			int itemID = FindItemIDForPlayerIndex( i );
			int sectionID = gr->GetTeam( i );

			if ( gr->IsLocalPlayer( i ) )
				selectedRow = itemID;

			if ( itemID == -1 )
				itemID = m_pPlayerList->AddItem( sectionID, playerData );
			else
				m_pPlayerList->ModifyItem( itemID, sectionID, playerData );

			// The local player in blue, the Hidden in red, the IRIS in black, spectators in green.
			if ( gr->IsLocalPlayer( i ) )
				m_pPlayerList->SetItemFgColor( itemID, Color( 20, 20, 200, 160 ) );
			else if ( sectionID == TEAM_HIDDEN )
				m_pPlayerList->SetItemFgColor( itemID, Color( 255, 50, 50, 160 ) );
			else if ( sectionID == TEAM_IRIS )
				m_pPlayerList->SetItemFgColor( itemID, Color( 0, 0, 0, 160 ) );
			else if ( sectionID == TEAM_SPECTATOR )
				m_pPlayerList->SetItemFgColor( itemID, Color( 52, 173, 32, 200 ) );

			playerData->deleteThis();
		}
		else
		{
			int itemID = FindItemIDForPlayerIndex( i );
			if ( itemID != -1 )
				m_pPlayerList->RemoveItem( itemID );
		}
	}

	if ( selectedRow != -1 )
		m_pPlayerList->SetSelectedItem( selectedRow );
}

// Most points first, then fewest deaths.
bool CHiddenScoreBoardDialog::StaticPlayerSortFunc( SectionedListPanel *list, int itemID1, int itemID2 )
{
	KeyValues *it1 = list->GetItemData( itemID1 );
	KeyValues *it2 = list->GetItemData( itemID2 );
	Assert( it1 && it2 );

	int v1 = it1->GetInt( "points" );
	int v2 = it2->GetInt( "points" );
	if ( v1 != v2 )
		return v1 > v2;

	v1 = it1->GetInt( "deaths" );
	v2 = it2->GetInt( "deaths" );
	if ( v1 != v2 )
		return v1 < v2;

	return itemID1 < itemID2;
}

IViewPortPanel *CreateHiddenScoreBoard( IViewPort *pViewPort )
{
	return new CHiddenScoreBoardDialog( pViewPort );
}
