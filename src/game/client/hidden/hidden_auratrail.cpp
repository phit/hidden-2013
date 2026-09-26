//========= Hidden: Source =====================================================//
//
// Purpose: The aura trails the Hidden sees on marines. Beta 4b attached a
//			sprites/aura entity particle trail to every player and changed the
//			trail renderer; the particles here are the stock trail's. See
//			docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_auratrail.h"
#include "c_hidden_player.h"
#include "particle_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define AURA_MATERIAL		"sprites/aura"
#define AURA_RATE			150		// particles per second, as the stock trail
#define AURA_LIFETIME		0.75f
#define AURA_START_SIZE		14
#define AURA_END_SIZE		0		// 0.5, stored as a byte by the stock trail
#define AURA_START_ALPHA	64

CSmartPtr<CHiddenAuraEmitter> CHiddenAuraEmitter::Create( C_BasePlayer *pOwner )
{
	CHiddenAuraEmitter *pRet = new CHiddenAuraEmitter( pOwner );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

CHiddenAuraEmitter::CHiddenAuraEmitter( C_BasePlayer *pOwner ) : CSimpleEmitter( "hidden_aura" )
{
	m_hOwner = pOwner;
	m_hMaterial = GetPMaterial( AURA_MATERIAL );
	m_teParticleSpawn.Init( AURA_RATE );
}

void CHiddenAuraEmitter::Emit( float flTimeDelta )
{
	C_BasePlayer *pOwner = m_hOwner;
	if ( !pOwner )
		return;

	SetSortOrigin( pOwner->WorldSpaceCenter() );

	// Somewhere in a random hitbox, or the collision box without a model.
	matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pOwner->GetModel() );
	mstudiohitboxset_t *pSet = pStudioHdr ? pStudioHdr->pHitboxSet( pOwner->GetHitboxSet() ) : NULL;
	if ( pSet && pSet->numhitboxes > 0 && pOwner->HitboxToWorldTransforms( hitboxbones ) )
	{
		while ( m_teParticleSpawn.NextEvent( flTimeDelta ) )
		{
			mstudiobbox_t *pBox = pSet->pHitbox( random->RandomInt( 0, pSet->numhitboxes - 1 ) );
			AddParticle( flTimeDelta, pBox->bbmin, pBox->bbmax, *hitboxbones[pBox->bone] );
		}
		return;
	}

	while ( m_teParticleSpawn.NextEvent( flTimeDelta ) )
		AddParticle( flTimeDelta, pOwner->CollisionProp()->OBBMins(), pOwner->CollisionProp()->OBBMaxs(), pOwner->EntityToWorldTransform() );
}

void CHiddenAuraEmitter::AddParticle( float flInitialDeltaTime, const Vector &vecMins, const Vector &vecMaxs, const matrix3x4_t &boxToWorld )
{
	Vector vecLocalPosition, vecWorldPosition;
	vecLocalPosition.x = Lerp( random->RandomFloat( 0.0f, 1.0f ), vecMins.x, vecMaxs.x );
	vecLocalPosition.y = Lerp( random->RandomFloat( 0.0f, 1.0f ), vecMins.y, vecMaxs.y );
	vecLocalPosition.z = Lerp( random->RandomFloat( 0.0f, 1.0f ), vecMins.z, vecMaxs.z );
	VectorTransform( vecLocalPosition, boxToWorld, vecWorldPosition );

	SimpleParticle *pParticle = AddSimpleParticle( m_hMaterial, vecWorldPosition, AURA_LIFETIME, AURA_START_SIZE );
	if ( !pParticle )
		return;

	pParticle->m_flLifetime = flInitialDeltaTime;
	pParticle->m_flRoll = Helper_RandomInt( 0, 360 );
	pParticle->m_flRollDelta = Helper_RandomFloat( -2.0f, 2.0f );
	pParticle->m_uchStartAlpha = AURA_START_ALPHA;
	pParticle->m_uchEndAlpha = 0;
	pParticle->m_uchStartSize = AURA_START_SIZE;
	pParticle->m_uchEndSize = AURA_END_SIZE;
	pParticle->m_vecVelocity = vec3_origin;
}

void CHiddenAuraEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	// Only a living Hidden using the aura, standing still, sees other living players' trails.
	C_Hidden_Player *pLocal = C_Hidden_Player::GetLocalHiddenPlayer();
	C_BasePlayer *pOwner = m_hOwner;
	if ( !pLocal || !pLocal->IsAlive() || pLocal->GetTeamNumber() != TEAM_HIDDEN || !pLocal->IsAuraActive() ||
		 !pOwner || pOwner == pLocal || !pOwner->IsAlive() )
		return;

	// Colours the inverted screen turns into green, orange and red.
	Vector vecColor;
	const int iHealth = pOwner->GetHealth();
	if ( iHealth >= 74 )
		vecColor.Init( 1.0f, 0.5f, 1.0f );
	else if ( iHealth >= 37 )
		vecColor.Init( 0.0f, 0.65f, 1.0f );
	else
		vecColor.Init( 0.4f, 1.0f, 1.0f );

	const SimpleParticle *pParticle = (const SimpleParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		const float t = pParticle->m_flLifetime / pParticle->m_flDieTime;

		Vector tPos;
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		const float sortKey = tPos.z;

		const float flAlpha = Lerp( t, pParticle->m_uchStartAlpha / 255.0f, pParticle->m_uchEndAlpha / 255.0f );
		const float flSize = Lerp( t, (float)pParticle->m_uchStartSize, (float)pParticle->m_uchEndSize );
		RenderParticle_ColorSize( pIterator->GetParticleDraw(), tPos, vecColor, flAlpha, flSize );

		pParticle = (const SimpleParticle *)pIterator->GetNext( sortKey );
	}
}
