//========= Hidden: Source =====================================================//
//
// Purpose: Picks the Hidden each round (docs/spec/hidden-selection.md).
//
//=============================================================================//

#ifndef HIDDEN_SELECTOR_H
#define HIDDEN_SELECTOR_H
#pragma once

#include "hidden_shareddefs.h"

class CHidden_Player;

class CHiddenSelector
{
public:
	CHiddenSelector();

	// Returns the player to be the Hidden this round, or NULL if there aren't enough players.
	CHidden_Player *SelectHidden( HiddenGameType_t nGameType, int iHiddenRounds );

	// A marine killed the Hidden: in classic mode the killer is next.
	void NewHidden( CHidden_Player *pKiller );
	// The player loses their place as the carried-over Hidden (death by the world, suicide, leaving).
	void ForfeitHidden( CHidden_Player *pPlayer );
	void ClearHidden( void );
	CHidden_Player *GetCurrentHidden( void ) const { return m_hCurrentHidden.Get(); }

private:
	void FillPlayerVectors( void );
	CHidden_Player *SetSelectHidden( CUtlVector<CHidden_Player *> &players );
	CHidden_Player *SelectWeightedHidden( CUtlVector<CHidden_Player *> &players );

	CUtlVector<CHidden_Player *> m_PrefPlayers;		// living marines who haven't opted out
	CUtlVector<CHidden_Player *> m_ReadyPlayers;	// everyone on a team
	CUtlVector<CHidden_Player *> m_AllPlayers;		// every connected player

	CHandle<CHidden_Player> m_hCurrentHidden;
	int m_iHiddenRounds;
};

#endif // HIDDEN_SELECTOR_H
