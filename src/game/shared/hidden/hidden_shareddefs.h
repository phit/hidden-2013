//========= Hidden: Source =====================================================//
//
// Purpose: Definitions shared by the Hidden client and server.
//
//=============================================================================//

#ifndef HIDDEN_SHAREDDEFS_H
#define HIDDEN_SHAREDDEFS_H
#pragma once

#include "hl2_shareddefs.h"

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

// Collision groups (docs/spec/map-entities.md#marine_clip). Beta 4b put marines in PLAYER_MOVEMENT
// and the Hidden in NPC_ACTOR; marine_clip had the first mod group, which is taken by HL2's here.
enum
{
	HIDDEN_COLLISION_GROUP_MARINE = COLLISION_GROUP_PLAYER_MOVEMENT,
	HIDDEN_COLLISION_GROUP_HIDDEN = COLLISION_GROUP_NPC_ACTOR,
	HIDDEN_COLLISION_GROUP_MARINE_CLIP = HL2COLLISION_GROUP_COMBINE_BALL_NPC + 1,
};

#define HIDDEN_LOCATION_LENGTH	25	// Beta 4b's location names are at most this long

#define HIDDEN_MAX_MARINES		8	// at most this many marines spawn per round

// Movement and stamina (docs/spec/hidden-abilities.md). The stamina rates are per tick.
#define HIDDEN_STAMINA_MAX			100.0f
#define HIDDEN_STAMINA_REGEN		0.3f	// on the ground
#define HIDDEN_POUNCE_STAMINA		20.0f	// needed and used by a pounce
#define HIDDEN_POUNCE_SPEED			615.0f	// along the aim
#define HIDDEN_CLING_JUMP_STAMINA	10.0f	// per tick jump is held on a wall
#define HIDDEN_AURA_STAMINA			0.1f	// per tick while the aura is on; it goes off below 1
#define HIDDEN_AURA_MAX_SPEED		0.5f	// the aura only shows anything this still
#define HIDDEN_CLING_DISTANCE		24.0f	// from the eyes to the wall
#define HIDDEN_BACK_SPEED_SCALE		0.8f	// marines walking backwards

// Max speeds (Beta 4b's CSDKPlayer), picked each command by CHidden_Player::UpdateMaxSpeed.
#define HIDDEN_MARINE_SPEED			180.0f
#define HIDDEN_HIDDEN_SPEED			220.0f
#define HIDDEN_MARINE_WALK_SPEED	120.0f
#define HIDDEN_HIDDEN_WALK_SPEED	160.0f
#define HIDDEN_BOOST_SPEED			250.0f	// marines' boost equipment
#define HIDDEN_BOOST_TIME			10.0f

enum
{
	HIDDEN_SPEED_RUN = 0,
	HIDDEN_SPEED_WALK,
	HIDDEN_SPEED_BOOST,
};
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

// The Hidden's stamina changes per tick. The Fake Tickrate plugin (hsm_fr_tick) scales them to
// flow as at another tickrate; 1 when it's off.
float HiddenStaminaTickScale( void );

#endif // HIDDEN_SHAREDDEFS_H
