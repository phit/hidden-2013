//========= Hidden: Source =====================================================//
//
// Purpose: The aura trails the Hidden sees on marines. See docs/spec/client.md.
//
//=============================================================================//

#ifndef HIDDEN_AURATRAIL_H
#define HIDDEN_AURATRAIL_H
#pragma once

#include "particles_simple.h"
#include "timedevent.h"

// One per player: Beta 4b's sprites/aura entity particle trail, drawn only for a Hidden using the
// aura, in a colour that shows the owner's health.
class CHiddenAuraEmitter : public CSimpleEmitter
{
public:
	DECLARE_CLASS( CHiddenAuraEmitter, CSimpleEmitter );

	static CSmartPtr<CHiddenAuraEmitter> Create( C_BasePlayer *pOwner );

	// Adds this frame's particles over the owner's hitboxes.
	void Emit( float flTimeDelta );

	virtual void RenderParticles( CParticleRenderIterator *pIterator );

private:
	CHiddenAuraEmitter( C_BasePlayer *pOwner );
	CHiddenAuraEmitter( const CHiddenAuraEmitter & );

	void AddParticle( float flInitialDeltaTime, const Vector &vecMins, const Vector &vecMaxs, const matrix3x4_t &boxToWorld );

	CHandle<C_BasePlayer> m_hOwner;
	PMaterialHandle m_hMaterial;
	TimedEvent m_teParticleSpawn;
};

#endif // HIDDEN_AURATRAIL_H
