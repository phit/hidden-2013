//========= Hidden: Source =====================================================//
//
// Purpose: The SDK template's pump shotgun, loaded shell by shell.
//			See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponShotgun C_WeaponShotgun
#endif

#define SHOTGUN_SPREAD		0.19f
#define SHOTGUN_CLIP_SIZE	8	// Beta 4b's WeaponIdle checks this number, not the script's clip_size

enum
{
	SHOTGUN_RELOAD_NONE = 0,
	SHOTGUN_RELOAD_STARTED,		// waiting for the start animation or the last shell
	SHOTGUN_RELOAD_LOADING,		// the next Reload() puts a shell in
};

class CWeaponShotgun : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponShotgun, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponShotgun();

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

private:
	CNetworkVar( float, m_flPumpTime );
	CNetworkVar( int, m_iInSpecialReload );

	CWeaponShotgun( const CWeaponShotgun & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponShotgun, DT_WeaponShotgun )

BEGIN_NETWORK_TABLE( CWeaponShotgun, DT_WeaponShotgun )
#ifdef CLIENT_DLL
	RecvPropTime( RECVINFO( m_flPumpTime ) ),
	RecvPropInt( RECVINFO( m_iInSpecialReload ) ),
#else
	SendPropTime( SENDINFO( m_flPumpTime ) ),
	SendPropInt( SENDINFO( m_iInSpecialReload ), 2, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponShotgun )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flPumpTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iInSpecialReload, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_shotgun, CWeaponShotgun );
PRECACHE_WEAPON_REGISTER( weapon_shotgun );

CWeaponShotgun::CWeaponShotgun()
{
	m_flPumpTime = 0.0f;
	m_iInSpecialReload = SHOTGUN_RELOAD_NONE;
}

void CWeaponShotgun::PrimaryAttack( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || !m_bDeployed )
		return;

	if ( pPlayer->GetSafe() )
	{
		// Beta 4b sets an absolute time here, so this doesn't delay anything.
		m_flNextPrimaryAttack = 0.5f;
		return;
	}

	// No firing underwater.
	if ( pPlayer->GetWaterLevel() == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		Reload();
		if ( m_iClip1 == 0 )
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

	FireHiddenBullets( pPlayer->EyeAngles() + 5.0f * pPlayer->GetPunchAngle(), SHOTGUN_SPREAD );

	CheckAmmoDepleted();
	if ( m_iClip1 != 0 )
		m_flPumpTime = gpGlobals->curtime + 0.5f;

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
	m_iInSpecialReload = SHOTGUN_RELOAD_NONE;

	const bool bOnGround = ( pPlayer->GetFlags() & FL_ONGROUND ) != 0;
	QAngle angPunch = pPlayer->GetPunchAngle();
	angPunch.x -= SharedRandomInt( "ShotgunRecoil", bOnGround ? 8 : 18, bOnGround ? 11 : 21 );
	pPlayer->SetPunchAngle( angPunch );
}

bool CWeaponShotgun::Reload( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return true;

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 || m_iClip1 == GetMaxClip1() )
		return true;

	// Don't reload until the last shot is done.
	if ( gpGlobals->curtime < m_flNextPrimaryAttack )
		return true;

	if ( m_iInSpecialReload == SHOTGUN_RELOAD_NONE )
	{
		// Start the reload.
		pPlayer->SetAnimation( PLAYER_RELOAD );
		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_iInSpecialReload = SHOTGUN_RELOAD_STARTED;

		pPlayer->m_flNextAttack = gpGlobals->curtime + 0.5f;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );
	}
	else if ( m_iInSpecialReload == SHOTGUN_RELOAD_STARTED )
	{
		if ( !HasWeaponIdleTimeElapsed() )
			return true;

		// Push a shell in.
		m_iInSpecialReload = SHOTGUN_RELOAD_LOADING;
		SendWeaponAnim( ACT_VM_RELOAD );
		SetWeaponIdleTime( gpGlobals->curtime + 0.6f );
	}
	else
	{
		// The shell is in.
		m_iClip1++;
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
		m_iInSpecialReload = SHOTGUN_RELOAD_STARTED;
	}

	return true;
}

void CWeaponShotgun::WeaponIdle( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_flPumpTime != 0.0f && m_flPumpTime < gpGlobals->curtime )
		m_flPumpTime = 0.0f;

	if ( !HasWeaponIdleTimeElapsed() )
		return;

	const int iAmmo = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );

	if ( m_iClip1 == 0 && m_iInSpecialReload == SHOTGUN_RELOAD_NONE && iAmmo != 0 )
	{
		Reload();
	}
	else if ( m_iInSpecialReload != SHOTGUN_RELOAD_NONE )
	{
		if ( m_iClip1 != SHOTGUN_CLIP_SIZE && iAmmo != 0 )
		{
			Reload();
		}
		else
		{
			// Reload finished: pump.
			SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );
			m_iInSpecialReload = SHOTGUN_RELOAD_NONE;
			SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
		}
	}
	else if ( pPlayer->GetSafe() )
	{
		SendWeaponAnim( GetLoweredActivity() );
		SetWeaponIdleTime( gpGlobals->curtime + 5.0f );
	}
	else
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

bool CWeaponShotgun::Deploy( void )
{
	DeployLowered();
	return BaseClass::Deploy();
}

bool CWeaponShotgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( !GetHiddenPlayerOwner() )
		return false;

	SetHolsterBodygroup( 1 );
	return BaseClass::Holster( pSwitchingTo );
}
