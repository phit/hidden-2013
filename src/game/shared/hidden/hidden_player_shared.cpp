//========= Hidden: Source =====================================================//
//
// Purpose: Player code shared by the client and the server: footsteps and
//			the max speed.
//			See docs/spec/sounds.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_player_shared.h"
#include "in_buttons.h"

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

// Beta 4b's CSDKPlayer speeds, run on both sides every command so the client predicts them: pressing
// +walk (not while ducking) walks and letting go runs, the boost (Boost, from impulse 100) speeds a
// marine up for 10 s, and whichever came last sets the speed; a boost running out goes back to
// running even with +walk held.
float CHidden_Player::UpdateMaxSpeed( int nButtons, int nOldButtons )
{
	const int nPressed = nButtons & ~nOldButtons;
	const int nReleased = nOldButtons & ~nButtons;

	if ( ( nPressed | nReleased ) & IN_WALK )
	{
		if ( m_bWalking && !( nPressed & IN_WALK ) )
		{
			m_bWalking = false;
			m_iSpeedMode = HIDDEN_SPEED_RUN;
		}
		else if ( !m_bWalking && ( nPressed & IN_WALK ) && !( nButtons & IN_DUCK ) )
		{
			m_bWalking = true;
			m_iSpeedMode = HIDDEN_SPEED_WALK;
		}
	}

	if ( m_flBoostEnd > 0.0f && m_flBoostEnd < gpGlobals->curtime )
	{
		m_flBoostEnd = 0.0f;
		m_iSpeedMode = HIDDEN_SPEED_RUN;
	}

	float flSpeed;
	if ( GetTeamNumber() == TEAM_HIDDEN )
		flSpeed = ( m_iSpeedMode == HIDDEN_SPEED_WALK ) ? HIDDEN_HIDDEN_WALK_SPEED : HIDDEN_HIDDEN_SPEED;
	else if ( m_iSpeedMode == HIDDEN_SPEED_WALK )
		flSpeed = HIDDEN_MARINE_WALK_SPEED;
	else if ( m_iSpeedMode == HIDDEN_SPEED_BOOST )
		flSpeed = HIDDEN_BOOST_SPEED;
	else
		flSpeed = HIDDEN_MARINE_SPEED;

#ifndef CLIENT_DLL
	SetMaxSpeed( flSpeed );	// for server code that reads it; the client doesn't receive it
#endif
	return flSpeed;
}
