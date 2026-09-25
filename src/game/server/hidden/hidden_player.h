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
	virtual void PostThink( void );
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
	int GetEquipment( void ) const { return m_iEquipment; }
	int GetCharacter( void ) const { return m_iCharacter; }
	bool HasValidCharacter( void ) const { return m_iCharacter >= 0; }
	void ReleaseCharacter( void );

	// Ready means the player picked a class and character and wants to play.
	bool IsReadyToPlay( void ) const { return m_bReadyToPlay; }
	void SetReadyToPlay( bool bReady ) { m_bReadyToPlay = bReady; }

	// Hidden selection (docs/spec/hidden-selection.md)
	bool GetNoHidden( void ) const { return m_bNoHidden; }
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
	bool IsBoosted( void ) const { return m_bBoosted; }
	int GetBoostCount( void ) const { return m_iBoostCount; }
	void Boost( void );

	// The location_brush the player last touched, shown in chat. Returns true if it changed.
	const char *GetCurrentLocation( void ) const { return m_szCurrentLocation; }
	bool SetCurrentLocation( const char *pszLocation );

	// Put a player who isn't playing this round into observer mode, keeping their team.
	void BecomeObserver( void );

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
	CNetworkVar( int, m_iWeighting );
	CNetworkVar( bool, m_bSafety );
	CNetworkVar( bool, m_bZoom );
	CNetworkVar( int, m_iShotsFired );
	CNetworkVar( bool, m_bStunned );
	CNetworkVar( float, m_flBlur );
	CNetworkVar( bool, m_bLAM );
	CNetworkVar( bool, m_bNightVision );
	CNetworkVar( int, m_iThrowGrenadeCounter );
	CNetworkString( m_szCurrentLocation, HIDDEN_LOCATION_LENGTH );

	IHiddenPlayerAnimState *m_pHiddenAnimState;

	int m_iBoostCount;
	float m_flBoostTimer;
	bool m_bBoosted;

	CUtlVector<StunTracker_t> m_Stunners;
	float m_flStunTime;
	float m_flNextStunUpdate;

	bool m_bReadyToPlay;
	bool m_bHadHidden;
};

inline CHidden_Player *ToHiddenPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CHidden_Player *>( pEntity );
}

#endif // HIDDEN_PLAYER_H
