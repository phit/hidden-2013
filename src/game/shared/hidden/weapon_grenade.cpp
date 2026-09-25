//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden's pipe bomb (Beta 4b's CBaseSDKGrenade and CSDKGrenade) and
//			its grenade_projectile, the SDK template's thrown grenade.
//			See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "basegrenade_shared.h"
#include "in_buttons.h"

#ifndef CLIENT_DLL
	#include "soundent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CHiddenGrenadeProjectile C_HiddenGrenadeProjectile
	#define CWeaponGrenade C_WeaponGrenade
#endif

#define PIPEBOMB_THROWN_MODEL	"models/weapons/pipe/w_pipebomb_thrown.mdl"
#define PIPEBOMB_FUSE			1.5f
#define PIPEBOMB_DAMAGE			90.0f
#define PIPEBOMB_RADIUS_SCALE	3.5f	// radius = damage * 3.5
#define PIPEBOMB_GRAVITY		0.4f
#define PIPEBOMB_FRICTION		0.2f
#define PIPEBOMB_ELASTICITY		0.45f
#define PIPEBOMB_MAX_SPEED		750.0f

//-----------------------------------------------------------------------------
// The thrown pipe bomb (Beta 4b's CBaseGrenadeProjectile)
//-----------------------------------------------------------------------------
class CHiddenGrenadeProjectile : public CBaseGrenade
{
public:
	DECLARE_CLASS( CHiddenGrenadeProjectile, CBaseGrenade );
	DECLARE_NETWORKCLASS();

	CHiddenGrenadeProjectile() {}

#ifdef CLIENT_DLL
	virtual void PostDataUpdate( DataUpdateType_t type );
#else
	DECLARE_DATADESC();

	static CHiddenGrenadeProjectile *Create( const Vector &vecSrc, const QAngle &vecAngles, const Vector &vecVel,
		const AngularImpulse &angImpulse, CBasePlayer *pThrower );

	virtual void Spawn( void );
	virtual void ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );
	virtual float GetShakeAmplitude( void ) { return 0.0f; }	// no screen shake

	void DangerSoundThink( void );
	void SetDetonateTimerLength( float flTimer ) { m_flDetonateTime = gpGlobals->curtime + flTimer; }
	void SetupInitialTransmittedGrenadeVelocity( const Vector &vecVelocity ) { m_vInitialVelocity = vecVelocity; }
#endif

private:
	// Lets the client start the interpolation along the throw instead of from a standstill.
	CNetworkVector( m_vInitialVelocity );

	CHiddenGrenadeProjectile( const CHiddenGrenadeProjectile & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( HiddenGrenadeProjectile, DT_HiddenGrenadeProjectile )

BEGIN_NETWORK_TABLE( CHiddenGrenadeProjectile, DT_HiddenGrenadeProjectile )
#ifdef CLIENT_DLL
	RecvPropVector( RECVINFO( m_vInitialVelocity ) ),
#else
	SendPropVector( SENDINFO( m_vInitialVelocity ), 20, 0, -3000, 3000 ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( grenade_projectile, CHiddenGrenadeProjectile );

#ifdef CLIENT_DLL

void CHiddenGrenadeProjectile::PostDataUpdate( DataUpdateType_t type )
{
	BaseClass::PostDataUpdate( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// Put the initial velocity into the interpolation history.
		CInterpolatedVar<Vector> &interpolator = GetOriginInterpolator();
		interpolator.ClearHistory();

		const float flChangeTime = GetLastChangeTime( LATCH_SIMULATION_VAR );

		// A sample one second back, then the current one.
		Vector vecOrigin = GetLocalOrigin() - m_vInitialVelocity;
		interpolator.AddToHead( flChangeTime - 1.0f, &vecOrigin, false );

		vecOrigin = GetLocalOrigin();
		interpolator.AddToHead( flChangeTime, &vecOrigin, false );
	}
}

#else

BEGIN_DATADESC( CHiddenGrenadeProjectile )
	DEFINE_THINKFUNC( DangerSoundThink ),
END_DATADESC()

CHiddenGrenadeProjectile *CHiddenGrenadeProjectile::Create( const Vector &vecSrc, const QAngle &vecAngles,
	const Vector &vecVel, const AngularImpulse &angImpulse, CBasePlayer *pThrower )
{
	CHiddenGrenadeProjectile *pGrenade = static_cast<CHiddenGrenadeProjectile *>(
		CBaseEntity::Create( "grenade_projectile", vecSrc, vecAngles, pThrower ) );
	if ( !pGrenade )
		return NULL;

	pGrenade->SetModel( PIPEBOMB_THROWN_MODEL );
	pGrenade->SetDetonateTimerLength( PIPEBOMB_FUSE );
	pGrenade->SetAbsVelocity( vecVel );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( vecVel );
	pGrenade->SetThrower( pThrower );

	pGrenade->SetGravity( PIPEBOMB_GRAVITY );
	pGrenade->SetFriction( PIPEBOMB_FRICTION );
	pGrenade->SetElasticity( PIPEBOMB_ELASTICITY );

	pGrenade->SetDamage( PIPEBOMB_DAMAGE );
	pGrenade->SetDamageRadius( PIPEBOMB_DAMAGE * PIPEBOMB_RADIUS_SCALE );
	pGrenade->ChangeTeam( pThrower->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angImpulse );

	pGrenade->SetThink( &CHiddenGrenadeProjectile::DangerSoundThink );
	pGrenade->SetNextThink( gpGlobals->curtime );

	pGrenade->RemoveEffects( EF_NOSHADOW );
	return pGrenade;
}

void CHiddenGrenadeProjectile::Spawn( void )
{
	// Deliberately skip CBaseGrenade::Spawn, as Beta 4b does.
	CBaseEntity::Spawn();

	SetBlocksLOS( false );
	SetSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	SetSolid( SOLID_BBOX );
	SetSize( Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
}

void CHiddenGrenadeProjectile::DangerSoundThink( void )
{
	if ( !IsInWorld() )
	{
		Remove();
		return;
	}

	if ( gpGlobals->curtime > m_flDetonateTime )
	{
		Detonate();
		return;
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5f, GetAbsVelocity().Length(), 0.2f );

	SetNextThink( gpGlobals->curtime + 0.2f );

	if ( GetWaterLevel() != 0 )
		SetAbsVelocity( GetAbsVelocity() * 0.5f );
}

// The SDK template's bounce: breaks glass, bounces off walls, comes to rest on floors.
void CHiddenGrenadeProjectile::ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity )
{
	// Players soak up most of the bounce.
	float flSurfaceElasticity = 1.0f;
	if ( trace.m_pEnt && trace.m_pEnt->IsPlayer() )
		flSurfaceElasticity = 0.3f;

	// Give glass a knock; if it breaks, fly on through, a little slower.
	if ( trace.m_pEnt && ( FClassnameIs( trace.m_pEnt, "func_breakable" ) || FClassnameIs( trace.m_pEnt, "func_breakable_surf" ) ) )
	{
		CTakeDamageInfo info( this, this, 10, DMG_CLUB );
		trace.m_pEnt->DispatchTraceAttack( info, GetAbsVelocity(), &trace );
		ApplyMultiDamage();

		if ( trace.m_pEnt->m_iHealth <= 0 )
		{
			SetAbsVelocity( GetAbsVelocity() * 0.4f );
			return;
		}
	}

	const float flTotalElasticity = clamp( GetElasticity() * flSurfaceElasticity, 0.0f, 0.9f );

	// A backoff of 2 is a reflection.
	Vector vecAbsVelocity;
	PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, 2.0f );
	vecAbsVelocity *= flTotalElasticity;

	// The total velocity, with conveyors and the like.
	VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
	const float flSpeedSqr = DotProduct( vecVelocity, vecVelocity );

	if ( trace.plane.normal.z > 0.7f )
	{
		// A floor: stop once slow enough.
		CBaseEntity *pEntity = trace.m_pEnt;
		SetAbsVelocity( vecAbsVelocity );

		if ( flSpeedSqr < 30.0f * 30.0f )
		{
			if ( pEntity && pEntity->IsStandable() )
				SetGroundEntity( pEntity );

			SetAbsVelocity( vec3_origin );
			SetLocalAngularVelocity( vec3_angle );

			// Lie flat on the ground, turned at random.
			QAngle angle;
			VectorAngles( trace.plane.normal, angle );
			angle[YAW] = random->RandomFloat( 0.0f, 360.0f );
			SetAbsAngles( angle );
		}
		else
		{
			Vector vecBaseDir = GetBaseVelocity();
			VectorNormalize( vecBaseDir );
			const float flScale = ( GetBaseVelocity() - vecAbsVelocity ).Dot( vecBaseDir );

			const float flFrameScale = ( 1.0f - trace.fraction ) * gpGlobals->frametime;
			VectorScale( vecAbsVelocity, flFrameScale, vecVelocity );
			VectorMA( vecVelocity, flFrameScale, GetBaseVelocity() * flScale, vecVelocity );
			PhysicsPushEntity( vecVelocity, &trace );
		}
	}
	else if ( flSpeedSqr < 30.0f * 30.0f )
	{
		// Too slow to escape a wall before gravity pins it there.
		SetAbsVelocity( vec3_origin );
		SetLocalAngularVelocity( vec3_angle );
	}
	else
	{
		SetAbsVelocity( vecAbsVelocity );
	}

	BounceSound();
}

#endif // !CLIENT_DLL

//-----------------------------------------------------------------------------
// The pipe bomb weapon
//-----------------------------------------------------------------------------
class CWeaponGrenade : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponGrenade, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponGrenade();

	virtual void Precache( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual bool Reload( void );
	virtual void ItemPostFrame( void );
	virtual bool AllowsAutoSwitchFrom( void ) const { return !m_bPinPulled; }

	// The pipe bomb isn't lowered between rounds.
	virtual void SetSafe( void ) {}

private:
	void StartGrenadeThrow( void );
	void ThrowGrenade( void );
	void ResetState( void );

	CNetworkVar( bool, m_bRedraw );		// the grenade has been thrown; draw another or switch away
	CNetworkVar( bool, m_bPinPulled );	// holding the pin, waiting for the release
	CNetworkVar( float, m_fThrowTime );	// when the throw animation releases the grenade

	CWeaponGrenade( const CWeaponGrenade & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGrenade, DT_WeaponGrenade )

BEGIN_NETWORK_TABLE( CWeaponGrenade, DT_WeaponGrenade )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bRedraw ) ),
	RecvPropBool( RECVINFO( m_bPinPulled ) ),
	RecvPropFloat( RECVINFO( m_fThrowTime ) ),
#else
	SendPropBool( SENDINFO( m_bRedraw ) ),
	SendPropBool( SENDINFO( m_bPinPulled ) ),
	SendPropFloat( SENDINFO( m_fThrowTime ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponGrenade )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bPinPulled, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fThrowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_grenade, CWeaponGrenade );
PRECACHE_WEAPON_REGISTER( weapon_grenade );

CWeaponGrenade::CWeaponGrenade()
{
	ResetState();
	m_nSkin = 1;
}

void CWeaponGrenade::ResetState( void )
{
	m_bRedraw = false;
	m_bPinPulled = false;
	m_fThrowTime = 0.0f;
}

void CWeaponGrenade::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( PIPEBOMB_THROWN_MODEL );
#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "grenade_projectile" );
#endif
}

bool CWeaponGrenade::Deploy( void )
{
	ResetState();
	return BaseClass::Deploy();
}

bool CWeaponGrenade::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	ResetState();

#ifndef CLIENT_DLL
	// Out of pipe bombs: get rid of the weapon.
	CBaseCombatCharacter *pOwner = GetOwner();
	if ( pOwner && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		pOwner->Weapon_Drop( this );
		UTIL_Remove( this );
	}
#endif

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponGrenade::PrimaryAttack( void )
{
	if ( m_bRedraw || m_bPinPulled )
		return;

	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 || pPlayer->GetSafe() )
		return;

	// Pull the pin; letting go of the button throws.
	SendWeaponAnim( ACT_VM_PULLPIN );
	m_bPinPulled = true;
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
}

void CWeaponGrenade::SecondaryAttack( void )
{
	if ( m_bRedraw )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// Only an animation in Beta 4b.
	SendWeaponAnim( ( pPlayer->GetFlags() & FL_DUCKING ) ? ACT_VM_SECONDARYATTACK : ACT_VM_HAULBACK );

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}

bool CWeaponGrenade::Reload( void )
{
	if ( m_bRedraw && m_flNextPrimaryAttack <= gpGlobals->curtime && m_flNextSecondaryAttack <= gpGlobals->curtime )
	{
		SendWeaponAnim( ACT_VM_DRAW );

		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
	}

	return true;
}

void CWeaponGrenade::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer || !pPlayer->GetViewModel( m_nViewModelIndex ) )
		return;

	if ( m_bPinPulled && !( pPlayer->m_nButtons & IN_ATTACK ) && gpGlobals->curtime > m_flNextPrimaryAttack )
	{
		// Let go of the button: throw.
		pPlayer->SetAnimation( PLAYER_ATTACK1 );
		StartGrenadeThrow();
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
		m_bPinPulled = false;
		SendWeaponAnim( ACT_VM_THROW );
		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
	}
	else if ( m_fThrowTime > 0.0f && m_fThrowTime < gpGlobals->curtime )
	{
		ThrowGrenade();
	}
	else if ( m_bRedraw )
	{
		// Once the throw animation is done, switch away, or drop the weapon if that was the last one.
		if ( m_flTimeWeaponIdle < gpGlobals->curtime )
		{
#ifndef CLIENT_DLL
			if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
			{
				pPlayer->Weapon_Drop( this, NULL, NULL );
				UTIL_Remove( this );
			}
			else
			{
				pPlayer->SwitchToNextBestWeapon( this );
			}
#endif
		}
	}
	else if ( m_flNextPrimaryAttack < gpGlobals->curtime )
	{
		BaseClass::ItemPostFrame();
	}
}

void CWeaponGrenade::StartGrenadeThrow( void )
{
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
	m_fThrowTime = gpGlobals->curtime + 0.1f;
}

void CWeaponGrenade::ThrowGrenade( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// Map the aim pitch to a throw angle: looking straight ahead throws 10 degrees up.
	QAngle angThrow = pPlayer->LocalEyeAngles();
	if ( angThrow.x < 90.0f )
	{
		angThrow.x = -10.0f + angThrow.x * ( ( 90.0f + 10.0f ) / 90.0f );
	}
	else
	{
		angThrow.x = 360.0f - angThrow.x;
		angThrow.x = -10.0f + angThrow.x * -( ( 90.0f - 10.0f ) / 90.0f );
	}

	float flVel = ( 90.0f - angThrow.x ) * 6.0f;
	if ( flVel > PIPEBOMB_MAX_SPEED )
		flVel = PIPEBOMB_MAX_SPEED;

	Vector vecForward, vecRight, vecUp;
	AngleVectors( angThrow, &vecForward, &vecRight, &vecUp );

	Vector vecSrc = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
	vecSrc += vecForward * 16.0f;

	const Vector vecThrow = vecForward * flVel + pPlayer->GetAbsVelocity();

#ifndef CLIENT_DLL
	CHiddenGrenadeProjectile::Create( vecSrc, vec3_angle, vecThrow, AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ), pPlayer );
#endif

	m_bRedraw = true;
	m_fThrowTime = 0.0f;
}
