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

class CHidden_Player : public CHL2MP_Player
{
public:
	DECLARE_CLASS( CHidden_Player, CHL2MP_Player );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CHidden_Player();

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
	virtual CBaseEntity *EntSelectSpawnPoint( void );

	// Class, character and loadout
	int GetPlayerClass( void ) const { return m_iPlayerClass; }
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

	// Put a player who isn't playing this round into observer mode, keeping their team.
	void BecomeObserver( void );

private:
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

	bool m_bReadyToPlay;
	bool m_bSafety;
	bool m_bHadHidden;
};

inline CHidden_Player *ToHiddenPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CHidden_Player *>( pEntity );
}

#endif // HIDDEN_PLAYER_H
