//========= Hidden: Source =====================================================//
//
// Purpose: marine_clip, a brush that only stops marines. The rule itself lives in
//			PassServerEntityFilter and CHiddenRules::ShouldCollide, shared, and the clip is sent
//			to clients (C_MarineClip) so their movement prediction runs into it too.
//			See docs/spec/map-entities.md.
//
//=============================================================================//

#include "cbase.h"
#include "basetoggle.h"
#include "hidden_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CMarineClip : public CBaseToggle
{
public:
	DECLARE_CLASS( CMarineClip, CBaseToggle );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void Spawn( void );

	// EF_NODRAW would keep it off clients; prediction needs it everywhere (deviation: Beta 4b
	// didn't send it, so marines rubber-banded against it).
	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }

	void BrushTouch( CBaseEntity *pOther ) {}
};

LINK_ENTITY_TO_CLASS( marine_clip, CMarineClip );

BEGIN_DATADESC( CMarineClip )
	DEFINE_FUNCTION( BrushTouch ),
END_DATADESC()

// The model, solidity and collision group come with the base entity's table.
IMPLEMENT_SERVERCLASS_ST( CMarineClip, DT_MarineClip )
END_SEND_TABLE()

void CMarineClip::Spawn( void )
{
	SetTouch( &CMarineClip::BrushTouch );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BSP );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( HIDDEN_COLLISION_GROUP_MARINE_CLIP );
	SetModel( STRING( GetModelName() ) );
}
