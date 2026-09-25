//========= Hidden: Source =====================================================//
//
// Purpose: Client side of the Hidden player.
//
//=============================================================================//

#ifndef C_HIDDEN_PLAYER_H
#define C_HIDDEN_PLAYER_H
#pragma once

#include "c_hl2mp_player.h"
#include "hidden_shareddefs.h"

class C_Hidden_Player : public C_HL2MP_Player
{
public:
	DECLARE_CLASS( C_Hidden_Player, C_HL2MP_Player );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_Hidden_Player();

	static C_Hidden_Player *GetLocalHiddenPlayer( void );

	int GetPlayerClass( void ) const { return m_iPlayerClass; }
	int GetCharacter( void ) const { return m_iCharacter; }
	int GetPrimary( void ) const { return m_iPrimary; }
	int GetSecondary( void ) const { return m_iSecondary; }
	int GetEquipment( void ) const { return m_iEquipment; }
	bool GetNoHidden( void ) const { return m_bNoHidden; }
	int GetWeighting( void ) const { return m_iWeighting; }

	bool GetSafe( void ) const { return m_bSafety; }
	bool GetZoom( void ) const { return m_bZoom; }
	void SetZoom( bool bZoom ) { m_bZoom = bZoom; }
	int GetShotsFired( void ) const { return m_iShotsFired; }
	void SetShotsFired( int iShots ) { m_iShotsFired = iShots; }
	bool IsStunned( void ) const { return m_bStunned; }
	float GetBlur( void ) const { return m_flBlur; }

private:
	C_Hidden_Player( const C_Hidden_Player & );

	int m_iPlayerClass;
	int m_iCharacter;
	int m_iPrimary;
	int m_iSecondary;
	int m_iEquipment;
	bool m_bNoHidden;
	int m_iWeighting;
	bool m_bSafety;
	bool m_bZoom;
	int m_iShotsFired;
	bool m_bStunned;
	float m_flBlur;
};

inline C_Hidden_Player *ToHiddenPlayer( C_BaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<C_Hidden_Player *>( pEntity );
}

#endif // C_HIDDEN_PLAYER_H
