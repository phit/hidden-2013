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

#define HIDDEN_LOCATION_LENGTH	25	// Beta 4b's location names are at most this long

#define HIDDEN_MAX_MARINES		8	// at most this many marines spawn per round
#define HIDDEN_NUM_CHARACTERS	9	// marine characters, one per player

// Marine classes (changeclass <n>).
enum
{
	HIDDEN_CLASS_ASSAULT = 0,
	HIDDEN_CLASS_SUPPORT,
	HIDDEN_CLASS_COUNT,

	HIDDEN_CLASS_NONE = -1,
};

// Loadout choices, as sent by the weapon menu (primary/secondary/equip <n>).
enum
{
	HIDDEN_PRIMARY_FN2000 = 0,
	HIDDEN_PRIMARY_P90,
	HIDDEN_PRIMARY_SHOTGUN,
	HIDDEN_PRIMARY_FN303,
	HIDDEN_PRIMARY_COUNT,

	HIDDEN_SECONDARY_PISTOL = 0,
	HIDDEN_SECONDARY_PISTOL2,
	HIDDEN_SECONDARY_COUNT,

	HIDDEN_EQUIPMENT_LASER = 0,
	HIDDEN_EQUIPMENT_FLASHLIGHT,
	HIDDEN_EQUIPMENT_NIGHTVISION,
	HIDDEN_EQUIPMENT_SONIC,
	HIDDEN_EQUIPMENT_BOOST,
	HIDDEN_EQUIPMENT_COUNT,

	HIDDEN_LOADOUT_NONE = -1,
	HIDDEN_LOADOUT_RANDOM = 9,
};

#endif // HIDDEN_SHAREDDEFS_H
