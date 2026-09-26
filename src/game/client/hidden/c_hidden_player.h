//========= Hidden: Source =====================================================//
//
// Purpose: Client side of the Hidden player.
//
//=============================================================================//

#ifndef C_HIDDEN_PLAYER_H
#define C_HIDDEN_PLAYER_H
#pragma once

#include "c_hl2mp_player.h"
#include "hidden_shareddefs.h"
#include "hidden_playeranimstate.h"
#include "tier1/smartptr.h"

class CHiddenAuraEmitter;

class C_Hidden_Player : public C_HL2MP_Player, public IHiddenPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( C_Hidden_Player, C_HL2MP_Player );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_Hidden_Player();
	~C_Hidden_Player();

	static C_Hidden_Player *GetLocalHiddenPlayer( void );

	int GetPlayerClass( void ) const { return m_iPlayerClass; }
	int GetCharacter( void ) const { return m_iCharacter; }
	int GetPrimary( void ) const { return m_iPrimary; }
	int GetSecondary( void ) const { return m_iSecondary; }
	int GetEquipment( void ) const { return m_iEquipment; }
	bool GetNoHidden( void ) const { return m_bNoHidden; }
	int GetWeighting( void ) const { return m_iWeighting; }

	bool GetSafe( void ) const { return m_bSafety; }
	bool GetZoom( void ) const { return m_bZoom; }
	void SetZoom( bool bZoom ) { m_bZoom = bZoom; }
	int GetShotsFired( void ) const { return m_iShotsFired; }
	void SetShotsFired( int iShots ) { m_iShotsFired = iShots; }
	bool IsStunned( void ) const { return m_bStunned; }
	float GetBlur( void ) const { return m_flBlur; }
	bool LaserIsOn( void ) const { return m_bLAM; }
	bool NightVisionEnabled( void ) const { return m_bNightVision; }
	const char *GetCurrentLocation( void ) const { return m_szCurrentLocation; }

	// Stamina and wall cling, predicted with the movement; see CHidden_Player.
	float GetStamina( void ) const { return m_flStamina; }
	void SetStamina( float flDelta ) { const float flNew = m_flStamina + flDelta; if ( flNew >= 0.0f && flNew <= HIDDEN_STAMINA_MAX ) m_flStamina = flNew; }
	bool IsClinging( void ) const { return m_bClinging; }
	void SetClinging( bool bClinging ) { m_bClinging = bClinging; }
	virtual void ItemPostFrame( void );
	virtual void PreThink( void );

	// The aura: on while the vision key is held; it only shows anything while standing still.
	bool IsViewing( void ) const { return m_bAura; }
	bool IsAuraActive( void );

	virtual void ClientThink( void );

	virtual ShadowType_t ShadowCastType( void );

	// Third-person animation (Beta 4b's SDK template anim state)
	virtual void UpdateClientSideAnimation( void );
	virtual const QAngle &GetRenderAngles( void );
	void DoAnimationEvent( HiddenPlayerAnimEvent_t event );
	virtual C_WeaponHiddenBase *HiddenAnim_GetActiveWeapon( void );
	virtual bool HiddenAnim_CanMove( void ) { return true; }
	virtual int HiddenAnim_GetThrowGrenadeCounter( void ) { return m_iThrowGrenadeCounter; }

private:
	C_Hidden_Player( const C_Hidden_Player & );

	int m_iPlayerClass;
	int m_iCharacter;
	int m_iPrimary;
	int m_iSecondary;
	int m_iEquipment;
	bool m_bNoHidden;
	int m_iWeighting;
	bool m_bSafety;
	bool m_bZoom;
	int m_iShotsFired;
	bool m_bStunned;
	float m_flBlur;
	bool m_bLAM;
	bool m_bNightVision;
	float m_flStamina;
	bool m_bClinging;
	bool m_bAura;
	bool m_bRequestAmmo;

	CSmartPtr<CHiddenAuraEmitter> m_pAuraEmitter;
	int m_iThrowGrenadeCounter;
	char m_szCurrentLocation[HIDDEN_LOCATION_LENGTH];

	IHiddenPlayerAnimState *m_pHiddenAnimState;
};

inline C_Hidden_Player *ToHiddenPlayer( C_BaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<C_Hidden_Player *>( pEntity );
}

#endif // C_HIDDEN_PLAYER_H
