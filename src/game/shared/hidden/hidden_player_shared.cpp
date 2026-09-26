//========= Hidden: Source =====================================================//
//
// Purpose: Player code shared by the client and the server: footsteps.
//			See docs/spec/sounds.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIDDEN_STEP_VOLUME_SCALE	0.45f	// the Hidden's footsteps, against a marine's

// Beta 4b's players stepped with the surface's sounds (stock CBasePlayer), not HL2MP's
// per-model ones.
void CHidden_Player::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	// Only UpdateStepSound's footsteps come in unforced; jumps and landings are at full volume.
	if ( !force && GetTeamNumber() == TEAM_HIDDEN )
		fvol *= HIDDEN_STEP_VOLUME_SCALE;

	CBasePlayer::PlayStepSound( vecOrigin, psurface, fvol, force );
}

// Running footsteps from 180 rather than SDK 2013's 220, so marines at full speed run.
void CHidden_Player::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	BaseClass::GetStepSoundVelocities( velwalk, velrun );

	if ( !( GetFlags() & FL_DUCKING ) && GetMoveType() != MOVETYPE_LADDER )
		*velrun = 180.0f;
}
