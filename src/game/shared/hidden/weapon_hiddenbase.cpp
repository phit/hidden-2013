//========= Hidden: Source =====================================================//
//
// Purpose: Base class of the Hidden weapons (Beta 4b's CWeaponSDKBase): safety,
//			magazine reloads and the SDK template's bullet code.
//			See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "ammodef.h"
#include "gamevars_shared.h"
#include "takedamageinfo.h"
#include "effect_dispatch_data.h"
#include "vstdlib/random.h"

#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "te_effect_dispatch.h"
	#include "ilagcompensationmanager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIDDEN_BULLET_RANGE			8000.0f
#define HIDDEN_BULLET_FALLOFF		0.85f	// damage scale per HIDDEN_BULLET_FALLOFF_STEP units travelled
#define HIDDEN_BULLET_FALLOFF_STEP	500.0f

// Mirrored damage (mp_friendlyfire 2) uses this damage type in Beta 4b.
#define HIDDEN_DMG_MIRROR			0x99a

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHiddenBase, DT_WeaponHiddenBase )

BEGIN_NETWORK_TABLE( CWeaponHiddenBase, DT_WeaponHiddenBase )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bDeployed ) ),
#else
	SendPropBool( SENDINFO( m_bDeployed ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponHiddenBase )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bDeployed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

CWeaponHiddenBase::CWeaponHiddenBase()
{
	m_bDeployed = false;
}

#ifdef CLIENT_DLL
bool CWeaponHiddenBase::ShouldDrawPickup( void )
{
	CHidden_Player *pOwner = GetHiddenPlayerOwner();
	if ( pOwner && pOwner->JustSpawned() )
		return false;

	return BaseClass::ShouldDrawPickup();
}
#endif

const CHiddenWeaponInfo &CWeaponHiddenBase::GetHiddenWpnData( void ) const
{
	return static_cast<const CHiddenWeaponInfo &>( GetWpnData() );
}

CHidden_Player *CWeaponHiddenBase::GetHiddenPlayerOwner( void ) const
{
	return ToHiddenPlayer( GetOwner() );
}

bool CWeaponHiddenBase::IsOwnerSafe( void ) const
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	return pPlayer && pPlayer->GetSafe();
}

void CWeaponHiddenBase::SetSafe( void )
{
	SendWeaponAnim( ACT_VM_IDLE_TO_LOWERED );
	SetWeaponIdleTime( gpGlobals->curtime + 5.0f );

#ifndef CLIENT_DLL
	RemoveLaserPointer();
#endif
}

bool CWeaponHiddenBase::Deploy( void )
{
	if ( m_bDeployed )
		return true;

	m_bDeployed = true;

#ifndef CLIENT_DLL
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( pPlayer && !pPlayer->GetSafe() && pPlayer->GetEquipment() == HIDDEN_EQUIPMENT_LASER && pPlayer->LaserIsOn() )
		CreateLaserPointer();
#endif

	return BaseClass::Deploy();
}

bool CWeaponHiddenBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	RemoveLaserPointer();
#endif

	m_bDeployed = false;
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponHiddenBase::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;

	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	// A marine's weapon comes back out when they leave a ladder.
	if ( pPlayer->GetMoveType() != MOVETYPE_LADDER && pPlayer->GetTeamNumber() == TEAM_IRIS )
		Deploy();

#ifndef CLIENT_DLL
	if ( pPlayer->GetSafe() || pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		return;

	// The laser follows the equipment toggle; unlike Deploy, this doesn't check the equipment.
	UpdateLaserPosition();

	if ( pPlayer->LaserIsOn() )
	{
		if ( !m_hLaserDot )
		{
			DevMsg( 1, "Creating laser pointer in ItemPostFrame\n" );
			CreateLaserPointer();
		}
	}
	else
	{
		RemoveLaserPointer();
	}
#endif
}

#ifndef CLIENT_DLL
void CWeaponHiddenBase::UpdateOnRemove( void )
{
	// Beta 4b leaves the dot behind when a weapon goes away with its owner.
	RemoveLaserPointer();
	BaseClass::UpdateOnRemove();
}

void CWeaponHiddenBase::CreateLaserPointer( void )
{
	if ( m_hLaserDot || !GetOwner() )
		return;

	m_hLaserDot = CHiddenLaserDot::Create( GetAbsOrigin(), GetOwner() );
	UpdateLaserPosition();
}

void CWeaponHiddenBase::UpdateLaserPosition( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || !pPlayer->IsAlive() || !pPlayer->LaserIsOn() )
		return;

	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );
	const Vector vecSrc = pPlayer->Weapon_ShootPosition();

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecForward * MAX_TRACE_LENGTH, MASK_SHOT & ~CONTENTS_WINDOW, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( m_hLaserDot )
		m_hLaserDot->SetLaserPosition( tr.endpos, tr.plane.normal );
}

void CWeaponHiddenBase::RemoveLaserPointer( void )
{
	if ( !m_hLaserDot )
		return;

	UTIL_Remove( m_hLaserDot );
	m_hLaserDot = NULL;
}
#endif

void CWeaponHiddenBase::FinishReload( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if ( !pOwner )
		return;

	if ( UsesClipsForAmmo1() )
	{
		m_iClip1 = GetMaxClip1();
		pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}

	if ( UsesClipsForAmmo2() )
	{
		const int iSecondary = MIN( GetMaxClip2() - m_iClip2, pOwner->GetAmmoCount( m_iSecondaryAmmoType ) );
		m_iClip2 += iSecondary;
		pOwner->RemoveAmmo( iSecondary, m_iSecondaryAmmoType );
	}

	if ( m_bReloadsSingly )
		m_bInReload = false;
}

void CWeaponHiddenBase::PlayEmptySound( void )
{
	// Beta 4b plays the rifle click for every weapon, whatever the script says.
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
}

void CWeaponHiddenBase::IdleOrLowered( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetSafe() )
	{
		SendWeaponAnim( GetLoweredActivity() );
		SetWeaponIdleTime( gpGlobals->curtime + 5.0f );
	}
	else if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 5.0f );
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

void CWeaponHiddenBase::DeployLowered( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	pPlayer->SetShotsFired( 0 );

	if ( pPlayer->GetSafe() )
	{
		SendWeaponAnim( GetLoweredActivity() );
		SetWeaponIdleTime( gpGlobals->curtime + 0.1f );
	}
}

void CWeaponHiddenBase::SetHolsterBodygroup( int iValue )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( pPlayer && pPlayer->IsAlive() )
		pPlayer->SetBodygroup( 2, iValue );
}

bool CWeaponHiddenBase::CheckSafety( void )
{
	if ( !IsOwnerSafe() )
		return false;

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	return true;
}

void CWeaponHiddenBase::FireAutomatic( float flSpread )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	pPlayer->SetShotsFired( pPlayer->GetShotsFired() + 1 );

	if ( m_iClip1 <= 0 )
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		}
		return;
	}

	pPlayer->DoMuzzleFlash();
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	m_iClip1--;
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FireHiddenBullets( pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(), flSpread );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetHiddenWpnData().m_flCycleTime;

	CheckAmmoDepleted();
	SetWeaponIdleTime( gpGlobals->curtime + 5.0f );

	const bool bOnGround = ( pPlayer->GetFlags() & FL_ONGROUND ) != 0;
	QAngle angPunch = pPlayer->GetPunchAngle();
	angPunch.x -= SharedRandomInt( "HiddenAutomaticRecoil", bOnGround ? 1 : 5, bOnGround ? 3 : 8 );
	pPlayer->SetPunchAngle( angPunch );
}

void CWeaponHiddenBase::CheckAmmoDepleted( void )
{
#ifndef CLIENT_DLL
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( pPlayer && m_iClip1 == 0 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
#endif
}

bool CWeaponHiddenBase::ReloadMagazine( const char *pszEjectEffect )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 )
		return false;

	if ( !DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD ) )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

	if ( pPlayer->GetFOV() != pPlayer->GetDefaultFOV() )
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.0f );

	CEffectData data;
	data.m_vOrigin = pPlayer->GetAbsOrigin();
	data.m_vAngles = pPlayer->GetAbsAngles();
	data.m_flScale = 1.0f;
	DispatchEffect( pszEjectEffect, data );

	pPlayer->SetShotsFired( 0 );
	return true;
}

void CWeaponHiddenBase::FireHiddenBullets( const QAngle &angShoot, float flSpread )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	const CHiddenWeaponInfo &info = GetHiddenWpnData();

	// FX_FireBullets played the shot on the shooter's client and sent it to everyone else.
	WeaponSound( SINGLE );

#ifndef CLIENT_DLL
	// FX_FireBullets played the third-person fire animation.
	pPlayer->DoAnimationEvent( HIDDEN_ANIMEVENT_FIRE_GUN_PRIMARY );

	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
	pPlayer->NoteWeaponFired();
#endif

	const Vector vecSrc = pPlayer->Weapon_ShootPosition();

	// The same seed on client and server gives the same spread pattern.
	int iSeed = CBaseEntity::GetPredictionRandomSeed() & 255;
	for ( int iBullet = 0; iBullet < info.m_iBullets; iBullet++ )
	{
		RandomSeed( ++iSeed );
		const float x = RandomFloat( -0.5f, 0.5f ) + RandomFloat( -0.5f, 0.5f );
		const float y = RandomFloat( -0.5f, 0.5f ) + RandomFloat( -0.5f, 0.5f );

		FireBullet( pPlayer, vecSrc, angShoot, flSpread, info.m_iDamage, x, y );
	}

#ifndef CLIENT_DLL
	lagcompensation->FinishLagCompensation( pPlayer );
#endif
}

// CSDKPlayer::FireBullet: one trace, damage falling off with distance, no penetration.
void CWeaponHiddenBase::FireBullet( CHidden_Player *pPlayer, const Vector &vecSrc, const QAngle &angShoot,
	float flSpread, int iDamage, float x, float y )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( angShoot, &vecForward, &vecRight, &vecUp );

	Vector vecDir = vecForward + x * flSpread * vecRight + y * flSpread * vecUp;
	VectorNormalize( vecDir );

	const Vector vecEnd = vecSrc + vecDir * HIDDEN_BULLET_RANGE;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( r_visualizetraces.GetBool() )
		DebugDrawLine( tr.startpos, tr.endpos, 255, 0, 0, true, -1.0f );

	// Rifle rounds leave a tracer one time in five.
	const int iAmmoType = m_iPrimaryAmmoType;
	if ( ( iAmmoType == GetAmmoDef()->Index( "AMMO_BULLETS" ) || iAmmoType == GetAmmoDef()->Index( "AMMO_556" ) ) &&
		random->RandomInt( 1, 5 ) == 2 )
	{
		pPlayer->MakeTracer( vecSrc, tr, GetAmmoDef()->TracerType( iAmmoType ) );
	}

	if ( tr.fraction == 1.0f )
		return;

	const float flDamage = iDamage * powf( HIDDEN_BULLET_FALLOFF, tr.fraction * HIDDEN_BULLET_RANGE / HIDDEN_BULLET_FALLOFF_STEP );

	if ( enginetrace->GetPointContents( tr.endpos ) & ( CONTENTS_WATER | CONTENTS_SLIME ) )
	{
		trace_t waterTrace;
		UTIL_TraceLine( vecSrc, tr.endpos, MASK_SHOT | CONTENTS_WATER | CONTENTS_SLIME, pPlayer, COLLISION_GROUP_NONE, &waterTrace );

		if ( !waterTrace.allsolid )
		{
			CEffectData data;
			data.m_vOrigin = waterTrace.endpos;
			data.m_vNormal = waterTrace.plane.normal;
			data.m_flScale = random->RandomFloat( 8.0f, 12.0f );
			if ( waterTrace.contents & CONTENTS_SLIME )
				data.m_fFlags |= FX_WATER_IN_SLIME;

			DispatchEffect( "gunshotsplash", data );
		}
	}
	else if ( !( tr.surface.flags & ( SURF_SKY | SURF_NODRAW | SURF_HINT | SURF_SKIP ) ) )
	{
		// No impact effect on teammates.
		if ( !tr.m_pEnt || !tr.m_pEnt->IsPlayer() || tr.m_pEnt->GetTeamNumber() != pPlayer->GetTeamNumber() )
			UTIL_ImpactTrace( &tr, DMG_BULLET | DMG_NEVERGIB );
	}

	if ( !tr.m_pEnt )
		return;

	ClearMultiDamage();

	CTakeDamageInfo info( pPlayer, pPlayer, flDamage, DMG_BULLET | DMG_NEVERGIB );
	CalculateBulletDamageForce( &info, iAmmoType, vecDir, tr.endpos );
	tr.m_pEnt->DispatchTraceAttack( info, vecDir, &tr );

#ifndef CLIENT_DLL
	pPlayer->TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecDir );
#endif

	ApplyMultiDamage();

#ifndef CLIENT_DLL
	// mp_friendlyfire 2: hitting a teammate hurts the shooter just as much.
	if ( friendlyfire.GetInt() == 2 && tr.m_pEnt->IsPlayer() && tr.m_pEnt->GetTeamNumber() == pPlayer->GetTeamNumber() )
	{
		ClearMultiDamage();

		CTakeDamageInfo mirrorInfo( pPlayer, pPlayer, flDamage, HIDDEN_DMG_MIRROR );
		CalculateBulletDamageForce( &mirrorInfo, iAmmoType, vecDir, tr.endpos );
		pPlayer->DispatchTraceAttack( mirrorInfo, -vecDir, &tr );

		ApplyMultiDamage();
	}
#endif
}
