//========= Hidden: Source =====================================================//
//
// Purpose: Third-person player animation: Beta 4b's CSDKPlayerAnimState, the 2006
//			SDK template's, which plays the CS:S animations the player models include
//			(cs_player_shared.mdl) by the weapon's "PlayerAnimationExtension".
//			See docs/spec/client.md.
//
//=============================================================================//

#ifndef HIDDEN_PLAYERANIMSTATE_H
#define HIDDEN_PLAYERANIMSTATE_H
#pragma once

#include "iplayeranimstate.h"
#include "base_playeranimstate.h"

#ifdef CLIENT_DLL
	class C_BaseAnimatingOverlay;
	class C_WeaponHiddenBase;
	#define CBaseAnimatingOverlay C_BaseAnimatingOverlay
	#define CWeaponHiddenBase C_WeaponHiddenBase
#else
	class CBaseAnimatingOverlay;
	class CWeaponHiddenBase;
#endif

// The template's PlayerAnimEvent_t, renamed: SDK 2013's multiplayer anim state has its own.
enum HiddenPlayerAnimEvent_t
{
	HIDDEN_ANIMEVENT_FIRE_GUN_PRIMARY = 0,
	HIDDEN_ANIMEVENT_FIRE_GUN_SECONDARY,
	HIDDEN_ANIMEVENT_THROW_GRENADE,
	HIDDEN_ANIMEVENT_JUMP,
	HIDDEN_ANIMEVENT_RELOAD,

	HIDDEN_ANIMEVENT_COUNT
};

#define HIDDEN_THROWGRENADE_COUNTER_BITS	3

class IHiddenPlayerAnimState : virtual public IPlayerAnimState
{
public:
	// Called on both client and server to play firing, reloading and grenade throws.
	virtual void DoAnimationEvent( HiddenPlayerAnimEvent_t event ) = 0;

	// True while the grenade prime or throw animation plays.
	virtual bool IsThrowingGrenade( void ) = 0;
};

class IHiddenPlayerAnimStateHelpers
{
public:
	virtual CWeaponHiddenBase *HiddenAnim_GetActiveWeapon( void ) = 0;
	virtual bool HiddenAnim_CanMove( void ) = 0;
	virtual int HiddenAnim_GetThrowGrenadeCounter( void ) = 0;
};

IHiddenPlayerAnimState *CreateHiddenPlayerAnimState( CBaseAnimatingOverlay *pEntity, IHiddenPlayerAnimStateHelpers *pHelpers );

#endif // HIDDEN_PLAYERANIMSTATE_H
