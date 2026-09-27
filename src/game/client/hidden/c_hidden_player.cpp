//========= Hidden: Source =====================================================//
//
// Purpose: Client side of the Hidden player.
//
//=============================================================================//

#include "cbase.h"
#include "c_hidden_player.h"
#include "c_basetempentity.h"
#include "weapon_hiddenbase.h"
#include "hidden_auratrail.h"
#include "in_buttons.h"

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
	RecvPropFloat( RECVINFO( m_flStamina ) ),
	RecvPropBool( RECVINFO( m_bClinging ) ),
	RecvPropBool( RECVINFO( m_bAura ) ),
	RecvPropBool( RECVINFO( m_bRequestAmmo ) ),
	RecvPropBool( RECVINFO( m_bRevealed ) ),
	RecvPropInt( RECVINFO( m_iSpeedMode ) ),
	RecvPropBool( RECVINFO( m_bWalking ) ),
	RecvPropFloat( RECVINFO( m_flBoostEnd ) ),
	RecvPropInt( RECVINFO( m_iThrowGrenadeCounter ) ),
	RecvPropString( RECVINFO( m_szCurrentLocation ) ),
END_RECV_TABLE()

// Weapons change these in predicted code.
BEGIN_PREDICTION_DATA( C_Hidden_Player )
	DEFINE_PRED_FIELD( m_bZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flStamina, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bClinging, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAura, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iSpeedMode, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bWalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flBoostEnd, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
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
	m_flStamina = 0.0f;
	m_bClinging = false;
	m_iSpeedMode = HIDDEN_SPEED_RUN;
	m_bWalking = false;
	m_flBoostEnd = 0.0f;
	m_bAura = false;
	m_bRequestAmmo = false;
	m_bRevealed = false;
	m_iThrowGrenadeCounter = 0;
	m_szCurrentLocation[0] = '\0';

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

void C_Hidden_Player::ItemPostFrame( void )
{
	// Predict the Hidden's stamina, as CHidden_Player::ItemPostFrame does.
	if ( GetTeamNumber() == TEAM_HIDDEN && m_flStamina < HIDDEN_STAMINA_MAX && GetGroundEntity() != NULL && !m_bAura )
		SetStamina( HIDDEN_STAMINA_REGEN );

	if ( m_bAura )
	{
		SetStamina( -HIDDEN_AURA_STAMINA );
		if ( m_flStamina < 1.0f )
			m_bAura = false;
	}

	BaseClass::ItemPostFrame();
}

void C_Hidden_Player::PreThink( void )
{
	// Predict the aura, as CHidden_Player::PreThink does.
	if ( GetTeamNumber() == TEAM_HIDDEN && ( ( m_afButtonPressed | m_afButtonReleased ) & IN_GRENADE1 ) )
		m_bAura = ( m_afButtonPressed & IN_GRENADE1 ) != 0;

	BaseClass::PreThink();
}

bool C_Hidden_Player::IsAuraActive( void )
{
	return m_bAura && GetLocalVelocity().Length() <= HIDDEN_AURA_MAX_SPEED;
}

void C_Hidden_Player::ClientThink( void )
{
	BaseClass::ClientThink();

	// Everyone leaves an aura trail, which only the Hidden ever sees, so only a living Hidden's
	// client makes them.
	C_Hidden_Player *pLocal = GetLocalHiddenPlayer();
	if ( pLocal && pLocal != this && pLocal->IsAlive() && pLocal->GetTeamNumber() == TEAM_HIDDEN && !IsDormant() )
	{
		if ( !m_pAuraEmitter )
			m_pAuraEmitter = CHiddenAuraEmitter::Create( this );

		m_pAuraEmitter->Emit( gpGlobals->frametime );
	}
}

// The Hidden casts no shadow.
ShadowType_t C_Hidden_Player::ShadowCastType( void )
{
	if ( GetTeamNumber() == TEAM_HIDDEN )
		return SHADOWS_NONE;

	return BaseClass::ShadowCastType();
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
