//========= Hidden: Source =====================================================//
//
// Purpose: FN303 less-lethal launcher (HL2's crossbow code) and its bolt, which
//			stuns the Hidden. See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "takedamageinfo.h"
#include "gamevars_shared.h"
#include "effect_dispatch_data.h"

#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "basecombatcharacter.h"
	#include "te_effect_dispatch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponFN303 C_WeaponFN303
#endif

#define FN303_BOLT_MODEL			"models/shells/pellet.mdl"
#define FN303_BOLT_SPEED			2000.0f
#define FN303_BOLT_SPEED_WATER		1500.0f
#define FN303_REFIRE_TIME			0.4f

#ifndef CLIENT_DLL

#define FN303_BOLT_NPC_DAMAGE		13
#define FN303_BOLT_OTHER_DAMAGE		3.0f
#define FN303_WEIGHTING				25

//-----------------------------------------------------------------------------
// The bolt (Beta 4b's CFN303Ball)
//-----------------------------------------------------------------------------
class CFN303Bolt : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CFN303Bolt, CBaseCombatCharacter );
	DECLARE_DATADESC();

	CFN303Bolt() { m_iDamage = 0; }

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual bool CreateVPhysics( void );
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;
	virtual Class_T Classify( void ) { return CLASS_NONE; }

	void BoltTouch( CBaseEntity *pOther );
	void BubbleThink( void );

	void SetDamage( int iDamage ) { m_iDamage = iDamage; }

private:
	int m_iDamage;
};

LINK_ENTITY_TO_CLASS( fn303_bolt, CFN303Bolt );

BEGIN_DATADESC( CFN303Bolt )
	DEFINE_FUNCTION( BoltTouch ),
	DEFINE_FUNCTION( BubbleThink ),
	DEFINE_FIELD( m_iDamage, FIELD_INTEGER ),
END_DATADESC()

void CFN303Bolt::Spawn( void )
{
	Precache();

	SetModel( FN303_BOLT_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this, -Vector( 1, 1, 1 ), Vector( 1, 1, 1 ) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.5f );

	UpdateWaterState();

	SetTouch( &CFN303Bolt::BoltTouch );
	SetThink( &CFN303Bolt::BubbleThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CFN303Bolt::Precache( void )
{
	PrecacheModel( FN303_BOLT_MODEL );
}

bool CFN303Bolt::CreateVPhysics( void )
{
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CFN303Bolt::PhysicsSolidMaskForEntity( void ) const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CFN303Bolt::BoltTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		return;

	trace_t tr;
	tr = GetTouchTrace();

	if ( pOther->m_takedamage == DAMAGE_NO )
	{
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			// Hit the world: leave the pellet lying there for a couple of seconds.
			EmitSound( "Weapon_FN303.BoltHitWorld" );

			SetMoveType( MOVETYPE_NONE );
			UTIL_ImpactTrace( &tr, DMG_NERVEGAS );	// Beta 4b's damage type for the impact
			AddEffects( EF_NODRAW );
			SetTouch( NULL );
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 2.0f );
		}
		else
		{
			if ( !( tr.surface.flags & SURF_SKY ) )
				UTIL_ImpactTrace( &tr, DMG_NERVEGAS );

			UTIL_Remove( this );
		}
		return;
	}

	Vector vecDir = GetAbsVelocity();
	VectorNormalize( vecDir );

	ClearMultiDamage();

	CBaseEntity *pOwner = GetOwnerEntity();
	CBasePlayer *pShooter = ToBasePlayer( pOwner );

	if ( pShooter && pOther->IsNPC() )
	{
		CTakeDamageInfo info( this, pOwner, m_iDamage, DMG_NEVERGIB );
		info.AdjustPlayerDamageInflictedForSkillLevel();
		CalculateMeleeDamageForce( &info, vecDir, tr.endpos, 0.7f );
		info.SetDamagePosition( tr.endpos );
		pOther->DispatchTraceAttack( info, vecDir, &tr );
	}
	else
	{
		// Players (and everything else) take a token 3 damage; the stun is the point.
		CTakeDamageInfo info( this, pOwner, FN303_BOLT_OTHER_DAMAGE, DMG_BULLET );
		CalculateMeleeDamageForce( &info, vecDir, tr.endpos, 0.7f );
		info.SetDamagePosition( tr.endpos );
		pOther->DispatchTraceAttack( info, vecDir, &tr );

		CHidden_Player *pVictim = ToHiddenPlayer( pOther );
		if ( !pVictim )
		{
			UTIL_ImpactTrace( &tr, DMG_NERVEGAS );
		}
		else
		{
			const bool bTeammate = pShooter && pVictim->GetTeamNumber() == pShooter->GetTeamNumber();
			if ( ( bTeammate && friendlyfire.GetInt() > 0 ) || pVictim->GetTeamNumber() == TEAM_HIDDEN )
				pVictim->Stun( pShooter );

			// Hitting the Hidden counts towards being picked as the Hidden; hitting a teammate counts against.
			CHidden_Player *pHiddenShooter = ToHiddenPlayer( pShooter );
			if ( pHiddenShooter )
			{
				if ( bTeammate )
				{
					pHiddenShooter->AddWeighting( -FN303_WEIGHTING );
					DevMsg( 1, "303 FF hit, remove 25 from weighting\n" );
				}
				else
				{
					pHiddenShooter->AddWeighting( FN303_WEIGHTING );
					DevMsg( 1, "303 hit, add 25 to weighting\n" );
				}
			}
		}
	}

	ApplyMultiDamage();

	SetAbsVelocity( vec3_origin );
	EmitSound( "Weapon_FN303.BoltHitBody" );

	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}

void CFN303Bolt::BubbleThink( void )
{
	QAngle angNewAngles;
	VectorAngles( GetAbsVelocity(), angNewAngles );
	SetAbsAngles( angNewAngles );

	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( GetWaterLevel() != 0 )
		UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5 );
}

#endif // !CLIENT_DLL

//-----------------------------------------------------------------------------
// The launcher
//-----------------------------------------------------------------------------
class CWeaponFN303 : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponFN303, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponFN303() {}

	virtual void Precache( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void ) {}
	virtual bool Reload( void );
	virtual void WeaponIdle( void );

private:
	void FireBolt( void );

	CWeaponFN303( const CWeaponFN303 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFN303, DT_WeaponFN303 )

BEGIN_NETWORK_TABLE( CWeaponFN303, DT_WeaponFN303 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponFN303 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_fn303, CWeaponFN303 );
PRECACHE_WEAPON_REGISTER( weapon_fn303 );

void CWeaponFN303::Precache( void )
{
#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "fn303_bolt" );
#endif
	PrecacheModel( FN303_BOLT_MODEL );

	BaseClass::Precache();
}

void CWeaponFN303::PrimaryAttack( void )
{
	if ( !m_bDeployed )
		return;

	if ( CheckSafety() )
		return;

	FireBolt();

	// Beta 4b asks for the duration of sequence 163 here, which the viewmodel doesn't have, so the
	// engine's out-of-range answer of 0.1 s is what it gets.
	SetWeaponIdleTime( gpGlobals->curtime + 0.1f );
}

void CWeaponFN303::FireBolt( void )
{
	if ( m_iClip1 <= 0 )
	{
		if ( m_bFireOnEmpty )
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15f;	// an absolute time in Beta 4b, so no real delay
		}
		else
		{
			Reload();
		}
		return;
	}

	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL
	const Vector vecAiming = pPlayer->CBasePlayer::GetAutoaimVector( 0.0f );
	const Vector vecSrc = pPlayer->Weapon_ShootPosition();

	QAngle angAiming;
	VectorAngles( vecAiming, angAiming );

	CFN303Bolt *pBolt = static_cast<CFN303Bolt *>( CreateEntityByName( "fn303_bolt" ) );
	if ( pBolt )
	{
		UTIL_SetOrigin( pBolt, vecSrc );
		pBolt->SetAbsAngles( angAiming );
		pBolt->Spawn();
		pBolt->SetOwnerEntity( pPlayer );
		pBolt->SetDamage( FN303_BOLT_NPC_DAMAGE );
		pBolt->SetAbsVelocity( vecAiming * ( pPlayer->GetWaterLevel() == 3 ? FN303_BOLT_SPEED_WATER : FN303_BOLT_SPEED ) );
	}
#endif

	m_iClip1--;

	pPlayer->ViewPunch( QAngle( -2, 0, 0 ) );

	WeaponSound( SINGLE );
	WeaponSound( SPECIAL2 );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	CheckAmmoDepleted();

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + FN303_REFIRE_TIME;
}

bool CWeaponFN303::Reload( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 )
		return false;

	if ( !DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD ) )
		return false;

	CEffectData data;
	data.m_vOrigin = pPlayer->GetAbsOrigin();
	data.m_vAngles = pPlayer->GetAbsAngles();
	data.m_flScale = 1.0f;
	DispatchEffect( "EjectFN303Clip", data );

	return true;
}

void CWeaponFN303::WeaponIdle( void )
{
	IdleOrLowered();
}
