//========= Hidden: Source =====================================================//
//
// Purpose: Definitions shared by the Hidden client and server.
//
//=============================================================================//

#ifndef HIDDEN_SHAREDDEFS_H
#define HIDDEN_SHAREDDEFS_H
#pragma once

// Teams. The numbers match Beta 4b and HL2MP's TEAM_COMBINE/TEAM_REBELS slots.
enum
{
	TEAM_IRIS = 2,		// the marines
	TEAM_HIDDEN,		// subject 617
};

// Game types, chosen by the map name's prefix (see docs/spec/game-modes.md).
enum HiddenGameType_t
{
	HIDDEN_GAMETYPE_HIDDEN = 1,			// hdn_ and anything else
	HIDDEN_GAMETYPE_OVERRUN,			// ovr_
	HIDDEN_GAMETYPE_MARINE_TUTORIAL,	// mtr_
	HIDDEN_GAMETYPE_HIDDEN_TUTORIAL,	// htr_
};

#define HIDDEN_MAX_MARINES		8	// at most this many marines spawn per round
#define HIDDEN_NUM_CHARACTERS	9	// marine characters, one per player

#endif // HIDDEN_SHAREDDEFS_H
