//========= Hidden: Source =====================================================//
//
// Purpose: marine_clip, a brush that only stops marines. The rule itself lives in
//			PassServerEntityFilter and CHiddenRules::ShouldCollide.
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

	virtual void Spawn( void );

	void BrushTouch( CBaseEntity *pOther ) {}
};

LINK_ENTITY_TO_CLASS( marine_clip, CMarineClip );

BEGIN_DATADESC( CMarineClip )
	DEFINE_FUNCTION( BrushTouch ),
END_DATADESC()

void CMarineClip::Spawn( void )
{
	SetTouch( &CMarineClip::BrushTouch );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BSP );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( HIDDEN_COLLISION_GROUP_MARINE_CLIP );
	SetModel( STRING( GetModelName() ) );
}
