//========= Hidden: Source =====================================================//
//
// Purpose: Server-side player corpses (corpse_ragdoll). Beta 4b made every
//			player's corpse a server ragdoll so the Hidden could feed on it, tear
//			it apart with the pigstick and carry it. See
//			docs/spec/hidden-abilities.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_corpse.h"
#include "hidden_player.h"
#include "gib.h"
#include "studio.h"
#include "bone_setup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CORPSE_GIB_LIFETIME		10.0f
#define CORPSE_GIB_MIN_SPEED	750.0f
#define CORPSE_GIB_MAX_SPEED	1500.0f
#define CORPSE_GIB_FORCE_DAMAGE	50.0f	// the torn-up corpse is thrown as by a 50 damage hit

LINK_ENTITY_TO_CLASS( corpse_ragdoll, CHiddenCorpse );

BEGIN_DATADESC( CHiddenCorpse )
	DEFINE_FIELD( m_iFeedHealth, FIELD_INTEGER ),
END_DATADESC()

// A marine killed by more than 100 damage, or by a blast, leaves one of three torn-up torsos and
// its pieces.
static const char *s_pszTorsos[] =
{
	"models/gibs/iristorso_1.mdl",
	"models/gibs/iristorso_2.mdl",
	"models/gibs/iristorso_3.mdl",
};

static const char *s_pszTorsoGibs[][7] =
{
	{ "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs2.mdl", "models/gibs/iris_gibs3.mdl", "models/gibs/iris_gibs3.mdl", "models/gibs/iris_gibs4.mdl", "models/gibs/iris_gibs6.mdl" },
	{ "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs2.mdl", "models/gibs/iris_gibs4.mdl", "models/gibs/iris_gibs5.mdl", "models/gibs/iris_gibs6.mdl", "models/gibs/iris_gibs7.mdl", NULL },
	{ "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs2.mdl", "models/gibs/iris_gibs3.mdl", "models/gibs/iris_gibs4.mdl", "models/gibs/iris_gibs6.mdl", "models/gibs/iris_gibs7.mdl", NULL },
};

// The pigstick's pieces, in order: a leg, six gibs, an arm, four gibs, another leg.
#define RAGGIB_LEG	"models/gibs/iris_raggibs_leg.mdl"
#define RAGGIB_ARM	"models/gibs/iris_raggibs_arm.mdl"

static const char *s_pszPigstickGibs1[] = { "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs2.mdl", "models/gibs/iris_gibs2.mdl", "models/gibs/iris_gibs3.mdl", "models/gibs/iris_gibs3.mdl" };
static const char *s_pszPigstickGibs2[] = { "models/gibs/iris_gibs4.mdl", "models/gibs/iris_gibs6.mdl", "models/gibs/iris_gibs7.mdl", "models/gibs/iris_gibs7.mdl" };

void PrecacheHiddenCorpses( void )
{
	for ( int i = 0; i < ARRAYSIZE( s_pszTorsos ); i++ )
		CBaseEntity::PrecacheModel( s_pszTorsos[i] );

	for ( int i = 1; i <= 7; i++ )
		CBaseEntity::PrecacheModel( UTIL_VarArgs( "models/gibs/iris_gibs%d.mdl", i ) );

	CBaseEntity::PrecacheModel( RAGGIB_LEG );
	CBaseEntity::PrecacheModel( RAGGIB_ARM );
}

static void SpawnGibs( CBaseEntity *pVictim, const char **ppszModels, int nCount )
{
	for ( int i = 0; i < nCount && ppszModels[i]; i++ )
		CGib::SpawnSpecificGibs( pVictim, 1, CORPSE_GIB_MIN_SPEED, CORPSE_GIB_MAX_SPEED, ppszModels[i], CORPSE_GIB_LIFETIME );
}

void CHiddenCorpse::TearApart( const Vector &vecForce, const Vector &vecDir )
{
	const Vector vecOrigin = GetAbsOrigin();
	const QAngle angles = GetAbsAngles();

	CreateRagGib( RAGGIB_LEG, vecOrigin, angles, vecForce, CORPSE_GIB_LIFETIME );
	SpawnGibs( this, s_pszPigstickGibs1, ARRAYSIZE( s_pszPigstickGibs1 ) );
	CreateRagGib( RAGGIB_ARM, vecOrigin, angles, vecForce, CORPSE_GIB_LIFETIME );
	SpawnGibs( this, s_pszPigstickGibs2, ARRAYSIZE( s_pszPigstickGibs2 ) );
	CreateRagGib( RAGGIB_LEG, vecOrigin, angles, vecForce, CORPSE_GIB_LIFETIME );

	UTIL_BloodImpact( vecOrigin, vecDir, BLOOD_COLOR_RED, 5 );
	UTIL_BloodSpray( vecOrigin, -vecDir, BLOOD_COLOR_RED, 10, FX_BLOODSPRAY_ALL );

	UTIL_Remove( this );
}

// The torsos have their own, smaller skeletons; pose each bone as the player's bone of the same
// name (Beta 4b matched them by index, which scrambled the pose), or as the pelvis.
static void RemapBonesByName( CStudioHdr *pFrom, const matrix3x4_t *pFromBones, CStudioHdr *pTo, matrix3x4_t *pToBones )
{
	for ( int i = 0; i < pTo->numbones(); i++ )
	{
		int iFrom = Studio_BoneIndexByName( pFrom, pTo->pBone( i )->pszName() );
		if ( iFrom < 0 )
			iFrom = 0;

		MatrixCopy( pFromBones[iFrom], pToBones[i] );
	}
}

CHiddenCorpse *CreateHiddenCorpse( CBaseAnimating *pAnimating, int nForceBone, const CTakeDamageInfo &info, int nCollisionGroup )
{
	CHiddenCorpse *pCorpse = static_cast<CHiddenCorpse *>( CBaseEntity::CreateNoSpawn( "corpse_ragdoll", pAnimating->GetAbsOrigin(), vec3_angle, NULL ) );
	pCorpse->CopyAnimationDataFrom( pAnimating );
	pCorpse->InitRagdollAnimation();

	if ( pAnimating->IsEFlagSet( EFL_NO_DISSOLVE ) )
		pCorpse->AddEFlags( EFL_NO_DISSOLVE );

	pCorpse->SetKiller( info.GetInflictor() );
	pCorpse->SetSourceClassName( pAnimating->GetClassname() );

	// The bones now and a moment ago, as CreateServerRagdoll takes them.
	static matrix3x4_t pBoneToWorld[MAXSTUDIOBONES], pBoneToWorldNext[MAXSTUDIOBONES];
	float dt = 0.1f;

	const unsigned short fPrevFlags = pAnimating->GetBoneCacheFlags();
	pAnimating->SetBoneCacheFlags( BCF_NO_ANIMATION_SKIP );

	const float flSequenceDuration = pAnimating->SequenceDuration( pAnimating->GetSequence() );
	const float flSequenceTime = pAnimating->GetCycle() * flSequenceDuration;
	if ( flSequenceTime <= dt && flSequenceTime > 0.0f )
		dt = flSequenceTime;

	const float flPreviousCycle = clamp( pAnimating->GetCycle() - ( dt * ( 1 / flSequenceDuration ) ), 0.f, 1.f );
	const float flCurCycle = pAnimating->GetCycle();
	pAnimating->SetupBones( pBoneToWorldNext, BONE_USED_BY_ANYTHING );
	pAnimating->SetCycle( flPreviousCycle );
	pAnimating->SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );
	pAnimating->SetCycle( flCurCycle );

	pAnimating->ClearBoneCacheFlags( BCF_NO_ANIMATION_SKIP );
	pAnimating->SetBoneCacheFlags( fPrevFlags );

	Vector vel = pAnimating->GetAbsVelocity();
	if ( vel.LengthSqr() > 0 )
	{
		const int numbones = pAnimating->GetModelPtr()->numbones();
		vel *= dt;
		for ( int i = 0; i < numbones; i++ )
		{
			Vector pos;
			MatrixGetColumn( pBoneToWorld[i], 3, pos );
			pos -= vel;
			MatrixSetColumn( pos, 3, pBoneToWorld[i] );
		}
	}

	const bool bGib = ( pAnimating->GetTeamNumber() != TEAM_HIDDEN && info.GetDamage() > 100.0f ) || info.GetDamageType() == DMG_BLAST;
	if ( !bGib )
	{
		pCorpse->InitRagdoll( info.GetDamageForce(), nForceBone, info.GetDamagePosition(), pBoneToWorld, pBoneToWorldNext, dt, nCollisionGroup, true );
	}
	else
	{
		// Beta 4b throws the torso along the attacker's normalised eye *position* (sic).
		CTakeDamageInfo gibInfo = info;
		gibInfo.SetDamage( CORPSE_GIB_FORCE_DAMAGE );

		CBaseEntity *pAttacker = info.GetAttacker();
		Vector vecDir = pAttacker ? pAttacker->EyePosition() : vec3_origin;
		VectorNormalize( vecDir );
		CalculateMeleeDamageForce( &gibInfo, vecDir, pAnimating->GetAbsOrigin(), 1.0f );

		const int iVariant = random->RandomInt( 0, ARRAYSIZE( s_pszTorsos ) - 1 );
		UTIL_BloodSpray( pAnimating->GetAbsOrigin(), vecDir, BLOOD_COLOR_RED, 10, FX_BLOODSPRAY_ALL );

		CStudioHdr *pFrom = pAnimating->GetModelPtr();
		pCorpse->SetModel( s_pszTorsos[iVariant] );
		SpawnGibs( pAnimating, s_pszTorsoGibs[iVariant], ARRAYSIZE( s_pszTorsoGibs[iVariant] ) );

		// The torso's skin follows the marine's class: 2 for assault, 1 for support.
		CHidden_Player *pPlayer = ToHiddenPlayer( pAnimating );
		pCorpse->m_nSkin = ( pPlayer && pPlayer->GetPlayerClass() == HIDDEN_CLASS_ASSAULT ) ? 2 : 1;

		static matrix3x4_t pTorsoBones[MAXSTUDIOBONES], pTorsoBonesNext[MAXSTUDIOBONES];
		CStudioHdr *pTo = pCorpse->GetModelPtr();
		RemapBonesByName( pFrom, pBoneToWorld, pTo, pTorsoBones );
		RemapBonesByName( pFrom, pBoneToWorldNext, pTo, pTorsoBonesNext );

		pCorpse->InitRagdoll( gibInfo.GetDamageForce(), nForceBone, gibInfo.GetDamagePosition(), pTorsoBones, pTorsoBonesNext, dt, nCollisionGroup, true );

		// Nothing left to feed on.
		pCorpse->AddFeedHealth( -HIDDEN_CORPSE_HEALTH );
	}

	if ( pAnimating->IsDissolving() )
		pCorpse->TransferDissolveFrom( pAnimating );

	// As CreateServerRagdoll: valid bounds for the first frame, or the client fades it out.
	pCorpse->CollisionProp()->SetCollisionBounds( pAnimating->CollisionProp()->OBBMins(), pAnimating->CollisionProp()->OBBMaxs() );

	return pCorpse;
}
