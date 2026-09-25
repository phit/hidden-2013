//========= Hidden: Source =====================================================//
//
// Purpose: info_spectator, a fixed spectator camera placed by the mapper
//			(see docs/spec/map-entities.md).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_spectator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIDDEN_SPECTATOR_MODEL "models/generic/gen_model_spec_camera.mdl"

LINK_ENTITY_TO_CLASS( info_spectator, CHiddenSpectatorPoint );

IMPLEMENT_NETWORKCLASS_ALIASED( HiddenSpectatorPoint, DT_HiddenSpectatorPoint )

BEGIN_NETWORK_TABLE( CHiddenSpectatorPoint, DT_HiddenSpectatorPoint )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flFOV ) ),
	RecvPropString( RECVINFO( m_szLocation ) ),
#else
	SendPropFloat( SENDINFO( m_flFOV ), 0, SPROP_NOSCALE ),
	SendPropString( SENDINFO( m_szLocation ) ),
#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
BEGIN_DATADESC( CHiddenSpectatorPoint )
	DEFINE_KEYFIELD( m_flFOV, FIELD_FLOAT, "fov" ),
	DEFINE_KEYFIELD( m_iszLocation, FIELD_STRING, "location" ),
END_DATADESC()

CHiddenSpectatorPoint::CHiddenSpectatorPoint()
{
	m_flFOV = 90.0f;
}

void CHiddenSpectatorPoint::Precache( void )
{
	PrecacheModel( HIDDEN_SPECTATOR_MODEL );
}

void CHiddenSpectatorPoint::Spawn( void )
{
	Precache();
	SetModel( HIDDEN_SPECTATOR_MODEL );
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	Q_strncpy( m_szLocation.GetForModify(), STRING( m_iszLocation ), HIDDEN_LOCATION_LENGTH );
}

int CHiddenSpectatorPoint::UpdateTransmitState( void )
{
	// Spectators need every camera, wherever they are.
	return SetTransmitState( FL_EDICT_ALWAYS );
}
#endif
