//========= Hidden: Source =====================================================//
//
// Purpose: Server-side player corpses (corpse_ragdoll), which the Hidden can
//			feed on and tear apart. See docs/spec/hidden-abilities.md.
//
//=============================================================================//

#ifndef HIDDEN_CORPSE_H
#define HIDDEN_CORPSE_H
#pragma once

#include "physics_prop_ragdoll.h"

#define HIDDEN_CORPSE_HEALTH	25	// what a corpse has to feed on

// Beta 4b's corpse_ragdoll was prop_ragdoll under a second name, with a feeding pool Hidden added
// to every ragdoll. Maps place them too (htr_tutorial).
class CHiddenCorpse : public CRagdollProp
{
public:
	DECLARE_CLASS( CHiddenCorpse, CRagdollProp );
	DECLARE_DATADESC();

	CHiddenCorpse() : m_iFeedHealth( HIDDEN_CORPSE_HEALTH ) {}

	int GetFeedHealth( void ) const { return m_iFeedHealth; }
	void AddFeedHealth( int iDelta ) { m_iFeedHealth += iDelta; }

	// The knife's pigstick: tear the corpse into pieces and remove it. vecForce throws the limbs,
	// vecDir is the stab's direction.
	void TearApart( const Vector &vecForce, const Vector &vecDir );

	virtual void UpdateOnRemove( void );

private:
	int m_iFeedHealth;
};

void PrecacheHiddenCorpses( void );

// The Hidden's secondary attack while carrying a ragdoll (any, as in Beta 4b: htr_tutorial's pinning
// lesson uses a prop_ragdoll): pin it to the wall in front, if any (called from
// CPlayerPickupController::Use just before it drops the ragdoll).
void HiddenPinHeldRagdoll( CBasePlayer *pPlayer, CBaseEntity *pHeld );

// A dying player's corpse (Beta 4b's CreateServerCorpseRagdoll).
CHiddenCorpse *CreateHiddenCorpse( CBaseAnimating *pAnimating, int nForceBone, const CTakeDamageInfo &info, int nCollisionGroup );

#endif // HIDDEN_CORPSE_H
