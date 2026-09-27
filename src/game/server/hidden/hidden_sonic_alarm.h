//========= Hidden: Source =====================================================//
//
// Purpose: The sonic alarm (npc_tripmine): HL2's tripmine turned into a reusable
//			alarm that never explodes. See docs/spec/weapons.md.
//
//=============================================================================//

#ifndef HIDDEN_SONIC_ALARM_H
#define HIDDEN_SONIC_ALARM_H
#pragma once

#include "basegrenade_shared.h"

class CBeam;

class CHiddenSonicAlarm : public CBaseGrenade
{
public:
	DECLARE_CLASS( CHiddenSonicAlarm, CBaseGrenade );
	DECLARE_DATADESC();

	CHiddenSonicAlarm();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	void PowerupThink( void );
	void BeamBreakThink( void );
	void AlarmThink( void );

	void SetAlarmOwner( CBaseEntity *pOwner ) { m_hOwner = pOwner; }

	// Where the beam points.
	const Vector &GetBeamDir( void ) const { return m_vecDir; }

private:
	void MakeBeam( void );
	void KillBeam( void );

	EHANDLE m_hOwner;
	EHANDLE m_hBreaker;	// what broke the beam last
	float m_flPowerUp;
	Vector m_vecDir;
	Vector m_vecEnd;
	float m_flBeamLength;
	CBeam *m_pBeam;
};

#endif // HIDDEN_SONIC_ALARM_H
