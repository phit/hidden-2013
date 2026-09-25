//========= Hidden: Source =====================================================//
//
// Purpose: Client side of the Hidden player.
//
//=============================================================================//

#include "cbase.h"
#include "c_hidden_player.h"
#include "c_basetempentity.h"
#include "weapon_hiddenbase.h"

// hidden_player_shared.h maps CHidden_Player to this class, but the network class name below must
// stay the server's.
#undef CHidden_Player

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( player, C_Hidden_Player );

//-----------------------------------------------------------------------------
// Player animation events from the server.
//-----------------------------------------------------------------------------
class C_TEHiddenPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEHiddenPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		C_Hidden_Player *pPlayer = ToHiddenPlayer( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
			pPlayer->DoAnimationEvent( (HiddenPlayerAnimEvent_t)m_iEvent );
	}

	CHandle<C_BasePlayer> m_hPlayer;
	int m_iEvent;
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEHiddenPlayerAnimEvent, DT_TEHiddenPlayerAnimEvent, CTEHiddenPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEHiddenPlayerAnimEvent, DT_TEHiddenPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_Hidden_Player, DT_Hidden_Player, CHidden_Player )
	RecvPropInt( RECVINFO( m_iPlayerClass ) ),
	RecvPropInt( RECVINFO( m_iCharacter ) ),
	RecvPropInt( RECVINFO( m_iPrimary ) ),
	RecvPropInt( RECVINFO( m_iSecondary ) ),
	RecvPropInt( RECVINFO( m_iEquipment ) ),
	RecvPropBool( RECVINFO( m_bNoHidden ) ),
	RecvPropInt( RECVINFO( m_iWeighting ) ),
	RecvPropBool( RECVINFO( m_bSafety ) ),
	RecvPropBool( RECVINFO( m_bZoom ) ),
	RecvPropInt( RECVINFO( m_iShotsFired ) ),
	RecvPropBool( RECVINFO( m_bStunned ) ),
	RecvPropFloat( RECVINFO( m_flBlur ) ),
	RecvPropBool( RECVINFO( m_bLAM ) ),
	RecvPropBool( RECVINFO( m_bNightVision ) ),
	RecvPropInt( RECVINFO( m_iThrowGrenadeCounter ) ),
END_RECV_TABLE()

// Weapons change these in predicted code.
BEGIN_PREDICTION_DATA( C_Hidden_Player )
	DEFINE_PRED_FIELD( m_bZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

C_Hidden_Player::C_Hidden_Player()
{
	m_iPlayerClass = HIDDEN_CLASS_NONE;
	m_iCharacter = -1;
	m_iPrimary = HIDDEN_LOADOUT_NONE;
	m_iSecondary = HIDDEN_LOADOUT_NONE;
	m_iEquipment = HIDDEN_LOADOUT_NONE;
	m_bNoHidden = false;
	m_iWeighting = 0;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;
	m_bStunned = false;
	m_flBlur = 0.0f;
	m_bLAM = false;
	m_bNightVision = false;
	m_iThrowGrenadeCounter = 0;

	m_pHiddenAnimState = CreateHiddenPlayerAnimState( this, this );
}

C_Hidden_Player::~C_Hidden_Player()
{
	m_pHiddenAnimState->Release();
}

void C_Hidden_Player::UpdateClientSideAnimation( void )
{
	// Our own yaw is the view's; everyone else's comes from the server.
	const QAngle angEyes = GetAnimEyeAngles();
	const float flYaw = ( this == C_BasePlayer::GetLocalPlayer() ) ? EyeAngles()[YAW] : angEyes[YAW];
	m_pHiddenAnimState->Update( flYaw, angEyes[PITCH] );

	BaseClass::UpdateClientSideAnimation();
}

const QAngle &C_Hidden_Player::GetRenderAngles( void )
{
	if ( IsRagdoll() )
		return vec3_angle;

	return m_pHiddenAnimState->GetRenderAngles();
}

void C_Hidden_Player::DoAnimationEvent( HiddenPlayerAnimEvent_t event )
{
	// The throw comes through m_iThrowGrenadeCounter.
	if ( event != HIDDEN_ANIMEVENT_THROW_GRENADE )
		m_pHiddenAnimState->DoAnimationEvent( event );
}

C_WeaponHiddenBase *C_Hidden_Player::HiddenAnim_GetActiveWeapon( void )
{
	return dynamic_cast<C_WeaponHiddenBase *>( GetActiveWeapon() );
}

C_Hidden_Player *C_Hidden_Player::GetLocalHiddenPlayer( void )
{
	return ToHiddenPlayer( C_BasePlayer::GetLocalPlayer() );
}
