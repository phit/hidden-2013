//========= Hidden: Source =====================================================//
//
// Purpose: The laser sight's dot (Beta 4b's CLaserDot, HL2MP's RPG dot with its own
//			sprite). See docs/spec/hidden-abilities.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_laserdot.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#include "beamdraw.h"
	#include "view.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIDDEN_LASERDOT_SPRITE	"sprites/laserglow1"

IMPLEMENT_NETWORKCLASS_ALIASED( HiddenLaserDot, DT_HiddenLaserDot )

// Nothing is sent: each client traces the owner's aim itself.
BEGIN_NETWORK_TABLE( CHiddenLaserDot, DT_HiddenLaserDot )
END_NETWORK_TABLE()

// Beta 4b's classname was env_laserdot, which HL2MP's RPG still registers here.
LINK_ENTITY_TO_CLASS( hidden_laserdot, CHiddenLaserDot );

#ifndef CLIENT_DLL

BEGIN_DATADESC( CHiddenLaserDot )
	DEFINE_FIELD( m_vecSurfaceNormal, FIELD_VECTOR ),
END_DATADESC()

CHiddenLaserDot *CHiddenLaserDot::Create( const Vector &vecOrigin, CBaseEntity *pOwner )
{
	CHiddenLaserDot *pDot = static_cast<CHiddenLaserDot *>( CBaseEntity::Create( "hidden_laserdot", vecOrigin, vec3_angle ) );
	if ( !pDot )
		return NULL;

	pDot->SetMoveType( MOVETYPE_NONE );
	pDot->AddSolidFlags( FSOLID_NOT_SOLID );
	pDot->AddEffects( EF_NOSHADOW );
	UTIL_SetSize( pDot, -Vector( 4, 4, 4 ), Vector( 4, 4, 4 ) );
	pDot->SetOwnerEntity( pOwner );
	pDot->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	return pDot;
}

void CHiddenLaserDot::SetLaserPosition( const Vector &vecOrigin, const Vector &vecNormal )
{
	SetAbsOrigin( vecOrigin );
	m_vecSurfaceNormal = vecNormal;
}

#else

void CHiddenLaserDot::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
		m_hSpriteMaterial.Init( HIDDEN_LASERDOT_SPRITE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

int CHiddenLaserDot::DrawModel( int flags )
{
	// Unlike HL2MP's dot, Beta 4b's isn't drawn without a living owner.
	C_HL2MP_Player *pOwner = ToHL2MPPlayer( GetOwnerEntity() );
	if ( !pOwner )
		return 0;

	Vector vecEnd;
	if ( pOwner->IsDormant() )
	{
		vecEnd = GetAbsOrigin();
	}
	else
	{
		if ( !pOwner->IsAlive() )
			return 0;

		// Trace the owner's aim here rather than trusting the networked position.
		Vector vecSrc, vecDir;
		if ( pOwner->IsLocalPlayer() )
		{
			vecSrc = CurrentViewOrigin();
			vecDir = CurrentViewForward();
		}
		else
		{
			vecSrc = pOwner->EyePosition();
			AngleVectors( pOwner->GetAnimEyeAngles(), &vecDir );
		}

		trace_t tr;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );

		// Just off the surface; HL2MP backs off 4 units.
		vecEnd = tr.endpos + tr.plane.normal * 1.25f;
	}

	// Flutter a little.
	const float flScale = 16.0f + random->RandomFloat( -4.0f, 4.0f );
	color32 color = { 255, 255, 255, 255 };

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( m_hSpriteMaterial, this );
	DrawSprite( vecEnd, flScale, flScale, color );

	return 1;
}

#endif
