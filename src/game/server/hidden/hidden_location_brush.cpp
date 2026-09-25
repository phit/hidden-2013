//========= Hidden: Source =====================================================//
//
// Purpose: location_brush, a trigger naming the area a player is in. The name
//			shows in their chat and on the HUD. See docs/spec/map-entities.md.
//
//=============================================================================//

#include "cbase.h"
#include "basetoggle.h"
#include "hidden_player.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CLocationBrush : public CBaseToggle
{
public:
	DECLARE_CLASS( CLocationBrush, CBaseToggle );
	DECLARE_DATADESC();

	virtual void Spawn( void );

	void BrushTouch( CBaseEntity *pOther );

private:
	string_t m_iszLocation;
	bool m_bLocationChange;	// whether the last touch changed the player's location
};

LINK_ENTITY_TO_CLASS( location_brush, CLocationBrush );

BEGIN_DATADESC( CLocationBrush )
	DEFINE_KEYFIELD( m_iszLocation, FIELD_STRING, "location" ),
	DEFINE_FIELD( m_bLocationChange, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( BrushTouch ),
END_DATADESC()

void CLocationBrush::Spawn( void )
{
	m_bLocationChange = true;
	SetTouch( &CLocationBrush::BrushTouch );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( GetParent() ? SOLID_VPHYSICS : SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );
	AddEffects( EF_NODRAW );
	SetModel( STRING( GetModelName() ) );
}

void CLocationBrush::BrushTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() || !pOther->IsAlive() )
		return;

	CHidden_Player *pPlayer = ToHiddenPlayer( pOther );
	if ( !pPlayer )
		return;

	const char *pszLocation = STRING( m_iszLocation );
	m_bLocationChange = pPlayer->SetCurrentLocation( pszLocation );
	if ( !m_bLocationChange )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( "player_location" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetString( "location", pszLocation );
		gameeventmanager->FireEvent( event );
	}
}
