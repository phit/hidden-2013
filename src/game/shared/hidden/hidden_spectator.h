//========= Hidden: Source =====================================================//
//
// Purpose: info_spectator, a fixed spectator camera placed by the mapper
//			(see docs/spec/map-entities.md).
//
//=============================================================================//

#ifndef HIDDEN_SPECTATOR_H
#define HIDDEN_SPECTATOR_H
#pragma once

#include "hidden_shareddefs.h"

#ifdef CLIENT_DLL
	#define CHiddenSpectatorPoint C_HiddenSpectatorPoint
#endif

class CHiddenSpectatorPoint : public CBaseEntity
{
public:
	DECLARE_CLASS( CHiddenSpectatorPoint, CBaseEntity );
	DECLARE_NETWORKCLASS();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	CHiddenSpectatorPoint();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual int UpdateTransmitState( void );
#endif

	float GetFOV( void ) const { return m_flFOV; }
	const char *GetLocation( void ) const { return m_szLocation; }

private:
	CNetworkVar( float, m_flFOV );
	CNetworkString( m_szLocation, HIDDEN_LOCATION_LENGTH );

#ifndef CLIENT_DLL
	string_t m_iszLocation;	// keyvalue, copied into m_szLocation
#endif
};

#endif // HIDDEN_SPECTATOR_H
