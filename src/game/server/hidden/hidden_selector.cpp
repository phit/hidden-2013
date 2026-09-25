//========= Hidden: Source =====================================================//
//
// Purpose: Picks the Hidden each round (docs/spec/hidden-selection.md).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_selector.h"
#include "hidden_player.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{
	SELECT_WEIGHTED = 0,
	SELECT_CLASSIC,
	SELECT_RANDOM,
};

CHiddenSelector::CHiddenSelector()
{
	m_iHiddenRounds = 0;
}

void CHiddenSelector::FillPlayerVectors( void )
{
	m_PrefPlayers.RemoveAll();
	m_ReadyPlayers.RemoveAll();
	m_AllPlayers.RemoveAll();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;

		m_AllPlayers.AddToTail( pPlayer );

		const int iTeam = pPlayer->GetTeamNumber();
		if ( iTeam != TEAM_IRIS && iTeam != TEAM_HIDDEN )
			continue;

		m_ReadyPlayers.AddToTail( pPlayer );

		if ( iTeam == TEAM_IRIS && pPlayer->IsAlive() && !pPlayer->GetNoHidden() )
			m_PrefPlayers.AddToTail( pPlayer );
	}
}

CHidden_Player *CHiddenSelector::SelectHidden( HiddenGameType_t nGameType, int iHiddenRounds )
{
	FillPlayerVectors();

	int iMinPlayers = 2;
	const char *pszMode = "Hidden";
	if ( nGameType == HIDDEN_GAMETYPE_OVERRUN )
	{
		iMinPlayers = 3;
		pszMode = "OverRun";
	}
	else if ( nGameType == HIDDEN_GAMETYPE_MARINE_TUTORIAL || nGameType == HIDDEN_GAMETYPE_HIDDEN_TUTORIAL )
	{
		iMinPlayers = 1;
		pszMode = "Tutorial";
	}

	if ( m_ReadyPlayers.Count() < iMinPlayers )
	{
		ClearHidden();

		for ( int i = 0; i < m_AllPlayers.Count(); i++ )
			m_AllPlayers[i]->BecomeObserver();

		const bool bConnected = ( m_AllPlayers.Count() < iMinPlayers );
		const int iNeeded = iMinPlayers - ( bConnected ? m_AllPlayers.Count() : m_ReadyPlayers.Count() );

		char szMessage[128];
		Q_snprintf( szMessage, sizeof( szMessage ), "%s Game : Needs %d more %s player(s) to start",
			pszMode, iNeeded, bConnected ? "connected" : "ready" );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, szMessage );
		return NULL;
	}

	CHidden_Player *pCurrent = m_hCurrentHidden.Get();
	if ( pCurrent && !m_ReadyPlayers.HasElement( pCurrent ) )
		pCurrent = NULL;

	const int iMethod = hdn_selectmethod.GetInt();
	if ( iMethod == SELECT_RANDOM )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Selected Randomly\n" );
		pCurrent = NULL;
	}
	else if ( pCurrent && pCurrent->GetNoHidden() )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Forfeited Selection\n" );
		pCurrent = NULL;
	}
	else if ( pCurrent && iHiddenRounds > 0 && m_iHiddenRounds >= iHiddenRounds )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Round Limit Reached\n" );
		pCurrent = NULL;
	}

	if ( pCurrent )
	{
		m_iHiddenRounds++;
		m_hCurrentHidden = pCurrent;
		pCurrent->SetHadHidden( true );
		return pCurrent;
	}

	CHidden_Player *pHidden = NULL;
	switch ( iMethod )
	{
	case SELECT_CLASSIC:
		pHidden = SetSelectHidden( m_PrefPlayers.Count() ? m_PrefPlayers : m_ReadyPlayers );
		break;
	case SELECT_RANDOM:
		pHidden = SetSelectHidden( m_ReadyPlayers );
		break;
	default:
		pHidden = SelectWeightedHidden( m_ReadyPlayers );
		if ( !pHidden )
			pHidden = SetSelectHidden( m_ReadyPlayers );
		break;
	}

	m_hCurrentHidden = pHidden;
	m_iHiddenRounds = pHidden ? 1 : 0;
	if ( pHidden )
		pHidden->SetHadHidden( true );

	return pHidden;
}

CHidden_Player *CHiddenSelector::SetSelectHidden( CUtlVector<CHidden_Player *> &players )
{
	// Prefer willing players who aren't the Hidden, then the Hidden team, then players who opted out.
	CUtlVector<CHidden_Player *> willing, hidden, optedOut;
	for ( int i = 0; i < players.Count(); i++ )
	{
		CHidden_Player *pPlayer = players[i];
		if ( pPlayer->GetNoHidden() )
			optedOut.AddToTail( pPlayer );
		else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
			hidden.AddToTail( pPlayer );
		else
			willing.AddToTail( pPlayer );
	}

	CUtlVector<CHidden_Player *> &group = willing.Count() ? willing : ( hidden.Count() ? hidden : optedOut );
	if ( !group.Count() )
		return NULL;

	// Rotation: everyone in the group gets a turn before anyone goes twice.
	CUtlVector<CHidden_Player *> candidates;
	for ( int i = 0; i < group.Count(); i++ )
	{
		if ( !group[i]->GetHadHidden() )
			candidates.AddToTail( group[i] );
	}

	if ( !candidates.Count() )
	{
		for ( int i = 0; i < group.Count(); i++ )
		{
			group[i]->SetHadHidden( false );
			candidates.AddToTail( group[i] );
		}
	}

	return candidates[random->RandomInt( 0, candidates.Count() - 1 )];
}

CHidden_Player *CHiddenSelector::SelectWeightedHidden( CUtlVector<CHidden_Player *> &players )
{
	// Each eligible player holds a ticket range as wide as their weighting.
	int iTotal = 0;
	CUtlVector<CHidden_Player *> eligible;
	for ( int i = 0; i < players.Count(); i++ )
	{
		CHidden_Player *pPlayer = players[i];
		if ( pPlayer->GetWeighting() > 0 && pPlayer->GetTeamNumber() != TEAM_HIDDEN && !pPlayer->GetNoHidden() )
		{
			eligible.AddToTail( pPlayer );
			iTotal += pPlayer->GetWeighting();
		}
	}

	if ( !iTotal )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "No player is eligible for weighted selection.\n" );
		return NULL;
	}

	char szMessage[128];
	for ( int i = 0; i < eligible.Count(); i++ )
	{
		Q_snprintf( szMessage, sizeof( szMessage ), "%s has a %.2f percent chance of weighted selection\n",
			eligible[i]->GetPlayerName(), 100.0f * eligible[i]->GetWeighting() / iTotal );
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, szMessage );
	}

	int iTicket = random->RandomInt( 0, iTotal - 1 );
	for ( int i = 0; i < eligible.Count(); i++ )
	{
		iTicket -= eligible[i]->GetWeighting();
		if ( iTicket < 0 )
			return eligible[i];
	}

	return eligible.Tail();
}

void CHiddenSelector::NewHidden( CHidden_Player *pKiller )
{
	ClearHidden();

	if ( pKiller && hdn_selectmethod.GetInt() == SELECT_CLASSIC && !pKiller->GetNoHidden() )
		m_hCurrentHidden = pKiller;
}

void CHiddenSelector::ForfeitHidden( CHidden_Player *pPlayer )
{
	if ( pPlayer && pPlayer == m_hCurrentHidden.Get() )
		ClearHidden();
}

void CHiddenSelector::ClearHidden( void )
{
	m_hCurrentHidden = NULL;
	m_iHiddenRounds = 0;
}
