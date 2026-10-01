//========= Hidden: Source =====================================================//
//
// Purpose: Player movement. Beta 4b's CSDKGameMovement is the SDK's CGameMovement
//			plus the Hidden's pounce and wall cling and the marines' slower
//			backwards walk, so we keep HL2's class but drop HL2's sprinting,
//			jump and useable ladders. See docs/spec/hidden-abilities.md.
//
//=============================================================================//

#include "cbase.h"
#include "hl_gamemovement.h"
#include "hidden_shareddefs.h"
#include "hidden_player_shared.h"
#include "in_buttons.h"
#include "hidden_cvars.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool g_bMovementOptimizations;

float HiddenStaminaTickScale( void )
{
	static ConVarRef hsm_fr_tick( "hsm_fr_tick" );
	const float flTick = hsm_fr_tick.IsValid() ? hsm_fr_tick.GetFloat() : 0.0f;
	return ( flTick >= 1.0f ) ? flTick * TICK_INTERVAL : 1.0f;
}

#ifdef GAME_DLL
static ConVar hdn_debug_movement( "hdn_debug_movement", "0", FCVAR_CHEAT, "Print the Hidden's wall cling and pounce events" );
#define HIDDEN_MOVEMENT_DEBUG( ... ) do { if ( hdn_debug_movement.GetBool() ) Msg( __VA_ARGS__ ); } while ( 0 )
#else
#define HIDDEN_MOVEMENT_DEBUG( ... ) do {} while ( 0 )
#endif

class CHiddenGameMovement : public CHL2GameMovement
{
	typedef CHL2GameMovement BaseClass;
public:
	// Beta 4b's speeds: the player's max speed as set by the game, with no HL2 sprint or walk.
	virtual void CheckParameters( void );
	virtual void ReduceTimers( void ) { CGameMovement::ReduceTimers(); }

	virtual bool CheckJumpButton( void );
	virtual void FullWalkMove( void );
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual void Duck( void );

	// Brush ladders, as in the SDK, instead of HL2's useable ladders.
	virtual bool LadderMove( void );
	virtual void FullLadderMove( void ) { CGameMovement::FullLadderMove(); }
	virtual bool OnLadder( trace_t &trace ) { return CGameMovement::OnLadder( trace ); }
	virtual int GetCheckInterval( IntervalType_t type ) { return CGameMovement::GetCheckInterval( type ); }

	// What the Hidden's movement runs into, for hdn_debug_movement.
	virtual void OnTryPlayerMoveCollision( trace_t &tr )
	{
		if ( IsHidden() && tr.m_pEnt && !tr.m_pEnt->IsWorld() )
			HIDDEN_MOVEMENT_DEBUG( "%.2f blocked by %s (group %d)\n", gpGlobals->curtime, tr.m_pEnt->GetClassname(), tr.m_pEnt->GetCollisionGroup() );
	}

private:
	CHidden_Player *GetHiddenPlayer( void ) { return static_cast<CHidden_Player *>( player ); }

	void SetClinging( bool bClinging, const char *pszWhy )
	{
		if ( GetHiddenPlayer()->IsClinging() != bClinging )
			HIDDEN_MOVEMENT_DEBUG( "%.2f cling %s: %s\n", gpGlobals->curtime, bClinging ? "on" : "off", pszWhy );
		GetHiddenPlayer()->SetClinging( bClinging );
	}
	bool IsHidden( void ) { return player->GetTeamNumber() == TEAM_HIDDEN; }

	void CheckBack( void );
	bool CheckPounceButton( void );
	void WallCling( void );
};

void CHiddenGameMovement::CheckParameters( void )
{
	// The same speed on both sides (m_flMaxspeed isn't networked); sv_maxspeed still caps it.
	mv->m_flMaxSpeed = mv->m_flClientMaxSpeed = MIN( GetHiddenPlayer()->UpdateMaxSpeed( mv->m_nButtons, mv->m_nOldButtons ), sv_maxspeed.GetFloat() );

	CGameMovement::CheckParameters();
}

// Marines on the ground walk backwards at 80 %. Beta 4b checks this right after the duck, and it
// counts as the speed crop, so a crouching marine backing off gets the crouch's third instead.
void CHiddenGameMovement::CheckBack( void )
{
	if ( player->GetTeamNumber() != TEAM_IRIS || m_iSpeedCropped != SPEED_CROPPED_RESET )
		return;

	if ( player->GetGroundEntity() != NULL && ( mv->m_nButtons & IN_BACK ) )
	{
		m_iSpeedCropped |= SPEED_CROPPED_DUCK;
		mv->m_flForwardMove *= HIDDEN_BACK_SPEED_SCALE;
	}
}

// The SDK's jump (21 units at 800 gravity) without HL2's forward boost; a clinging Hidden can jump
// off the wall.
bool CHiddenGameMovement::CheckJumpButton( void )
{
	if ( player->pl.deadflag )
	{
		mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
		return false;
	}

	// See if we are waterjumping. If so, decrement count and return.
	if ( player->m_flWaterJumpTime )
	{
		player->m_flWaterJumpTime -= gpGlobals->frametime;
		if ( player->m_flWaterJumpTime < 0 )
			player->m_flWaterJumpTime = 0;

		return false;
	}

	// If we are in the water most of the way...
	if ( player->GetWaterLevel() >= 2 )
	{
		// swimming, not jumping
		SetGroundEntity( NULL );

		if ( player->GetWaterType() == CONTENTS_WATER )
			mv->m_vecVelocity[2] = 100;
		else if ( player->GetWaterType() == CONTENTS_SLIME )
			mv->m_vecVelocity[2] = 80;

		// play swimming sound
		if ( player->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second
			player->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	CHidden_Player *pHidden = GetHiddenPlayer();
	if ( player->GetGroundEntity() == NULL && !pHidden->IsClinging() )
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	// Don't allow jumping when the player is in a stasis field.
	if ( player->m_Local.m_bSlowMovement )
		return false;

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	// Cannot jump while in the unduck transition.
	if ( player->m_Local.m_bDucking && ( player->GetFlags() & FL_DUCKING ) )
		return false;

	// Still updating the eye position.
	if ( player->m_Local.m_flDuckJumpTime > 0.0f )
		return false;

	SetClinging( false, "jump" );

	// In the air now.
	SetGroundEntity( NULL );

	player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );

	MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );

	float flGroundFactor = 1.0f;
	if ( player->m_pSurfaceData )
		flGroundFactor = player->m_pSurfaceData->game.jumpFactor;

	// Like Beta 4b, the optimised path doesn't follow sv_gravity.
	const float flMul = g_bMovementOptimizations ? 268.3281572999747f : sqrt( 2 * GetCurrentGravity() * GAMEMOVEMENT_JUMP_HEIGHT );

	// Accelerate upward. If we are ducking...
	const float startz = mv->m_vecVelocity[2];
	if ( player->m_Local.m_bDucking || ( player->GetFlags() & FL_DUCKING ) )
		mv->m_vecVelocity[2] = flGroundFactor * flMul;
	else
		mv->m_vecVelocity[2] += flGroundFactor * flMul;

	FinishGravity();

	mv->m_outJumpVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.15f;

	OnJump( mv->m_outJumpVel.z );

	// Set jump time.
	if ( gpGlobals->maxClients == 1 )
	{
		player->m_Local.m_flJumpTime = GAMEMOVEMENT_JUMP_TIME;
		player->m_Local.m_bInDuckJump = true;
	}

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}

// The Hidden's pounce: a leap along the aim, from the ground or off a wall.
bool CHiddenGameMovement::CheckPounceButton( void )
{
	if ( player->pl.deadflag )
	{
		mv->m_nOldButtons |= IN_BULLRUSH;
		return false;
	}

	CHidden_Player *pHidden = GetHiddenPlayer();
	if ( pHidden->GetStamina() < HIDDEN_POUNCE_STAMINA )
	{
		if ( !( mv->m_nOldButtons & IN_BULLRUSH ) )
			HIDDEN_MOVEMENT_DEBUG( "%.2f no pounce: stamina %.1f\n", gpGlobals->curtime, pHidden->GetStamina() );
		return false;
	}

	// In the water it's a swim stroke, as a jump would be (and costs nothing).
	if ( player->GetWaterLevel() >= 2 )
	{
		SetGroundEntity( NULL );

		if ( player->GetWaterType() == CONTENTS_WATER )
			mv->m_vecVelocity[2] = 100;
		else if ( player->GetWaterType() == CONTENTS_SLIME )
			mv->m_vecVelocity[2] = 80;

		if ( player->m_flSwimSoundTime <= 0 )
		{
			player->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	if ( mv->m_nOldButtons & IN_BULLRUSH )
		return false;

	if ( player->m_Local.m_flDuckJumpTime > 0.0f )
		return false;

	if ( player->GetGroundEntity() == NULL && !pHidden->IsClinging() )
		return false;

	HIDDEN_MOVEMENT_DEBUG( "%.2f pounce from %s\n", gpGlobals->curtime,
		player->GetGroundEntity() ? player->GetGroundEntity()->GetClassname() : "a wall" );
	SetGroundEntity( NULL );
	SetClinging( false, "pounce" );

	player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );
	MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );

	Vector vecAim;
	AngleVectors( mv->m_vecViewAngles + player->m_Local.m_vecPunchAngle, &vecAim );
	mv->m_vecVelocity = vecAim * HIDDEN_POUNCE_SPEED;
	HIDDEN_MOVEMENT_DEBUG( "%.2f pounce: velocity %.0f %.0f %.0f\n", gpGlobals->curtime, mv->m_vecVelocity.x, mv->m_vecVelocity.y, mv->m_vecVelocity.z );

	FinishGravity();

	pHidden->SetStamina( -HIDDEN_POUNCE_STAMINA );
	mv->m_nOldButtons |= IN_BULLRUSH;
	return true;
}

void CHiddenGameMovement::FullWalkMove( void )
{
	if ( GetHiddenPlayer()->IsClinging() )
	{
		WallCling();
		return;
	}

	if ( !( mv->m_nButtons & IN_BULLRUSH ) )
		mv->m_nOldButtons &= ~IN_BULLRUSH;

	BaseClass::FullWalkMove();
}

// Beta 4b checks the pounce right after the jump, before the ground move; the ground friction
// in between doesn't matter since the pounce replaces the velocity.
void CHiddenGameMovement::WalkMove( void )
{
	if ( IsHidden() && ( mv->m_nButtons & IN_BULLRUSH ) && CheckPounceButton() )
	{
		AirMove();
		return;
	}

	BaseClass::WalkMove();
}

// A Hidden in the air holding the pounce key grabs a wall within 24 units of the eyes.
void CHiddenGameMovement::AirMove( void )
{
	CHidden_Player *pHidden = GetHiddenPlayer();
	if ( IsHidden() && !pHidden->IsClinging() && ( mv->m_nButtons & IN_BULLRUSH ) )
	{
		Vector vecForward;
		player->EyeVectors( &vecForward );

		const Vector vecEyes = player->EyePosition();
		trace_t tr;
		UTIL_TraceLine( vecEyes, vecEyes + vecForward * HIDDEN_CLING_DISTANCE, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
		{
			SetClinging( true, "grabbed a wall" );
			return;
		}
	}

	BaseClass::AirMove();
}

// Stuck to the wall: no movement, stamina drains, and jump or pounce lets go.
void CHiddenGameMovement::WallCling( void )
{
	CHidden_Player *pHidden = GetHiddenPlayer();

	mv->m_vecVelocity.Init();

	if ( player->GetGroundEntity() != NULL || pHidden->GetStamina() < 1.0f )
	{
		SetClinging( false, player->GetGroundEntity() ? "on the ground" : "out of stamina" );
		return;
	}

	pHidden->SetStamina( hdn_staminadrain.GetFloat() * HiddenStaminaTickScale() );

	// Holding jump costs stamina every tick, even when the jump doesn't fire (Beta 4b).
	if ( mv->m_nButtons & IN_JUMP )
	{
		pHidden->SetStamina( -HIDDEN_CLING_JUMP_STAMINA );
		CheckJumpButton();
	}
	else
	{
		mv->m_nOldButtons &= ~IN_JUMP;
	}

	if ( mv->m_nButtons & IN_BULLRUSH )
		CheckPounceButton();
	else
		mv->m_nOldButtons &= ~IN_BULLRUSH;
}

// Ducking lets go of the wall.
void CHiddenGameMovement::Duck( void )
{
	CHidden_Player *pHidden = GetHiddenPlayer();
	if ( pHidden->IsClinging() && player->IsAlive() &&
		 ( ( mv->m_nButtons & IN_DUCK ) || player->m_Local.m_bDucking || ( player->GetFlags() & FL_DUCKING ) || player->m_Local.m_bInDuckJump ) )
	{
		SetClinging( false, "duck" );
	}

	BaseClass::Duck();
	CheckBack();
}

bool CHiddenGameMovement::LadderMove( void )
{
	if ( !CGameMovement::LadderMove() )
		return false;

	SetClinging( false, "ladder" );

	// Marines put their weapon away while they hold on to a ladder, every tick they're on it;
	// CWeaponHiddenBase::ItemPostFrame brings it back out once they're off.
	if ( player->GetTeamNumber() == TEAM_IRIS )
	{
		CBaseCombatWeapon *pWeapon = player->GetActiveWeapon();
		if ( pWeapon )
			pWeapon->Holster( NULL );
	}

	return true;
}

// Expose our interface in place of HL2's.
static CHiddenGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
