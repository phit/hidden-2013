//========= Hidden: Source =====================================================//
//
// Purpose: Playable bots for both teams, on NextBot (Beta 4b's own bots were
//			the SDK template's test bots, now bot_add_test in hidden_bot.cpp).
//			Not a Beta 4b feature: see docs/spec/bots.md.
//
//=============================================================================//

#ifndef HIDDEN_NBOT_H
#define HIDDEN_NBOT_H
#ifdef _WIN32
#pragma once
#endif

#include "hidden_player.h"
#include "NextBot/Player/NextBotPlayer.h"
#include "NextBot/Player/NextBotPlayerBody.h"
#include "NextBot/Player/NextBotPlayerLocomotion.h"
#include "NextBotVisionInterface.h"
#include "nav_mesh.h"

class CHiddenBot;

//-----------------------------------------------------------------------------
// Nav mesh generation seeded from every Hidden and marine spawn; stock seeds
// from info_player_start, which the Hidden maps don't have.
//-----------------------------------------------------------------------------
class CHiddenNavMesh : public CNavMesh
{
public:
	virtual void AddWalkableSeeds( void );
};

//-----------------------------------------------------------------------------
class CHiddenBotLocomotion : public PlayerLocomotion
{
public:
	DECLARE_CLASS( CHiddenBotLocomotion, PlayerLocomotion );

	CHiddenBotLocomotion( INextBot *bot ) : PlayerLocomotion( bot ) {}

	virtual float GetMaxJumpHeight( void ) const { return 56.0f; }
	virtual float GetDeathDropHeight( void ) const;
	virtual bool IsAreaTraversable( const CNavArea *area ) const;
	virtual bool IsEntityTraversable( CBaseEntity *obstacle, TraverseWhenType when = EVENTUALLY ) const;

protected:
	virtual void AdjustPosture( const Vector &moveGoal ) {}	// never crouch to navigate
};

//-----------------------------------------------------------------------------
class CHiddenBotBody : public PlayerBody
{
public:
	CHiddenBotBody( INextBot *bot ) : PlayerBody( bot ) {}

	virtual float GetHeadAimTrackingInterval( void ) const;
};

//-----------------------------------------------------------------------------
// Marines only notice the cloaked Hidden now and then: surely up close, more
// often the nearer and the stiller it is, and easily once they're tracking it.
//-----------------------------------------------------------------------------
class CHiddenBotVision : public IVision
{
public:
	CHiddenBotVision( INextBot *bot ) : IVision( bot ) { memset( m_flLastNoticeCheck, 0, sizeof( m_flLastNoticeCheck ) ); }

	virtual void CollectPotentiallyVisibleEntities( CUtlVector< CBaseEntity * > *potentiallyVisible );
	virtual bool IsIgnored( CBaseEntity *subject ) const;
	virtual bool IsVisibleEntityNoticed( CBaseEntity *subject ) const;
	virtual float GetMaxVisionRange( void ) const { return 4000.0f; }
	virtual float GetMinRecognizeTime( void ) const;

private:
	mutable float m_flLastNoticeCheck[MAX_PLAYERS + 1];	// per subject, so the chance is per second
};

//-----------------------------------------------------------------------------
class CHiddenBot : public NextBotPlayer< CHidden_Player >
{
public:
	DECLARE_CLASS( CHiddenBot, NextBotPlayer< CHidden_Player > );

	enum DifficultyType
	{
		EASY,
		NORMAL,
		HARD,
		EXPERT,
	};

	CHiddenBot();
	virtual ~CHiddenBot();

	static CBasePlayer *AllocatePlayerEntity( edict_t *edict, const char *playerName );

	virtual void Spawn( void );
	virtual bool IsDormantWhenDead( void ) const { return false; }	// let the game's death think run

	DECLARE_INTENTION_INTERFACE( CHiddenBot );

	virtual PlayerLocomotion *GetLocomotionInterface( void ) const { return m_locomotor; }
	virtual PlayerBody *GetBodyInterface( void ) const { return m_body; }
	virtual IVision *GetVisionInterface( void ) const { return m_vision; }

	virtual bool IsEnemy( const CBaseEntity *them ) const;

	DifficultyType GetDifficulty( void ) const { return m_difficulty; }
	void SetDifficulty( DifficultyType difficulty ) { m_difficulty = difficulty; }

	// A marine who keeps to the others, or one who wanders off alone.
	bool IsLoner( void ) const { return m_bLoner; }

	bool IsOnPlayingTeam( void ) const { return GetTeamNumber() == TEAM_IRIS || GetTeamNumber() == TEAM_HIDDEN; }

	// The Hidden's pounce key (IN_BULLRUSH), for one tick, unless the leap along the current aim
	// would end in the void or a trigger_hurt.
	void PressPounceButton( void );

	// Marine weapons: pick a gun with ammo, reload, and fire it the way it wants.
	void EquipBestGun( void );
	void ReloadIfNeeded( bool bInCombat );
	void FireGun( void );
	bool IsCloseRangeGun( void ) const;

	bool IsLineOfFireClear( const Vector &where );

	virtual void Update( void );
	virtual void Event_Killed( const CTakeDamageInfo &info );

private:
	// Keeps the bot from stepping off a drop it can't take or into a trigger_hurt.
	void GuardLedges( void );
	bool IsStepSafe( const Vector &vecDir ) const;
	bool IsLeapSafe( void );
	bool LedgeDebug( const char *pszWhy, const Vector &vecWhere ) const;

	CHiddenBotLocomotion *m_locomotor;
	CHiddenBotBody *m_body;
	CHiddenBotVision *m_vision;

	DifficultyType m_difficulty;
	bool m_bLoner;
	bool m_bFireToggle;
	float m_flLeftGround;	// debug: when and how the bot last left the ground
	Vector m_vecLastGround;
	Vector m_vecLastGroundVel;
	int m_iLastGroundButtons;
	mutable float m_flNextLedgeDebug;
};

inline CHiddenBot *ToHiddenBot( CBaseEntity *pEntity )
{
	return ( pEntity && pEntity->IsPlayer() ) ? dynamic_cast< CHiddenBot * >( pEntity ) : NULL;
}

// Path cost for the bots: distance, with jumps and big drops priced or ruled out.
class CHiddenBotPathCost : public IPathCost
{
public:
	CHiddenBotPathCost( CHiddenBot *me ) : m_me( me ) {}
	virtual float operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const;

private:
	CHiddenBot *m_me;
};

#endif // HIDDEN_NBOT_H
