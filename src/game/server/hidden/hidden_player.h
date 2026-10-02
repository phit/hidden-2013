//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden player: marine class and character, loadout, team setup
//			and Hidden selection state. See docs/spec/teams-classes.md.
//
//=============================================================================//

#ifndef HIDDEN_PLAYER_H
#define HIDDEN_PLAYER_H
#pragma once

#include "hl2mp_player.h"
#include "hidden_shareddefs.h"
#include "hidden_playeranimstate.h"

class CHidden_Player : public CHL2MP_Player, public IHiddenPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( CHidden_Player, CHL2MP_Player );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CHidden_Player();
	~CHidden_Player();

	static CHidden_Player *CreatePlayer( const char *className, edict_t *ed )
	{
		CHL2MP_Player::s_PlayerEdict = ed;
		return static_cast<CHidden_Player *>( CreateEntityByName( className ) );
	}

	virtual void Precache( void );
	virtual void InitialSpawn( void );
	virtual void Spawn( void );
	virtual void ChangeTeam( int iTeam ) OVERRIDE;
	virtual bool ClientCommand( const CCommand &args );
	virtual void PlayerDeathThink( void );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void CreateRagdollEntity( void );

	// Spectating (docs/spec/game-rules.md): the map's info_spectator cameras (Beta 4b's observer
	// mode 1, OBS_MODE_FIXED here) or the living marines' helmet cams (mode 2, OBS_MODE_IN_EYE).
	virtual bool SetObserverMode( int mode );
	virtual bool SetObserverTarget( CBaseEntity *target );
	virtual CBaseEntity *FindNextObserverTarget( bool bReverse );
	virtual bool IsValidObserverTarget( CBaseEntity *target );

	// Only the Hidden picks things up: corpses (+use, on release) and light props.
	virtual bool IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );
	virtual void PlayerUse( void );

	// hidden_player_shared.cpp
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void GetStepSoundVelocities( float *velwalk, float *velrun );
	float UpdateMaxSpeed( int nButtons, int nOldButtons );
	virtual void PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize = true );
	// The pickup controller let go of pObject (its collision group is back already).
	void OnObjectReleased( CBaseEntity *pObject );

	// radio <n>: voice calls and taunts (docs/spec/teams-classes.md). Marines who call for ammo can
	// be resupplied by a support marine's +use.
	bool Radio( int iMessage );
	bool IsRequestingAmmo( void ) const { return m_bRequestAmmo; }
	void ClearBlur( void ) { m_flBlur = 0.0f; }
	void GiveRequestedAmmo( CHidden_Player *pGiver );
	virtual void PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper );
	virtual void PreThink( void );
	virtual bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual void PostThink( void );
	virtual void ItemPostFrame( void );
	virtual void ImpulseCommands( void );

	// Third-person animation (Beta 4b's SDK template anim state, played on the clients).
	virtual void SetAnimation( PLAYER_ANIM playerAnim );
	void DoAnimationEvent( HiddenPlayerAnimEvent_t event );
	virtual CWeaponHiddenBase *HiddenAnim_GetActiveWeapon( void );
	virtual bool HiddenAnim_CanMove( void ) { return true; }
	virtual int HiddenAnim_GetThrowGrenadeCounter( void ) { return m_iThrowGrenadeCounter; }
	virtual CBaseEntity *EntSelectSpawnPoint( void );

	// Only marines have a flashlight, and it's silent (equipment 1).
	virtual int FlashlightIsOn( void );
	virtual void FlashlightTurnOn( void );
	virtual void FlashlightTurnOff( void );

	// Class, character and loadout
	int GetPlayerClass( void ) const { return m_iPlayerClass; }
	void SetPlayerClass( int iClass ) { m_iPlayerClass = iClass; }	// takes effect at the next spawn
	int GetPrimary( void ) const { return m_iPrimary; }
	void SetPrimary( int iPrimary ) { m_iPrimary = iPrimary; }
	int GetSecondary( void ) const { return m_iSecondary; }
	void SetSecondary( int iSecondary ) { m_iSecondary = iSecondary; }
	int GetEquipment( void ) const { return m_iEquipment; }
	int GetCharacter( void ) const { return m_iCharacter; }
	bool HasValidCharacter( void ) const { return m_iCharacter >= 0; }
	void ReleaseCharacter( void );

	// Ready means the player picked a class and character and wants to play.
	bool IsReadyToPlay( void ) const { return m_bReadyToPlay; }
	void SetReadyToPlay( bool bReady ) { m_bReadyToPlay = bReady; }

	// Hidden selection (docs/spec/hidden-selection.md)
	bool GetNoHidden( void ) const { return m_bNoHidden; }

	// Set by an admin (hdn_spec_unrestricted): may also watch the Hidden and use the chase and free cameras.
	bool IsSpecUnrestricted( void ) const { return m_bSpecUnrestricted; }
	void SetSpecUnrestricted( bool bUnrestricted ) { m_bSpecUnrestricted = bUnrestricted; }
	bool GetHadHidden( void ) const { return m_bHadHidden; }
	void SetHadHidden( bool bHad ) { m_bHadHidden = bHad; }
	int GetWeighting( void ) const { return m_iWeighting; }
	void AddWeighting( int iAmount );

	// Safety is on between rounds: no damage, weapons lowered.
	bool GetSafe( void ) const { return m_bSafety; }
	void SetSafe( bool bSafe ) { m_bSafety = bSafe; }

	// Weapon state shared with the client (docs/spec/weapons.md)
	bool GetZoom( void ) const { return m_bZoom; }
	void SetZoom( bool bZoom ) { m_bZoom = bZoom; }
	int GetShotsFired( void ) const { return m_iShotsFired; }
	void SetShotsFired( int iShots ) { m_iShotsFired = iShots; }

	// Equipment (docs/spec/hidden-abilities.md): the laser sight, night vision and the boost.
	bool LaserIsOn( void ) const { return m_bLAM; }
	void LaserTurnOn( void ) { m_bLAM = true; }
	void LaserTurnOff( void ) { m_bLAM = false; }
	bool NightVisionEnabled( void ) const { return m_bNightVision; }
	void SetNightVision( bool bEnabled ) { m_bNightVision = bEnabled; }
	bool IsBoosted( void ) const { return m_flBoostEnd > 0.0f; }
	int GetBoostCount( void ) const { return m_iBoostCount; }
	void Boost( void );

	// The location_brush the player last touched, shown in chat. Returns true if it changed.
	const char *GetCurrentLocation( void ) const { return m_szCurrentLocation; }
	bool SetCurrentLocation( const char *pszLocation );

	// Put a player who isn't playing this round into observer mode, keeping their team.
	void BecomeObserver( void );

	// OverRun respawns (docs/spec/game-modes.md): the round timer value at which the player comes
	// back, and whether they're queued at all (only with more than 9 s of the round left by then).
	void SetSpawnTimer( int iRoundRemain, int iDelay ) { m_iSpawnTime = iRoundRemain - iDelay; m_bSpawnQueued = ( m_iSpawnTime > 9 ); }
	int GetSpawnTime( void ) const { return m_iSpawnTime; }
	bool IsSpawnQueued( void ) const { return m_bSpawnQueued; }
	void SetSpawnQueued( bool bQueued ) { m_bSpawnQueued = bQueued; }

	// Stamina (the Hidden's; marines have none). Changes that would leave 0-100 are dropped whole,
	// as in Beta 4b, so regeneration stops just short of 100.
	float GetStamina( void ) const { return m_flStamina; }
	void SetStamina( float flDelta ) { const float flNew = m_flStamina + flDelta; if ( flNew >= 0.0f && flNew <= HIDDEN_STAMINA_MAX ) m_flStamina = flNew; }

	// The aura (vision key, +aura): on while held, drains stamina (docs/spec/hidden-abilities.md).
	bool IsViewing( void ) const { return m_bAura; }
	void SetAura( bool bAura ) { m_bAura = bAura; }

	// Shown as another model by the Visibility plugin (hsm_vis); the player sees the helmet cam overlay.
	bool IsRevealed( void ) const { return m_bRevealed; }
	void SetRevealed( bool bRevealed ) { m_bRevealed = bRevealed; }

	// Wall cling, Beta 4b's movetype 12 (see hidden_gamemovement.cpp).
	bool IsClinging( void ) const { return m_bClinging; }
	void SetClinging( bool bClinging ) { m_bClinging = bClinging; }

	// +walk: marines 120, the Hidden 160; letting go restores the run speed.
	void StartWalking( void );
	void StopWalking( void );

	// Stun and blur (docs/spec/hidden-abilities.md): FN303 hits stun, explosions and the boost
	// send a shockwave. Only the Hidden takes stun damage; anyone's view blurs.
	bool IsStunned( void ) const { return m_bStunned; }
	void Stun( CBasePlayer *pStunner );
	void Shockwave( float flAmount, float flDuration );

private:
	struct StunTracker_t
	{
		EHANDLE hStunner;
		float flExpires;
	};

	void UpdateStun( void );
	void ResetStun( void );

	bool TakeCharacter( int iCharacter );
	void SetupMarine( void );
	void SetupHidden( void );
	void GiveMarineLoadout( void );
	void GiveHiddenLoadout( void );

	CNetworkVar( int, m_iPlayerClass );
	CNetworkVar( int, m_iCharacter );
	CNetworkVar( int, m_iPrimary );
	CNetworkVar( int, m_iSecondary );
	CNetworkVar( int, m_iEquipment );
	CNetworkVar( bool, m_bNoHidden );
	bool m_bSpecUnrestricted;
	CNetworkVar( int, m_iWeighting );
	CNetworkVar( bool, m_bSafety );
	CNetworkVar( bool, m_bZoom );
	CNetworkVar( int, m_iShotsFired );
	CNetworkVar( bool, m_bStunned );
	CNetworkVar( float, m_flBlur );
	CNetworkVar( bool, m_bLAM );
	CNetworkVar( bool, m_bNightVision );
	CNetworkVar( float, m_flStamina );
	CNetworkVar( bool, m_bClinging );
	CNetworkVar( bool, m_bAura );
	CNetworkVar( bool, m_bRequestAmmo );
	CNetworkVar( bool, m_bRevealed );
	CNetworkVar( int, m_iSpawnCount );	// counts spawns, so the client can tell the loadout from pickups
	CNetworkVar( int, m_iSpeedMode );	// HIDDEN_SPEED_*: run, walk or boost, whichever came last
	CNetworkVar( bool, m_bWalking );
	CNetworkVar( float, m_flBoostEnd );	// 0 when not boosted
	CNetworkVar( int, m_iThrowGrenadeCounter );
	CNetworkString( m_szCurrentLocation, HIDDEN_LOCATION_LENGTH );

	IHiddenPlayerAnimState *m_pHiddenAnimState;

	int m_iBoostCount;
	CTakeDamageInfo m_KillInfo;	// for the corpse
	bool m_bUseDroppedObject;	// this +use press let go of what we held

	// Objects let go of inside us, which we own (so neither collides with the other) until we're clear.
	struct ReleasedObject_t
	{
		EHANDLE hObject;
		EHANDLE hOldOwner;
	};
	CUtlVector<ReleasedObject_t> m_ReleasedObjects;
	bool IsInsideObject( CBaseEntity *pObject );
	void UpdateReleasedObjects( void );
	bool m_bAmmoReceived;		// resupplied this life; no more ammo calls
	CUtlVector<EHANDLE> m_Cameras;	// the map's info_spectator cameras, in map order
	float m_flRadioTimer;		// no radio before this

	CUtlVector<StunTracker_t> m_Stunners;
	float m_flStunTime;
	float m_flNextStunUpdate;

	bool m_bReadyToPlay;
	int m_iSpawnTime;
	bool m_bSpawnQueued;
	bool m_bHadHidden;
};

// The server console or rcon, or a listen server's host; tells anyone else they can't.
bool HiddenIsCommandIssuedByServerAdmin( const char *pszCommand );

inline CHidden_Player *ToHiddenPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CHidden_Player *>( pEntity );
}

#endif // HIDDEN_PLAYER_H
