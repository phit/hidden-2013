//========= Hidden: Source =====================================================//
//
// Purpose: Base class of the Hidden weapons (Beta 4b's CWeaponSDKBase): safety,
//			magazine reloads and the SDK template's bullet code.
//			See docs/spec/weapons.md.
//
//=============================================================================//

#ifndef WEAPON_HIDDENBASE_H
#define WEAPON_HIDDENBASE_H
#pragma once

#include "hidden_player_shared.h"
#include "weapon_hl2mpbase.h"
#include "hidden_weapon_parse.h"
#include "activitylist.h"

#ifndef CLIENT_DLL
	#include "hidden_laserdot.h"
#endif

#ifdef CLIENT_DLL
	#define CWeaponHiddenBase C_WeaponHiddenBase
#endif

class CWeaponHiddenBase : public CWeaponHL2MPBase
{
public:
	DECLARE_CLASS( CWeaponHiddenBase, CWeaponHL2MPBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponHiddenBase();

	const CHiddenWeaponInfo &GetHiddenWpnData( void ) const;
	CHidden_Player *GetHiddenPlayerOwner( void ) const;

	// True while the owner's safety is on (between rounds): weapons are lowered and can't fire.
	bool IsOwnerSafe( void ) const;

	// Lower the weapon when the round ends.
	virtual void SetSafe( void );

	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void ItemPostFrame( void );
#ifndef CLIENT_DLL
	virtual void UpdateOnRemove( void );
#endif

	// A reload always fills the clip and uses up one spare magazine; what was left in the old one
	// is lost. The reserve counts magazines (Beta 4b changed CBaseCombatWeapon::FinishReload).
	virtual void FinishReload( void );

	// For the third-person grenade prime animation.
	virtual bool IsPinPulled( void ) const { return false; }

protected:
	// The 2006 SDK's ACT_VM_LOWERED, which SDK 2013 dropped. The Beta 4b viewmodels still use it.
	static Activity GetLoweredActivity( void ) { return ActivityList_RegisterPrivateActivity( "ACT_VM_LOWERED" ); }

	void PlayEmptySound( void );

	// FX_FireBullets: fire the weapon's "Bullets" from the owner's eyes with the given spread.
	void FireHiddenBullets( const QAngle &angShoot, float flSpread );

	// Run the idle animation or, with safety on, keep the weapon lowered (the firearms' WeaponIdle).
	void IdleOrLowered( void );

	// With safety on, show the lowered weapon on deploy (the firearms' Deploy).
	void DeployLowered( void );

	// Set the owner's holster bodygroup for the weapon being put away.
	void SetHolsterBodygroup( int iValue );

	// With safety on, wait out the current animation instead of firing. Returns true if safe.
	bool CheckSafety( void );

	// The FN2000's and P90's shot: one "CycleTime" apart, aimed at twice the view punch, kicking
	// the view up 1-3 degrees on the ground and 5-8 in the air.
	void FireAutomatic( float flSpread );

	// Tell the owner's suit the ammo has run out.
	void CheckAmmoDepleted( void );

	// The firearms' magazine reload: a new clip and the dropped magazine's client effect.
	bool ReloadMagazine( const char *pszEjectEffect );

	// Set when deployed, cleared when holstered.
	CNetworkVar( bool, m_bDeployed );

private:
#ifndef CLIENT_DLL
	// The laser sight (equipment 0): a dot where the owner aims while their laser is on.
	void CreateLaserPointer( void );
	void UpdateLaserPosition( void );
	void RemoveLaserPointer( void );

	CHandle<CHiddenLaserDot> m_hLaserDot;
#endif

	void FireBullet( CHidden_Player *pPlayer, const Vector &vecSrc, const QAngle &angShoot, float flSpread,
		int iDamage, float x, float y );

	CWeaponHiddenBase( const CWeaponHiddenBase & );
};

#endif // WEAPON_HIDDENBASE_H
