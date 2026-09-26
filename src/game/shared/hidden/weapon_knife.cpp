//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden's knife (Beta 4b's CBludgeonWeaponSDKBase, HL2's crowbar
//			code): slashes and the delayed pigstick. See docs/spec/hidden-abilities.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "hidden_cvars.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "effect_dispatch_data.h"

#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "te_effect_dispatch.h"
	#include "ilagcompensationmanager.h"
	#include "hidden_corpse.h"
	#include "RagdollBoogie.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponKnife C_WeaponKnife
#endif

#define KNIFE_RANGE				96.0f
#define KNIFE_HULL_DIM			16.0f
#define KNIFE_HULL_RADIUS		( 1.732f * KNIFE_HULL_DIM )	// centre to corner of the hull
#define KNIFE_TIME_TO_IDLE		3.0f

#define KNIFE_SLASH_DAMAGE		37.0f
#define KNIFE_PIGSTICK_DAMAGE	925.0f
#define KNIFE_TRIGGER_DAMAGE	25.0f
#define KNIFE_FEED_HEALTH		5		// taken from a corpse per slash
#define KNIFE_FEED_MAX_HEALTH	100

// The pigstick lands 1.3 s after the key press; the knife is busy for longer.
#define PIGSTICK_HIT_DELAY		1.3f
#define PIGSTICK_NEXT_PRIMARY	2.55f
#define PIGSTICK_NEXT_SECONDARY	3.0f

static const Vector g_vecKnifeMins( -KNIFE_HULL_DIM, -KNIFE_HULL_DIM, -KNIFE_HULL_DIM );
static const Vector g_vecKnifeMaxs( KNIFE_HULL_DIM, KNIFE_HULL_DIM, KNIFE_HULL_DIM );

class CWeaponKnife : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponKnife, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponKnife();

	virtual void Spawn( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void ItemPostFrame( void );

	// The knife is never lowered.
	virtual void SetSafe( void ) {}

private:
	void Swing( bool bPigstick );
	void Hit( trace_t &tr, Activity nHitActivity );
	void ChooseIntersectionPoint( trace_t &tr, CBasePlayer *pOwner );
	void ImpactWater( const Vector &vecStart, const Vector &vecEnd );
	void AddViewKick( void );

	CNetworkVar( bool, m_bPigstickPending );
	CNetworkVar( float, m_flPigstickTime );

	CWeaponKnife( const CWeaponKnife & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponKnife, DT_WeaponKnife )

BEGIN_NETWORK_TABLE( CWeaponKnife, DT_WeaponKnife )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bPigstickPending ) ),
	RecvPropTime( RECVINFO( m_flPigstickTime ) ),
#else
	SendPropBool( SENDINFO( m_bPigstickPending ) ),
	SendPropTime( SENDINFO( m_flPigstickTime ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponKnife )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bPigstickPending, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flPigstickTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_knife, CWeaponKnife );
PRECACHE_WEAPON_REGISTER( weapon_knife );

CWeaponKnife::CWeaponKnife()
{
	m_bPigstickPending = false;
	m_flPigstickTime = 0.0f;
	m_bFiresUnderwater = true;
}

void CWeaponKnife::Spawn( void )
{
	BaseClass::Spawn();

	AddEffects( EF_NOSHADOW );
	m_nBody = 1;
}

bool CWeaponKnife::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	// Can't put the knife away mid-pigstick.
	if ( m_bPigstickPending )
		return false;

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponKnife::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( m_bPigstickPending && m_flPigstickTime < gpGlobals->curtime )
	{
		Swing( true );
		return;
	}

	if ( ( pOwner->m_nButtons & IN_ATTACK2 ) && m_flNextSecondaryAttack <= gpGlobals->curtime && !m_bPigstickPending )
		SecondaryAttack();

	if ( ( pOwner->m_nButtons & IN_ATTACK ) && m_flNextPrimaryAttack <= gpGlobals->curtime && !m_bPigstickPending )
		PrimaryAttack();

	if ( !( pOwner->m_nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) )
		WeaponIdle();
}

void CWeaponKnife::PrimaryAttack( void )
{
	if ( m_bPigstickPending )
		return;

	Swing( false );
}

void CWeaponKnife::SecondaryAttack( void )
{
	if ( !sv_pigstick.GetBool() || m_bPigstickPending )
		return;

	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.2f;
		return;
	}

	// Wind up; the stab itself comes from ItemPostFrame when the time is up.
	m_flPigstickTime = gpGlobals->curtime + PIGSTICK_HIT_DELAY;
	m_flNextSecondaryAttack = gpGlobals->curtime + PIGSTICK_NEXT_SECONDARY;
	m_flNextPrimaryAttack = gpGlobals->curtime + PIGSTICK_NEXT_PRIMARY;
	m_bPigstickPending = true;

	SendWeaponAnim( ACT_VM_HITCENTER2 );
	WeaponSound( SPECIAL1 );
}

void CWeaponKnife::Swing( bool bPigstick )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		return;
	}

#ifndef CLIENT_DLL
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	const Vector vecSwingStart = pPlayer->Weapon_ShootPosition();
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward, NULL, NULL );
	Vector vecSwingEnd = vecSwingStart + vecForward * KNIFE_RANGE;

	trace_t tr;
	UTIL_TraceLine( vecSwingStart, vecSwingEnd, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &tr );

	Activity nActivity = bPigstick ? ACT_VM_HITCENTER2 : ACT_VM_HITCENTER;

#ifndef CLIENT_DLL
	CTakeDamageInfo triggerInfo( pPlayer, pPlayer, KNIFE_TRIGGER_DAMAGE, DMG_CLUB );
	TraceAttackToTriggers( triggerInfo, tr.startpos, tr.endpos, vec3_origin );
#endif

	if ( tr.fraction == 1.0f )
	{
		// Missed with the line: try a hull, pulled back so its corners reach no further.
		vecSwingEnd -= vecForward * KNIFE_HULL_RADIUS;
		UTIL_TraceHull( vecSwingStart, vecSwingEnd, g_vecKnifeMins, g_vecKnifeMaxs, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction < 1.0f && tr.m_pEnt )
		{
			Vector vecToTarget = tr.m_pEnt->GetAbsOrigin() - vecSwingStart;
			VectorNormalize( vecToTarget );

			// Only if the target is roughly in front.
			if ( DotProduct( vecToTarget, vecForward ) >= 0.70721f )
				ChooseIntersectionPoint( tr, pPlayer );
			else
				tr.fraction = 1.0f;
		}
	}

	ImpactWater( vecSwingStart, tr.endpos );

	if ( tr.fraction == 1.0f )
		nActivity = ACT_VM_MISSCENTER;	// Beta 4b uses the slash miss for the pigstick too
	else
		Hit( tr, nActivity );

	WeaponSound( SINGLE );

	// The pigstick's animation started with the wind-up.
	if ( !bPigstick )
	{
		SendWeaponAnim( nActivity );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	}

	SetWeaponIdleTime( gpGlobals->curtime + KNIFE_TIME_TO_IDLE );
	m_bPigstickPending = false;

#ifndef CLIENT_DLL
	lagcompensation->FinishLagCompensation( pPlayer );
#endif
}

void CWeaponKnife::Hit( trace_t &tr, Activity nHitActivity )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	AddViewKick();

	CBaseEntity *pHit = tr.m_pEnt;
	if ( pHit )
	{
		Vector vecForward;
		pPlayer->EyeVectors( &vecForward, NULL, NULL );
		VectorNormalize( vecForward );

#ifndef CLIENT_DLL
		const float flDamage = ( nHitActivity == ACT_VM_HITCENTER2 ) ? KNIFE_PIGSTICK_DAMAGE : KNIFE_SLASH_DAMAGE;

		CTakeDamageInfo info( pPlayer, pPlayer, flDamage, DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForward, tr.endpos );
		pHit->DispatchTraceAttack( info, vecForward, &tr );
		ApplyMultiDamage();

		// Corpses: the pigstick tears one apart; a slash feeds the Hidden 5 health while the corpse
		// has any left, and makes it twitch.
		if ( FClassnameIs( pHit, "corpse_ragdoll" ) )
		{
			CHiddenCorpse *pCorpse = static_cast<CHiddenCorpse *>( pHit );
			if ( nHitActivity == ACT_VM_HITCENTER2 )
			{
				pCorpse->TearApart( info.GetDamageForce(), vecForward );
			}
			else
			{
				if ( pCorpse->GetFeedHealth() > 0 )
				{
					pPlayer->SetHealth( MIN( pPlayer->GetHealth() + KNIFE_FEED_HEALTH, KNIFE_FEED_MAX_HEALTH ) );
					pCorpse->AddFeedHealth( -KNIFE_FEED_HEALTH );
				}

				UTIL_BloodSpray( pCorpse->GetAbsOrigin(), -vecForward, BLOOD_COLOR_RED, random->RandomInt( 4, 8 ), FX_BLOODSPRAY_ALL );

				if ( pCorpse->GetFeedHealth() > 0 )
					CRagdollBoogie::Create( pCorpse, 250.0f, gpGlobals->curtime, 0.5f );
			}
		}

		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecForward );
#endif
	}

	UTIL_ImpactTrace( &tr, DMG_SLASH );
}

// HL2's crowbar: when the hull hits, find the nearest corner trace that does, so the impact lands
// on the surface.
void CWeaponKnife::ChooseIntersectionPoint( trace_t &tr, CBasePlayer *pOwner )
{
	const Vector vecSrc = tr.startpos;
	const Vector vecHullEnd = vecSrc + ( ( tr.endpos - vecSrc ) * 2.0f );

	trace_t tmpTrace;
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction != 1.0f )
	{
		tr = tmpTrace;
		return;
	}

	const Vector *pMinMax[2] = { &g_vecKnifeMins, &g_vecKnifeMaxs };
	float flDistance = 1e6f;

	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 2; j++ )
		{
			for ( int k = 0; k < 2; k++ )
			{
				Vector vecEnd;
				vecEnd.x = vecHullEnd.x + pMinMax[i]->x;
				vecEnd.y = vecHullEnd.y + pMinMax[j]->y;
				vecEnd.z = vecHullEnd.z + pMinMax[k]->z;

				UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
				if ( tmpTrace.fraction < 1.0f )
				{
					const float flThisDistance = ( tmpTrace.endpos - vecSrc ).Length();
					if ( flThisDistance < flDistance )
					{
						tr = tmpTrace;
						flDistance = flThisDistance;
					}
				}
			}
		}
	}
}

void CWeaponKnife::ImpactWater( const Vector &vecStart, const Vector &vecEnd )
{
	// Nothing to do if the swing starts in water.
	if ( enginetrace->GetPointContents( vecStart ) & ( CONTENTS_WATER | CONTENTS_SLIME ) )
		return;

	trace_t tr;
	UTIL_TraceLine( vecStart, vecEnd, CONTENTS_WATER | CONTENTS_SLIME, GetOwner(), COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction >= 1.0f )
		return;

	CEffectData data;
	data.m_vOrigin = tr.endpos;
	data.m_vNormal = tr.plane.normal;
	data.m_flScale = 8.0f;
	if ( tr.contents & CONTENTS_SLIME )
		data.m_fFlags |= FX_WATER_IN_SLIME;

	DispatchEffect( "watersplash", data );
}

void CWeaponKnife::AddViewKick( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	QAngle angPunch;
	angPunch.x = SharedRandomFloat( "knifepax", -1.0f, 1.0f );
	angPunch.y = SharedRandomFloat( "knifepay", -1.0f, 1.0f );
	angPunch.z = 0.0f;
	pPlayer->ViewPunch( angPunch );
}
