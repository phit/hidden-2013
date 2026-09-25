//========= Hidden: Source =====================================================//
//
// Purpose: Weapon script data: the Beta 4b keys on top of the HL2MP ones.
//			See docs/spec/weapons.md.
//
//=============================================================================//

#ifndef HIDDEN_WEAPON_PARSE_H
#define HIDDEN_WEAPON_PARSE_H
#pragma once

#include "hl2mp_weapon_parse.h"

class CHiddenWeaponInfo : public CHL2MPSWeaponInfo
{
public:
	DECLARE_CLASS_GAMEROOT( CHiddenWeaponInfo, CHL2MPSWeaponInfo );

	CHiddenWeaponInfo();

	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

public:
	int m_iDamage;		// "Damage": per bullet
	int m_iBullets;		// "Bullets": per shot
	float m_flCycleTime;	// "CycleTime": time between shots
	char m_szAnimExtension[16];	// "PlayerAnimationExtension"
};

#endif // HIDDEN_WEAPON_PARSE_H
