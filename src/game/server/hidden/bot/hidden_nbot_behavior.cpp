//========= Hidden: Source =====================================================//
//
// Purpose: What the bots do. Marines roam in a group (or alone), look around,
//			and shoot the Hidden when they notice it; the Hidden hunts the most
//			isolated marine, pounces in, knifes or pigsticks it, retreats when
//			hurt and outnumbered, and feeds on corpses to heal.
//			Not a Beta 4b feature: see docs/spec/bots.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_nbot.h"
#include "hidden_corpse.h"
#include "NextBotBehavior.h"
#include "Path/NextBotChasePath.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_pigstick;
extern ConVar sv_gravity;

ConVar hdn_bot_debug( "hdn_bot_debug", "0", FCVAR_GAMEDLL | FCVAR_CHEAT, "Print what the bots decide" );

#define BOT_DEBUG( me, ... ) do { if ( hdn_bot_debug.GetBool() ) { Msg( "%.1f %s: ", gpGlobals->curtime, (me)->GetPlayerName() ); Msg( __VA_ARGS__ ); } } while ( 0 )

#define HIDDEN_POUNCE_SPEED		615.0f	// docs/spec/hidden-abilities.md
#define HIDDEN_POUNCE_STAMINA	20.0f
#define HIDDEN_KNIFE_RANGE		70.0f

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Follow the path when there is a nav mesh; head straight there when there isn't.
static void MoveAlongPath( CHiddenBot *me, PathFollower &path, const Vector &vecGoal, CountdownTimer &repathTimer, float flRepath )
{
	if ( repathTimer.IsElapsed() || !path.IsValid() )
	{
		repathTimer.Start( flRepath );
		CHiddenBotPathCost cost( me );
		path.Compute( me, vecGoal, cost );
	}

	if ( path.IsValid() && TheNavMesh->IsLoaded() )
		path.Update( me );
	else
		me->GetLocomotionInterface()->Approach( vecGoal );
}

// Glance around every few seconds, as a player on patrol would.
static void LookAround( CHiddenBot *me, CountdownTimer &lookTimer )
{
	if ( !lookTimer.IsElapsed() )
		return;
	lookTimer.Start( RandomFloat( 1.5f, 3.5f ) );

	QAngle angles = me->EyeAngles();
	angles.y += RandomFloat( -120.0f, 120.0f );
	angles.x = RandomFloat( -10.0f, 10.0f );

	Vector vecForward;
	AngleVectors( angles, &vecForward );
	me->GetBodyInterface()->AimHeadTowards( me->EyePosition() + vecForward * 500.0f, IBody::INTERESTING, 1.0f, NULL, "Looking around" );
}

static CNavArea *PickRandomArea( CHiddenBot *me, float flMaxRange )
{
	if ( !TheNavAreas.Count() )
		return NULL;

	for ( int iTry = 0; iTry < 20; iTry++ )
	{
		CNavArea *pArea = TheNavAreas[RandomInt( 0, TheNavAreas.Count() - 1 )];
		if ( pArea && ( flMaxRange <= 0.0f || me->IsRangeLessThan( pArea->GetCenter(), flMaxRange ) ) )
			return pArea;
	}
	return TheNavAreas[RandomInt( 0, TheNavAreas.Count() - 1 )];
}

// The teammate this bot keeps close to: the lowest-numbered other living marine,
// so a group forms around one player (humans included).
static CBasePlayer *FindGroupLeader( CHiddenBot *me )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer != me && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
		{
			CHiddenBot *pBot = ToHiddenBot( pPlayer );
			if ( !pBot || !pBot->IsLoner() )
				return pPlayer;
		}
	}
	return NULL;
}

static bool IsVisibleNow( const CKnownEntity *known )
{
	return known && known->GetEntity() && known->IsVisibleRecently() && known->GetTimeSinceLastSeen() < 0.5f;
}

//=============================================================================
// Marines
//=============================================================================
class CHiddenBotMarineAttack : public Action< CHiddenBot >
{
public:
	virtual ActionResult< CHiddenBot > OnStart( CHiddenBot *me, Action< CHiddenBot > *priorAction )
	{
		m_strafeTimer.Start( RandomFloat( 0.5f, 1.5f ) );
		m_bStrafeLeft = RandomInt( 0, 1 ) == 0;
		return Continue();
	}

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() )
			return Continue();

		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		if ( !threat || threat->IsObsolete() || !threat->GetEntity() )
			return Done( "No threat" );

		me->EquipBestGun();
		me->ReloadIfNeeded( true );

		CBaseEntity *pThreat = threat->GetEntity();

		if ( threat->IsVisibleRecently() )
		{
			const float flRange = me->GetRangeTo( pThreat );
			const float flTooClose = me->IsCloseRangeGun() ? 90.0f : 180.0f;

			if ( flRange < flTooClose )
			{
				me->PressBackwardButton();
			}
			else if ( flRange > ( me->IsCloseRangeGun() ? 450.0f : 900.0f ) || !me->IsLineOfFireClear( pThreat->WorldSpaceCenter() ) )
			{
				CHiddenBotPathCost cost( me );
				if ( TheNavMesh->IsLoaded() )
					m_chase.Update( me, pThreat, cost );
				else
					me->GetLocomotionInterface()->Approach( pThreat->GetAbsOrigin() );
			}

			// Harder bots don't stand still while shooting.
			if ( me->GetDifficulty() >= CHiddenBot::NORMAL )
			{
				if ( m_strafeTimer.IsElapsed() )
				{
					m_strafeTimer.Start( RandomFloat( 0.4f, 1.2f ) );
					m_bStrafeLeft = !m_bStrafeLeft;
				}
				if ( m_bStrafeLeft )
					me->PressLeftButton();
				else
					me->PressRightButton();
			}
		}
		else
		{
			// Lost it: check where it was last seen, then give up.
			const Vector &vecLast = threat->GetLastKnownPosition();
			if ( me->IsRangeLessThan( vecLast, 60.0f ) || threat->GetTimeSinceLastSeen() > 8.0f )
			{
				me->GetVisionInterface()->ForgetEntity( pThreat );
				return Done( "Lost the Hidden" );
			}

			me->GetBodyInterface()->AimHeadTowards( vecLast + Vector( 0, 0, HumanEyeHeight ), IBody::IMPORTANT, 0.5f, NULL, "Where I last saw it" );
			MoveAlongPath( me, m_path, vecLast, m_repathTimer, 1.0f );
		}

		return Continue();
	}

	virtual EventDesiredResult< CHiddenBot > OnStuck( CHiddenBot *me )
	{
		me->GetLocomotionInterface()->Jump();
		return TryContinue();
	}

	virtual const char *GetName( void ) const { return "MarineAttack"; }

private:
	ChasePath m_chase;
	PathFollower m_path;
	CountdownTimer m_repathTimer;
	CountdownTimer m_strafeTimer;
	bool m_bStrafeLeft;
};

class CHiddenBotMarineRoam : public Action< CHiddenBot >
{
public:
	virtual ActionResult< CHiddenBot > OnStart( CHiddenBot *me, Action< CHiddenBot > *priorAction )
	{
		m_pGoalArea = NULL;
		m_waitTimer.Invalidate();
		return Continue();
	}

	virtual ActionResult< CHiddenBot > OnResume( CHiddenBot *me, Action< CHiddenBot > *interruptingAction )
	{
		m_path.Invalidate();
		return Continue();
	}

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() )
			return Continue();

		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		if ( threat && !threat->IsObsolete() )
			return SuspendFor( new CHiddenBotMarineAttack, "Found the Hidden" );

		me->EquipBestGun();
		me->ReloadIfNeeded( false );
		LookAround( me, m_lookTimer );

		CBasePlayer *pLeader = me->IsLoner() ? NULL : FindGroupLeader( me );
		if ( pLeader )
		{
			// Keep near the group, drifting a little now and then.
			if ( me->IsRangeGreaterThan( pLeader, 220.0f ) )
			{
				MoveAlongPath( me, m_path, pLeader->GetAbsOrigin(), m_repathTimer, 1.0f );
				m_waitTimer.Invalidate();
			}
			else if ( !m_waitTimer.HasStarted() )
			{
				m_waitTimer.Start( RandomFloat( 2.0f, 6.0f ) );
			}
			else if ( m_waitTimer.IsElapsed() && TheNavMesh->IsLoaded() )
			{
				m_waitTimer.Invalidate();
				CNavArea *pArea = PickRandomArea( me, 300.0f );
				if ( pArea )
				{
					CHiddenBotPathCost cost( me );
					m_path.Compute( me, pArea->GetCenter(), cost );
					m_repathTimer.Start( 3.0f );
				}
			}
			else if ( m_path.IsValid() && !m_repathTimer.IsElapsed() )
			{
				m_path.Update( me );
			}
			return Continue();
		}

		// On its own: wander from place to place, pausing at each.
		if ( m_waitTimer.HasStarted() && !m_waitTimer.IsElapsed() )
			return Continue();

		if ( !m_pGoalArea || me->IsRangeLessThan( m_pGoalArea->GetCenter(), 80.0f ) || !m_path.IsValid() )
		{
			if ( m_pGoalArea )
				m_waitTimer.Start( RandomFloat( 1.0f, 4.0f ) );

			m_pGoalArea = PickRandomArea( me, 0.0f );
			if ( !m_pGoalArea )
			{
				// No nav mesh: at least keep moving.
				me->PressForwardButton();
				return Continue();
			}

			CHiddenBotPathCost cost( me );
			m_path.Compute( me, m_pGoalArea->GetCenter(), cost );
		}

		m_path.Update( me );
		return Continue();
	}

	virtual EventDesiredResult< CHiddenBot > OnMoveToFailure( CHiddenBot *me, const Path *path, MoveToFailureType reason )
	{
		m_pGoalArea = NULL;
		return TryContinue();
	}

	virtual EventDesiredResult< CHiddenBot > OnStuck( CHiddenBot *me )
	{
		me->GetLocomotionInterface()->Jump();
		m_pGoalArea = NULL;
		m_path.Invalidate();
		return TryContinue();
	}

	virtual const char *GetName( void ) const { return "MarineRoam"; }

private:
	PathFollower m_path;
	CNavArea *m_pGoalArea;
	CountdownTimer m_repathTimer;
	CountdownTimer m_waitTimer;
	CountdownTimer m_lookTimer;
};

//=============================================================================
// The Hidden
//=============================================================================
static void EquipKnife( CHiddenBot *me )
{
	CBaseCombatWeapon *pKnife = me->Weapon_OwnsThisType( "weapon_knife" );
	if ( pKnife && me->GetActiveWeapon() != pKnife )
		me->Weapon_Switch( pKnife );
}

static int CountAlliesNear( CBasePlayer *pPlayer, float flRange )
{
	int iAllies = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( pOther && pOther != pPlayer && pOther->IsAlive() && pOther->GetTeamNumber() == pPlayer->GetTeamNumber() &&
			( pOther->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).IsLengthLessThan( flRange ) )
		{
			iAllies++;
		}
	}
	return iAllies;
}

// The marine the Hidden goes for: near, and far from help.
static CBasePlayer *PickVictim( CHiddenBot *me )
{
	CBasePlayer *pBest = NULL;
	float flBestScore = FLT_MAX;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_IRIS )
			continue;

		int iAllies = 0;
		for ( int j = 1; j <= gpGlobals->maxClients; j++ )
		{
			CBasePlayer *pOther = UTIL_PlayerByIndex( j );
			if ( pOther && pOther != pPlayer && pOther->IsAlive() && pOther->GetTeamNumber() == TEAM_IRIS &&
				( pOther->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).IsLengthLessThan( 500.0f ) )
			{
				iAllies++;
			}
		}

		const float flScore = me->GetRangeTo( pPlayer ) + 600.0f * iAllies;
		if ( flScore < flBestScore )
		{
			flBestScore = flScore;
			pBest = pPlayer;
		}
	}
	return pBest;
}

static CHiddenCorpse *FindCorpseToFeedOn( CHiddenBot *me, float flMaxRange )
{
	CHiddenCorpse *pBest = NULL;
	float flBestRange = flMaxRange;

	for ( CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "corpse_ragdoll" ); pEnt;
		pEnt = gEntList.FindEntityByClassname( pEnt, "corpse_ragdoll" ) )
	{
		CHiddenCorpse *pCorpse = dynamic_cast< CHiddenCorpse * >( pEnt );
		if ( !pCorpse || pCorpse->GetFeedHealth() <= 0 )
			continue;

		const float flRange = me->GetRangeTo( pCorpse->WorldSpaceCenter() );
		if ( flRange < flBestRange )
		{
			flBestRange = flRange;
			pBest = pCorpse;
		}
	}
	return pBest;
}

// The closest marine seen in the last moment, if any.
static CBasePlayer *GetVisibleMarine( CHiddenBot *me, float flMaxRange )
{
	CBasePlayer *pBest = NULL;
	float flBestRange = flMaxRange;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_IRIS )
			continue;

		const CKnownEntity *known = me->GetVisionInterface()->GetKnown( pPlayer );
		if ( !known || !known->IsVisibleRecently() || known->GetTimeSinceLastSeen() > 1.0f )
			continue;

		const float flRange = me->GetRangeTo( pPlayer );
		if ( flRange < flBestRange )
		{
			flBestRange = flRange;
			pBest = pPlayer;
		}
	}
	return pBest;
}

// Whether the marine is looking our way (within about 50 degrees).
static bool IsFacingMe( CHiddenBot *me, CBasePlayer *pVictim )
{
	Vector vecFacing;
	AngleVectors( pVictim->EyeAngles(), &vecFacing );
	Vector vecToMe = me->GetAbsOrigin() - pVictim->GetAbsOrigin();
	vecToMe.z = 0.0f;
	vecFacing.z = 0.0f;
	vecToMe.NormalizeInPlace();
	vecFacing.NormalizeInPlace();
	return DotProduct( vecFacing, vecToMe ) > 0.65f;
}

// Launch angle for a pounce to land on the target (the low arc), aim straight when out of reach.
// A steep pounce goes up at 55 degrees instead, to clear a fence or a wall edge on the way.
static Vector GetPounceAimPoint( CHiddenBot *me, CBaseEntity *pTarget, bool bSteep = false )
{
	Vector vecTo = pTarget->WorldSpaceCenter() - me->GetAbsOrigin();
	const float flDZ = vecTo.z;
	vecTo.z = 0.0f;
	const float flDist = vecTo.NormalizeInPlace();

	const float v = HIDDEN_POUNCE_SPEED;
	const float g = sv_gravity.GetFloat();
	const float flDisc = v * v * v * v - g * ( g * flDist * flDist + 2.0f * flDZ * v * v );

	float flAngle = DEG2RAD( 45.0f );
	if ( bSteep )
		flAngle = DEG2RAD( 55.0f );
	else if ( flDisc >= 0.0f && flDist > 1.0f )
		flAngle = atanf( ( v * v - sqrtf( flDisc ) ) / ( g * flDist ) );

	const Vector vecDir = vecTo * cosf( flAngle ) + Vector( 0, 0, 1 ) * sinf( flAngle );
	return me->EyePosition() + vecDir * 200.0f;
}

class CHiddenBotRetreat : public Action< CHiddenBot >
{
public:
	CHiddenBotRetreat( float flMinTime = 4.0f, float flMaxTime = 7.0f ) : m_flMinTime( flMinTime ), m_flMaxTime( flMaxTime ) {}

	virtual ActionResult< CHiddenBot > OnStart( CHiddenBot *me, Action< CHiddenBot > *priorAction )
	{
		m_giveUpTimer.Start( RandomFloat( m_flMinTime, m_flMaxTime ) );
		m_leapTimer.Start( RandomFloat( 0.0f, 0.3f ) );
		m_bLeaping = false;
		return Continue();
	}

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() )
			return Continue();

		CBasePlayer *pThreat = GetVisibleMarine( me, 1500.0f );
		if ( m_giveUpTimer.IsElapsed() || !pThreat )
			return Done( "Got away" );

		EquipKnife( me );

		// Somewhere well away from the marine, on the far side of us.
		if ( m_repathTimer.IsElapsed() || !m_path.IsValid() )
		{
			m_repathTimer.Start( 1.5f );

			Vector vecAway = me->GetAbsOrigin() - pThreat->GetAbsOrigin();
			vecAway.z = 0.0f;
			vecAway.NormalizeInPlace();
			m_vecGoal = me->GetAbsOrigin() + vecAway * 600.0f;

			for ( int iTry = 0; iTry < 15 && TheNavAreas.Count(); iTry++ )
			{
				CNavArea *pArea = PickRandomArea( me, 1000.0f );
				if ( pArea && ( pArea->GetCenter() - pThreat->GetAbsOrigin() ).LengthSqr() > ( me->GetAbsOrigin() - pThreat->GetAbsOrigin() ).LengthSqr() + 300.0f * 300.0f )
				{
					m_vecGoal = pArea->GetCenter();
					break;
				}
			}

			CHiddenBotPathCost cost( me );
			m_path.Compute( me, m_vecGoal, cost );
		}

		// Leap away along the way we're running, when there's room.
		if ( m_bLeaping )
		{
			if ( me->GetBodyInterface()->IsHeadAimingOnTarget() || m_leapAimTimer.IsElapsed() )
			{
				BOT_DEBUG( me, "leaping away\n" );
				me->PressPounceButton();
				m_bLeaping = false;
				m_leapTimer.Start( RandomFloat( 2.0f, 3.0f ) );
			}
		}
		else if ( m_leapTimer.IsElapsed() && me->GetStamina() >= HIDDEN_POUNCE_STAMINA + 10.0f && me->GetLocomotionInterface()->IsOnGround() )
		{
			Vector vecDir = me->GetAbsVelocity();
			vecDir.z = 0.0f;
			if ( vecDir.NormalizeInPlace() > 100.0f )
			{
				trace_t tr;
				UTIL_TraceHull( me->GetAbsOrigin() + Vector( 0, 0, 20 ), me->GetAbsOrigin() + Vector( 0, 0, 20 ) + vecDir * 200.0f,
					VEC_HULL_MIN, VEC_HULL_MAX, MASK_PLAYERSOLID, me, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
				if ( !tr.DidHit() )
				{
					m_bLeaping = true;
					m_leapAimTimer.Start( 0.4f );
					const Vector vecAim = vecDir * cosf( DEG2RAD( 25.0f ) ) + Vector( 0, 0, sinf( DEG2RAD( 25.0f ) ) );
					me->GetBodyInterface()->AimHeadTowards( me->EyePosition() + vecAim * 200.0f, IBody::CRITICAL, 0.5f, NULL, "Leaping away" );
				}
			}
		}

		if ( m_path.IsValid() && TheNavMesh->IsLoaded() )
			m_path.Update( me );
		else
			me->GetLocomotionInterface()->Approach( m_vecGoal );
		return Continue();
	}

	virtual const char *GetName( void ) const { return "HiddenRetreat"; }

private:
	float m_flMinTime;
	float m_flMaxTime;
	CountdownTimer m_leapTimer;
	CountdownTimer m_leapAimTimer;
	bool m_bLeaping;
	PathFollower m_path;
	Vector m_vecGoal;
	CountdownTimer m_repathTimer;
	CountdownTimer m_giveUpTimer;
};

class CHiddenBotAttack : public Action< CHiddenBot >
{
public:
	CHiddenBotAttack( CBasePlayer *pVictim ) : m_chase( ChasePath::LEAD_SUBJECT ) { m_hVictim = pVictim; }

	virtual ActionResult< CHiddenBot > OnStart( CHiddenBot *me, Action< CHiddenBot > *priorAction )
	{
		m_bAimingPounce = false;
		m_pounceTimer.Start( RandomFloat( 0.5f, 1.5f ) );
		m_flBestRange = FLT_MAX;
		m_iStalls = 0;
		m_iSlashes = 0;
		m_iSlashesBeforeRunning = RandomInt( 1, 3 );
		m_bSteepPounce = false;
		m_progressTimer.Start( 3.0f );
		m_directTimer.Invalidate();
		m_strafeTimer.Start( 0.5f );
		m_bStrafeLeft = RandomInt( 0, 1 ) == 0;
		return Continue();
	}

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() )
			return Continue();

		EquipKnife( me );

		CBasePlayer *pVictim = m_hVictim.Get();
		if ( !pVictim || !pVictim->IsAlive() || pVictim->GetTeamNumber() != TEAM_IRIS )
		{
			// Hit and run: don't wait for the rest of them to turn up.
			if ( GetVisibleMarine( me, 1000.0f ) )
				return ChangeTo( new CHiddenBotRetreat, "Kill made, slipping away" );
			return Done( "Victim gone" );
		}

		const CKnownEntity *known = me->GetVisionInterface()->GetKnown( pVictim );
		if ( !known || known->GetTimeSinceLastSeen() > 4.0f )
			return Done( "Lost the victim" );

		// Someone much closer turned up: go for them instead.
		CBasePlayer *pCloser = GetVisibleMarine( me, 0.5f * me->GetRangeTo( pVictim ) );
		if ( pCloser )
		{
			m_hVictim = pCloser;
			pVictim = pCloser;
		}

		if ( ( me->GetHealth() < 50 && me->GetVisionInterface()->GetKnownCount( TEAM_IRIS, true, 800.0f ) >= 2 ) || me->GetHealth() < 25 )
			return ChangeTo( new CHiddenBotRetreat, "Hurt and outnumbered" );

		const float flRange = me->GetRangeTo( pVictim );

		// Up close: knife it, and keep circling.
		if ( flRange < HIDDEN_KNIFE_RANGE + 30.0f )
		{
			m_bAimingPounce = false;
			me->GetBodyInterface()->AimHeadTowards( pVictim, IBody::CRITICAL, 0.3f, NULL, "Knifing" );
			me->GetLocomotionInterface()->Approach( pVictim->GetAbsOrigin() );

			if ( flRange < HIDDEN_KNIFE_RANGE && me->GetBodyInterface()->IsHeadAimingOnTarget() && m_knifeTimer.IsElapsed() )
			{
				if ( ShouldPigstick( me, pVictim ) )
				{
					BOT_DEBUG( me, "pigstick %s\n", pVictim->GetPlayerName() );
					me->PressAltFireButton();
					m_knifeTimer.Start( 3.0f );
				}
				else
				{
					BOT_DEBUG( me, "slash %s\n", pVictim->GetPlayerName() );
					me->PressFireButton();
					m_knifeTimer.Start( 0.5f );

					// Hit and run: a slash or two, then away before they can line up a shot.
					if ( ++m_iSlashes >= m_iSlashesBeforeRunning )
						return ChangeTo( new CHiddenBotRetreat( 1.5f, 3.0f ), "Hit and run" );
				}
			}

			Strafe( me );
			return Continue();
		}

		// A pounce under way: wait until the aim is up, then jump.
		if ( m_bAimingPounce )
		{
			me->GetBodyInterface()->AimHeadTowards( GetPounceAimPoint( me, pVictim, m_bSteepPounce ), IBody::CRITICAL, 0.3f, NULL, "Aiming a pounce" );
			if ( me->GetBodyInterface()->IsHeadAimingOnTarget() || m_pounceAimTimer.IsElapsed() )
			{
				BOT_DEBUG( me, "pounce at %s from %.0f %.0f %.0f, %.0f away, stamina %.0f\n", pVictim->GetPlayerName(), me->GetAbsOrigin().x, me->GetAbsOrigin().y, me->GetAbsOrigin().z, flRange, me->GetStamina() );
				me->PressPounceButton();
				m_bAimingPounce = false;
				m_pounceTimer.Start( RandomFloat( 2.0f, 4.0f ) );
			}
			return Continue();
		}

		// Not getting any closer: the chase heads straight for a near target, which fails across a
		// fence or a drop, so follow a full path to them for a while.
		if ( flRange < m_flBestRange - 20.0f )
		{
			m_flBestRange = flRange;
			m_progressTimer.Start( 3.0f );
			m_iStalls = 0;
		}
		else if ( m_progressTimer.IsElapsed() )
		{
			m_iStalls++;
			BOT_DEBUG( me, "no progress towards %s (%d), taking the long way\n", pVictim->GetPlayerName(), m_iStalls );
			m_directTimer.Start( 4.0f );
			m_path.Invalidate();
			m_flBestRange = flRange;
			m_progressTimer.Start( 3.0f );
		}
		const bool bPathing = !m_directTimer.IsElapsed();

		// Pounce from mid range, or straight down onto someone below us, or when stuck.
		const float flHoriz = ( pVictim->GetAbsOrigin() - me->GetAbsOrigin() ).Length2D();
		const float flDZ = pVictim->GetAbsOrigin().z - me->GetAbsOrigin().z;
		const bool bBelow = flDZ < -50.0f && flHoriz < 350.0f;
		// Head-on pounces from range get it shot; wait until they look away or we're close.
		const bool bFacingMe = IsFacingMe( me, pVictim );
		const bool bMidRange = flRange > 160.0f && flRange < 550.0f && ( !bFacingMe || flRange < 280.0f ) &&
			RandomFloat() < 0.5f + 0.15f * me->GetDifficulty();

		const bool bCanSee = IsVisibleNow( known );
		const bool bClear = me->IsLineOfFireClear( pVictim->WorldSpaceCenter() );
		const bool bReady = m_pounceTimer.IsElapsed() && me->GetStamina() >= HIDDEN_POUNCE_STAMINA + 10.0f && me->GetLocomotionInterface()->IsOnGround();

		// Seen through a fence we can't get round: leap over it.
		if ( bCanSee && bReady && !bClear && m_iStalls >= 1 && flHoriz < 500.0f )
		{
			BOT_DEBUG( me, "pouncing over something at %s\n", pVictim->GetPlayerName() );
			m_bAimingPounce = true;
			m_bSteepPounce = true;
			m_pounceAimTimer.Start( 0.6f );
			return Continue();
		}

		if ( bCanSee && bReady && bClear && ( bMidRange || bBelow ) )
		{
			m_bSteepPounce = false;
			m_bAimingPounce = true;
			m_pounceAimTimer.Start( 0.6f );
			return Continue();
		}

		// Close the distance, weaving while it can see us.
		me->GetBodyInterface()->AimHeadTowards( pVictim, IBody::IMPORTANT, 0.3f, NULL, "Stalking" );
		if ( bPathing )
		{
			MoveAlongPath( me, m_path, pVictim->GetAbsOrigin(), m_repathTimer, 1.0f );
		}
		else if ( bFacingMe && flRange > 250.0f && TheNavMesh->IsLoaded() )
		{
			// It's looking at us: work round behind it rather than walk into its sights.
			if ( m_flankTimer.IsElapsed() || !m_flankPath.IsValid() )
			{
				m_flankTimer.Start( 1.0f );

				Vector vecFacing;
				AngleVectors( pVictim->EyeAngles(), &vecFacing );
				vecFacing.z = 0.0f;
				vecFacing.NormalizeInPlace();
				const Vector vecSide( -vecFacing.y, vecFacing.x, 0.0f );
				const float flSide = ( ( me->GetAbsOrigin() - pVictim->GetAbsOrigin() ).Dot( vecSide ) > 0.0f ) ? 1.0f : -1.0f;
				m_vecFlank = pVictim->GetAbsOrigin() - vecFacing * 200.0f + vecSide * flSide * 250.0f;

				CHiddenBotPathCost cost( me );
				m_flankPath.Compute( me, m_vecFlank, cost );
				BOT_DEBUG( me, "flanking %s\n", pVictim->GetPlayerName() );
			}
			m_flankPath.Update( me );
		}
		else
		{
			CHiddenBotPathCost cost( me );
			if ( TheNavMesh->IsLoaded() )
				m_chase.Update( me, pVictim, cost );

			// No path (or no mesh): head straight for them.
			if ( !TheNavMesh->IsLoaded() || !m_chase.IsValid() )
				me->GetLocomotionInterface()->Approach( pVictim->GetAbsOrigin() );
		}

		if ( bCanSee && flRange < 700.0f && me->GetDifficulty() >= CHiddenBot::NORMAL )
			Strafe( me );

		if ( hdn_bot_debug.GetBool() && m_debugTimer.IsElapsed() )
		{
			m_debugTimer.Start( 2.0f );
			BOT_DEBUG( me, "attacking %s from %.0f %.0f %.0f, %.0f away (dz %.0f), %s, path %s, speed %.0f, stamina %.0f\n", pVictim->GetPlayerName(), me->GetAbsOrigin().x, me->GetAbsOrigin().y, me->GetAbsOrigin().z, flRange,
				pVictim->GetAbsOrigin().z - me->GetAbsOrigin().z, bCanSee ? "visible" : "hidden", m_chase.IsValid() ? "ok" : "none",
				me->GetAbsVelocity().Length(), me->GetStamina() );
		}

		return Continue();
	}

	virtual EventDesiredResult< CHiddenBot > OnStuck( CHiddenBot *me )
	{
		me->GetLocomotionInterface()->Jump();
		return TryContinue();
	}

	virtual const char *GetName( void ) const { return "HiddenAttack"; }

private:
	// The pigstick kills outright but lands 1.3 s later: use it on a marine who isn't facing us
	// or is barely moving, and not every time.
	bool ShouldPigstick( CHiddenBot *me, CBasePlayer *pVictim ) const
	{
		if ( !sv_pigstick.GetBool() )
			return false;

		Vector vecFacing;
		AngleVectors( pVictim->EyeAngles(), &vecFacing );
		Vector vecToMe = me->GetAbsOrigin() - pVictim->GetAbsOrigin();
		vecToMe.z = 0.0f;
		vecToMe.NormalizeInPlace();

		const bool bBehind = DotProduct( vecFacing, vecToMe ) < 0.2f;
		const bool bSlow = pVictim->GetAbsVelocity().Length2D() < 60.0f;
		if ( bBehind )
			return RandomFloat() < 0.6f + 0.1f * me->GetDifficulty();
		return bSlow && RandomFloat() < 0.3f + 0.1f * me->GetDifficulty();
	}

	void Strafe( CHiddenBot *me )
	{
		if ( m_strafeTimer.IsElapsed() )
		{
			m_strafeTimer.Start( RandomFloat( 0.3f, 0.9f ) );
			m_bStrafeLeft = !m_bStrafeLeft;
		}
		if ( m_bStrafeLeft )
			me->PressLeftButton();
		else
			me->PressRightButton();
	}

	CHandle< CBasePlayer > m_hVictim;
	ChasePath m_chase;
	PathFollower m_path;
	PathFollower m_flankPath;
	Vector m_vecFlank;
	CountdownTimer m_flankTimer;
	CountdownTimer m_repathTimer;
	CountdownTimer m_pounceTimer;
	CountdownTimer m_pounceAimTimer;
	CountdownTimer m_knifeTimer;
	CountdownTimer m_strafeTimer;
	CountdownTimer m_debugTimer;
	CountdownTimer m_progressTimer;
	CountdownTimer m_directTimer;
	float m_flBestRange;
	int m_iStalls;
	int m_iSlashes;
	int m_iSlashesBeforeRunning;
	bool m_bSteepPounce;
	bool m_bAimingPounce;
	bool m_bStrafeLeft;
};

class CHiddenBotFeed : public Action< CHiddenBot >
{
public:
	CHiddenBotFeed( CHiddenCorpse *pCorpse ) { m_hCorpse = pCorpse; }

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() )
			return Continue();

		CHiddenCorpse *pCorpse = m_hCorpse.Get();
		if ( !pCorpse || pCorpse->GetFeedHealth() <= 0 || me->GetHealth() >= 95 )
			return Done( "Done feeding" );

		if ( GetVisibleMarine( me, 1200.0f ) )
			return Done( "Disturbed" );

		EquipKnife( me );

		const Vector vecCorpse = pCorpse->WorldSpaceCenter();
		if ( me->IsRangeGreaterThan( vecCorpse, 55.0f ) )
		{
			MoveAlongPath( me, m_path, vecCorpse, m_repathTimer, 2.0f );
			return Continue();
		}

		me->GetBodyInterface()->AimHeadTowards( vecCorpse, IBody::CRITICAL, 0.3f, NULL, "Feeding" );
		if ( me->GetBodyInterface()->IsHeadAimingOnTarget() )
			me->PressFireButton();

		return Continue();
	}

	virtual const char *GetName( void ) const { return "HiddenFeed"; }

private:
	CHandle< CHiddenCorpse > m_hCorpse;
	PathFollower m_path;
	CountdownTimer m_repathTimer;
};

class CHiddenBotHunt : public Action< CHiddenBot >
{
public:
	virtual ActionResult< CHiddenBot > OnResume( CHiddenBot *me, Action< CHiddenBot > *interruptingAction )
	{
		m_path.Invalidate();
		m_pickTimer.Invalidate();
		return Continue();
	}

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() )
			return Continue();

		EquipKnife( me );

		// Go for a marine with at most two others near (or one right next to us); stalk bigger groups.
		CBasePlayer *pSeen = GetVisibleMarine( me, 900.0f );
		if ( pSeen && ( CountAlliesNear( pSeen, 400.0f ) <= 2 || me->IsRangeLessThan( pSeen, 250.0f ) || ( me->GetHealth() > 90 && RandomFloat() < 0.02f ) ) )
			return SuspendFor( new CHiddenBotAttack( pSeen ), "Found a marine" );

		// A group in sight and close enough to spot us: back off out of view and wait for them to split up.
		if ( pSeen && me->IsRangeLessThan( pSeen, 1000.0f ) )
			return SuspendFor( new CHiddenBotRetreat, "Too many of them" );

		if ( me->GetHealth() < 60 && me->GetVisionInterface()->GetTimeSinceVisible( TEAM_IRIS ) > 3.0f )
		{
			CHiddenCorpse *pCorpse = FindCorpseToFeedOn( me, 1500.0f );
			if ( pCorpse )
				return SuspendFor( new CHiddenBotFeed( pCorpse ), "Hungry" );
		}

		// Always on the move, towards whoever is most alone.
		if ( m_pickTimer.IsElapsed() || !m_hVictim.Get() || !m_hVictim->IsAlive() )
		{
			m_pickTimer.Start( RandomFloat( 3.0f, 5.0f ) );
			m_hVictim = PickVictim( me );
		}

		CBasePlayer *pVictim = m_hVictim.Get();
		if ( !pVictim )
		{
			LookAround( me, m_lookTimer );
			return Continue();
		}

		MoveAlongPath( me, m_path, pVictim->GetAbsOrigin(), m_repathTimer, 1.5f );

		if ( hdn_bot_debug.GetBool() && m_debugTimer.IsElapsed() )
		{
			m_debugTimer.Start( 3.0f );
			const Vector &vecPos = me->GetAbsOrigin();
			BOT_DEBUG( me, "hunting %s, %.0f away, at %.0f %.0f %.0f, speed %.0f, path %s, area %d\n", pVictim->GetPlayerName(),
				me->GetRangeTo( pVictim ), vecPos.x, vecPos.y, vecPos.z, me->GetAbsVelocity().Length(),
				m_path.IsValid() ? "ok" : "none", me->GetLastKnownArea() ? me->GetLastKnownArea()->GetID() : -1 );
		}
		return Continue();
	}

	virtual EventDesiredResult< CHiddenBot > OnStuck( CHiddenBot *me )
	{
		me->GetLocomotionInterface()->Jump();
		m_path.Invalidate();
		return TryContinue();
	}

	virtual const char *GetName( void ) const { return "HiddenHunt"; }

private:
	CHandle< CBasePlayer > m_hVictim;
	PathFollower m_path;
	CountdownTimer m_repathTimer;
	CountdownTimer m_pickTimer;
	CountdownTimer m_lookTimer;
	CountdownTimer m_debugTimer;
};

// Don't shoot through a friend.
static bool IsTeammateInTheWay( CHiddenBot *me, CBaseEntity *pTarget )
{
	Vector vecAim;
	me->EyeVectors( &vecAim );

	trace_t tr;
	UTIL_TraceLine( me->EyePosition(), me->EyePosition() + vecAim * me->GetRangeTo( pTarget->WorldSpaceCenter() ), MASK_SHOT, me, COLLISION_GROUP_NONE, &tr );
	return tr.m_pEnt && tr.m_pEnt != pTarget && tr.m_pEnt->IsPlayer() && tr.m_pEnt->GetTeamNumber() == me->GetTeamNumber();
}

//=============================================================================
// Main action: picks the team's behaviour, and aims and fires for marines.
//=============================================================================
class CHiddenBotMainAction : public Action< CHiddenBot >
{
public:
	virtual Action< CHiddenBot > *InitialContainedAction( CHiddenBot *me )
	{
		if ( me->GetTeamNumber() == TEAM_HIDDEN )
			return new CHiddenBotHunt;
		return new CHiddenBotMarineRoam;
	}

	virtual ActionResult< CHiddenBot > Update( CHiddenBot *me, float interval )
	{
		if ( !me->IsAlive() || !me->IsOnPlayingTeam() || me->IsObserver() )
			return Continue();

		me->GetVisionInterface()->SetFieldOfView( me->GetFOV() );

		if ( me->GetTeamNumber() == TEAM_IRIS )
			AimAndFire( me );

		return Continue();
	}

	virtual EventDesiredResult< CHiddenBot > OnInjured( CHiddenBot *me, const CTakeDamageInfo &info )
	{
		// Being hit gives the attacker away.
		CBaseEntity *pAttacker = info.GetAttacker();
		if ( pAttacker && me->IsEnemy( pAttacker ) )
			me->GetVisionInterface()->AddKnownEntity( pAttacker );
		return TryContinue();
	}

	virtual EventDesiredResult< CHiddenBot > OnStuck( CHiddenBot *me )
	{
		me->GetLocomotionInterface()->Jump();
		if ( RandomInt( 0, 1 ) )
			me->PressLeftButton( 0.5f );
		else
			me->PressRightButton( 0.5f );
		return TryContinue();
	}

	virtual QueryResultType ShouldAttack( const INextBot *me, const CKnownEntity *them ) const { return ANSWER_YES; }

	// Marines aim at the chest with an error that shrinks with skill; the Hidden aims true.
	virtual Vector SelectTargetPoint( const INextBot *meBot, const CBaseCombatCharacter *subject ) const
	{
		CHiddenBot *me = static_cast< CHiddenBot * >( meBot->GetEntity() );
		Vector vecTarget = subject->WorldSpaceCenter() + Vector( 0, 0, 8 );
		if ( me->GetTeamNumber() == TEAM_HIDDEN )
			return vecTarget;

		// The cloak makes a moving Hidden hard to pin down: up to 4 degrees more at full speed.
		static const float s_flErrorDegrees[] = { 4.0f, 2.5f, 1.5f, 0.7f };
		const float flMoving = 4.0f * clamp( subject->GetAbsVelocity().Length() / 220.0f, 0.0f, 1.0f );
		const float flError = tanf( DEG2RAD( s_flErrorDegrees[me->GetDifficulty()] + flMoving ) ) * me->GetRangeTo( vecTarget );
		return vecTarget + Vector( RandomFloat( -flError, flError ), RandomFloat( -flError, flError ), RandomFloat( -flError, flError ) * 0.5f );
	}

	virtual const char *GetName( void ) const { return "MainAction"; }

private:
	void AimAndFire( CHiddenBot *me )
	{
		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		if ( !threat || !threat->GetEntity() )
			return;

		if ( !IsVisibleNow( threat ) )
			return;

		CBaseEntity *pThreat = threat->GetEntity();
		me->GetBodyInterface()->AimHeadTowards( pThreat, IBody::CRITICAL, 0.5f, NULL, "Aiming at the Hidden" );

		if ( me->GetBodyInterface()->GetLookAtSubject() == pThreat && me->GetBodyInterface()->IsHeadAimingOnTarget() &&
			me->IsLineOfFireClear( pThreat->WorldSpaceCenter() ) && !IsTeammateInTheWay( me, pThreat ) )
		{
			me->FireGun();
		}
	}
};

IMPLEMENT_INTENTION_INTERFACE( CHiddenBot, CHiddenBotMainAction );
