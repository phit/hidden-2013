//========= Hidden: Source =====================================================//
//
// Purpose: Third-person player animation: Beta 4b's CSDKPlayerAnimState, the 2006
//			SDK template's, which plays the CS:S animations the player models include
//			(cs_player_shared.mdl) by the weapon's "PlayerAnimationExtension".
//			See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "hidden_playeranimstate.h"
#include "weapon_hiddenbase.h"

#ifdef CLIENT_DLL
	#include "bone_setup.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ANIM_TOPSPEED_WALK			100
#define ANIM_TOPSPEED_RUN			250
#define ANIM_TOPSPEED_RUN_CROUCH	85

// Faster than this plays the run animation.
#define HIDDEN_ANIM_RUN_SPEED		175.0f

#define DEFAULT_IDLE_NAME				"idle_upper_"
#define DEFAULT_CROUCH_IDLE_NAME		"crouch_idle_upper_"
#define DEFAULT_CROUCH_WALK_NAME		"crouch_walk_upper_"
#define DEFAULT_WALK_NAME				"walk_upper_"
#define DEFAULT_RUN_NAME				"run_upper_"

#define DEFAULT_FIRE_IDLE_NAME			"idle_shoot_"
#define DEFAULT_FIRE_CROUCH_NAME		"crouch_idle_shoot_"
#define DEFAULT_FIRE_CROUCH_WALK_NAME	"crouch_walk_shoot_"
#define DEFAULT_FIRE_WALK_NAME			"walk_shoot_"
#define DEFAULT_FIRE_RUN_NAME			"run_shoot_"

#define FIRESEQUENCE_LAYER		( AIMSEQUENCE_LAYER + NUM_AIMSEQUENCE_LAYERS )
#define RELOADSEQUENCE_LAYER	( FIRESEQUENCE_LAYER + 1 )
#define GRENADESEQUENCE_LAYER	( RELOADSEQUENCE_LAYER + 1 )
#define NUM_LAYERS_WANTED		( GRENADESEQUENCE_LAYER + 1 )

class CHiddenPlayerAnimState : public CBasePlayerAnimState, public IHiddenPlayerAnimState
{
public:
	DECLARE_CLASS( CHiddenPlayerAnimState, CBasePlayerAnimState );

	CHiddenPlayerAnimState();

	void Init( CBaseAnimatingOverlay *pPlayer, IHiddenPlayerAnimStateHelpers *pHelpers );

	virtual void DoAnimationEvent( HiddenPlayerAnimEvent_t event );
	virtual bool IsThrowingGrenade( void );
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle );
	virtual void ClearAnimationState( void );
	virtual bool CanThePlayerMove( void );
	virtual float GetCurrentMaxGroundSpeed( void );
	virtual Activity CalcMainActivity( void );
	virtual void DebugShowAnimState( int iStartLine );
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
	virtual void ClearAnimationLayers( void );

private:
	int CalcFireLayerSequence( HiddenPlayerAnimEvent_t event );
	void ComputeFireSequence( CStudioHdr *pStudioHdr );

	void ComputeReloadSequence( CStudioHdr *pStudioHdr );
	int CalcReloadLayerSequence( void );

	bool IsOuterGrenadePrimed( void );
	void ComputeGrenadeSequence( CStudioHdr *pStudioHdr );
	int CalcGrenadePrimeSequence( void );
	int CalcGrenadeThrowSequence( void );

	const char *GetWeaponSuffix( void );
	bool HandleJumping( void );

	void UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd );

	// Jumping (nothing sends the jump event in Beta 4b, so this never plays)
	bool m_bJumping;
	float m_flJumpStartTime;
	bool m_bFirstJumpFrame;

	// Reload layer
	bool m_bReloading;
	float m_flReloadCycle;
	int m_iReloadSequence;

	// Fire layer: plays each fire animation to the end.
	bool m_bFiring;
	int m_iFireSequence;
	float m_flFireCycle;

	// Grenade layer: priming is watched on the weapon, the throw on the player's counter.
	bool m_bThrowingGrenade;
	bool m_bPrimingGrenade;
	float m_flGrenadeCycle;
	int m_iGrenadeSequence;
	int m_iLastThrowGrenadeCounter;

	IHiddenPlayerAnimStateHelpers *m_pHelpers;
};

IHiddenPlayerAnimState *CreateHiddenPlayerAnimState( CBaseAnimatingOverlay *pEntity, IHiddenPlayerAnimStateHelpers *pHelpers )
{
	CHiddenPlayerAnimState *pState = new CHiddenPlayerAnimState;
	pState->Init( pEntity, pHelpers );
	return pState;
}

CHiddenPlayerAnimState::CHiddenPlayerAnimState()
{
	m_pOuter = NULL;
	m_pHelpers = NULL;
	m_bJumping = false;
	m_flJumpStartTime = 0.0f;
	m_bFirstJumpFrame = false;
	m_bReloading = false;
	m_flReloadCycle = 0.0f;
	m_iReloadSequence = -1;
	m_bFiring = false;
	m_iFireSequence = -1;
	m_flFireCycle = 0.0f;
	m_bThrowingGrenade = false;
	m_bPrimingGrenade = false;
	m_flGrenadeCycle = 0.0f;
	m_iGrenadeSequence = -1;
	m_iLastThrowGrenadeCounter = 0;
}

void CHiddenPlayerAnimState::Init( CBaseAnimatingOverlay *pEntity, IHiddenPlayerAnimStateHelpers *pHelpers )
{
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 90;
	config.m_LegAnimType = LEGANIM_9WAY;
	config.m_bUseAimSequences = true;

	m_pHelpers = pHelpers;

	BaseClass::Init( pEntity, config );
}

void CHiddenPlayerAnimState::ClearAnimationState( void )
{
	m_bJumping = false;
	m_bFiring = false;
	m_bReloading = false;
	m_bThrowingGrenade = m_bPrimingGrenade = false;
	m_iLastThrowGrenadeCounter = m_pHelpers->HiddenAnim_GetThrowGrenadeCounter();

	BaseClass::ClearAnimationState();
}

void CHiddenPlayerAnimState::DoAnimationEvent( HiddenPlayerAnimEvent_t event )
{
	switch ( event )
	{
	case HIDDEN_ANIMEVENT_FIRE_GUN_PRIMARY:
	case HIDDEN_ANIMEVENT_FIRE_GUN_SECONDARY:
		// Restart the fire layer whatever it's doing.
		m_flFireCycle = 0;
		m_iFireSequence = CalcFireLayerSequence( event );
		m_bFiring = ( m_iFireSequence != -1 );
		break;

	case HIDDEN_ANIMEVENT_JUMP:
		m_bJumping = true;
		m_bFirstJumpFrame = true;
		m_flJumpStartTime = gpGlobals->curtime;
		break;

	case HIDDEN_ANIMEVENT_RELOAD:
		m_iReloadSequence = CalcReloadLayerSequence();
		if ( m_iReloadSequence != -1 )
		{
			m_bReloading = true;
			m_flReloadCycle = 0;
		}
		break;

	default:
		break;	// the grenade throw arrives through the counter
	}
}

bool CHiddenPlayerAnimState::IsThrowingGrenade( void )
{
	if ( m_bThrowingGrenade )
		return m_flGrenadeCycle < 0.25f;

	const bool bThrowPending = ( m_iLastThrowGrenadeCounter != m_pHelpers->HiddenAnim_GetThrowGrenadeCounter() );
	return bThrowPending || IsOuterGrenadePrimed();
}

int CHiddenPlayerAnimState::CalcReloadLayerSequence( void )
{
	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix || !m_pHelpers->HiddenAnim_GetActiveWeapon() )
		return -1;

	char szName[512];
	Q_snprintf( szName, sizeof( szName ), "reload_%s", pSuffix );
	int iReloadSequence = m_pOuter->LookupSequence( szName );
	if ( iReloadSequence != -1 )
		return iReloadSequence;

	// Fall back to the M4's.
	iReloadSequence = CalcSequenceIndex( "reload_m4" );
	if ( iReloadSequence > 0 )
		return iReloadSequence;

	return -1;
}

#ifdef CLIENT_DLL
void CHiddenPlayerAnimState::UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd )
{
	if ( !bEnabled )
		return;

	flCurCycle += m_pOuter->GetSequenceCycleRate( pStudioHdr, iSequence ) * gpGlobals->frametime;
	if ( flCurCycle > 1 )
	{
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	C_AnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iLayer );
	pLayer->m_flCycle = flCurCycle;
	pLayer->m_nSequence = iSequence;
	pLayer->m_flPlaybackRate = 1.0;
	pLayer->m_flWeight = 1.0f;
	pLayer->m_nOrder = iLayer;
}
#endif

bool CHiddenPlayerAnimState::IsOuterGrenadePrimed( void )
{
	CWeaponHiddenBase *pWeapon = m_pHelpers->HiddenAnim_GetActiveWeapon();
	return pWeapon && pWeapon->IsPinPulled();
}

void CHiddenPlayerAnimState::ComputeGrenadeSequence( CStudioHdr *pStudioHdr )
{
#ifdef CLIENT_DLL
	if ( m_bThrowingGrenade )
	{
		UpdateLayerSequenceGeneric( pStudioHdr, GRENADESEQUENCE_LAYER, m_bThrowingGrenade, m_flGrenadeCycle, m_iGrenadeSequence, false );
		return;
	}

	// Priming isn't an event: watch the weapon. A pending throw plays the prime animation first.
	const bool bThrowPending = ( m_iLastThrowGrenadeCounter != m_pHelpers->HiddenAnim_GetThrowGrenadeCounter() );
	if ( !IsOuterGrenadePrimed() && !bThrowPending )
	{
		m_bPrimingGrenade = false;
		return;
	}

	if ( !m_bPrimingGrenade )
	{
		// Someone who just came into the PVS with the pin pulled is already primed.
		m_flGrenadeCycle = ( TimeSinceLastAnimationStateClear() < 0.4f ) ? 1.0f : 0.0f;
		m_iGrenadeSequence = CalcGrenadePrimeSequence();
	}

	m_bPrimingGrenade = true;
	UpdateLayerSequenceGeneric( pStudioHdr, GRENADESEQUENCE_LAYER, m_bPrimingGrenade, m_flGrenadeCycle, m_iGrenadeSequence, true );

	if ( bThrowPending && m_flGrenadeCycle == 1 )
	{
		m_iLastThrowGrenadeCounter = m_pHelpers->HiddenAnim_GetThrowGrenadeCounter();

		m_iGrenadeSequence = CalcGrenadeThrowSequence();
		if ( m_iGrenadeSequence != -1 )
		{
			m_bThrowingGrenade = true;
			m_bPrimingGrenade = false;
			m_flGrenadeCycle = 0;
		}
	}
#endif
}

int CHiddenPlayerAnimState::CalcGrenadePrimeSequence( void )
{
	return CalcSequenceIndex( "idle_shoot_gren1" );
}

int CHiddenPlayerAnimState::CalcGrenadeThrowSequence( void )
{
	return CalcSequenceIndex( "idle_shoot_gren2" );
}

void CHiddenPlayerAnimState::ComputeReloadSequence( CStudioHdr *pStudioHdr )
{
#ifdef CLIENT_DLL
	UpdateLayerSequenceGeneric( pStudioHdr, RELOADSEQUENCE_LAYER, m_bReloading, m_flReloadCycle, m_iReloadSequence, false );
#endif
}

int CHiddenPlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return 0;

	if ( bForceIdle )
	{
		if ( GetCurrentMainSequenceActivity() == ACT_CROUCHIDLE )
			return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_IDLE_NAME, pSuffix );

		return CalcSequenceIndex( "%s%s", DEFAULT_IDLE_NAME, pSuffix );
	}

	switch ( GetCurrentMainSequenceActivity() )
	{
	case ACT_RUN:
		return CalcSequenceIndex( "%s%s", DEFAULT_RUN_NAME, pSuffix );

	case ACT_WALK:
	case ACT_RUNTOIDLE:
	case ACT_IDLETORUN:
		return CalcSequenceIndex( "%s%s", DEFAULT_WALK_NAME, pSuffix );

	case ACT_CROUCHIDLE:
		return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_IDLE_NAME, pSuffix );

	case ACT_RUN_CROUCH:
		return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_WALK_NAME, pSuffix );

	case ACT_IDLE:
	default:
		return CalcSequenceIndex( "%s%s", DEFAULT_IDLE_NAME, pSuffix );
	}
}

const char *CHiddenPlayerAnimState::GetWeaponSuffix( void )
{
	CWeaponHiddenBase *pWeapon = m_pHelpers->HiddenAnim_GetActiveWeapon();
	if ( !pWeapon )
		return "Pistol";

	return pWeapon->GetHiddenWpnData().m_szAnimExtension;
}

int CHiddenPlayerAnimState::CalcFireLayerSequence( HiddenPlayerAnimEvent_t event )
{
	if ( !m_pHelpers->HiddenAnim_GetActiveWeapon() )
		return 0;

	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return 0;

	// The weapon has usually been switched by the time a throw event arrives.
	if ( event == HIDDEN_ANIMEVENT_THROW_GRENADE )
		pSuffix = "Gren";

	switch ( GetCurrentMainSequenceActivity() )
	{
	case ACT_PLAYER_RUN_FIRE:
	case ACT_RUN:
		return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_RUN_NAME, pSuffix );

	case ACT_PLAYER_WALK_FIRE:
	case ACT_WALK:
		return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_WALK_NAME, pSuffix );

	case ACT_PLAYER_CROUCH_FIRE:
	case ACT_CROUCHIDLE:
		return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_CROUCH_NAME, pSuffix );

	case ACT_PLAYER_CROUCH_WALK_FIRE:
	case ACT_RUN_CROUCH:
		return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_CROUCH_WALK_NAME, pSuffix );

	case ACT_PLAYER_IDLE_FIRE:
	default:
		return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_IDLE_NAME, pSuffix );
	}
}

bool CHiddenPlayerAnimState::CanThePlayerMove( void )
{
	return m_pHelpers->HiddenAnim_CanMove();
}

float CHiddenPlayerAnimState::GetCurrentMaxGroundSpeed( void )
{
	const Activity currentActivity = m_pOuter->GetSequenceActivity( m_pOuter->GetSequence() );
	if ( currentActivity == ACT_WALK || currentActivity == ACT_IDLE )
		return ANIM_TOPSPEED_WALK;
	if ( currentActivity == ACT_RUN )
		return ANIM_TOPSPEED_RUN;
	if ( currentActivity == ACT_RUN_CROUCH )
		return ANIM_TOPSPEED_RUN_CROUCH;
	return 0;
}

bool CHiddenPlayerAnimState::HandleJumping( void )
{
	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();
		}

		// The on-ground flag can still be set right when the event comes in, so wait a moment.
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f && ( m_pOuter->GetFlags() & FL_ONGROUND ) )
		{
			m_bJumping = false;
			RestartMainSequence();
		}
	}

	return m_bJumping;
}

Activity CHiddenPlayerAnimState::CalcMainActivity( void )
{
	if ( HandleJumping() )
		return ACT_HOP;

	const float flOuterSpeed = GetOuterXYSpeed();
	const bool bMoving = ( flOuterSpeed > MOVING_MINIMUM_SPEED );

	if ( m_pOuter->GetFlags() & FL_DUCKING )
		return bMoving ? ACT_RUN_CROUCH : ACT_CROUCHIDLE;

	if ( !bMoving )
		return ACT_IDLE;

	return ( flOuterSpeed > HIDDEN_ANIM_RUN_SPEED ) ? ACT_RUN : ACT_WALK;
}

void CHiddenPlayerAnimState::DebugShowAnimState( int iStartLine )
{
#ifdef CLIENT_DLL
	engine->Con_NPrintf( iStartLine++, "fire  : %s, cycle: %.2f\n", m_bFiring ? GetSequenceName( m_pOuter->GetModelPtr(), m_iFireSequence ) : "[not firing]", m_flFireCycle );
	engine->Con_NPrintf( iStartLine++, "reload: %s, cycle: %.2f\n", m_bReloading ? GetSequenceName( m_pOuter->GetModelPtr(), m_iReloadSequence ) : "[not reloading]", m_flReloadCycle );
	BaseClass::DebugShowAnimState( iStartLine );
#endif
}

void CHiddenPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	BaseClass::ComputeSequences( pStudioHdr );

	ComputeFireSequence( pStudioHdr );
	ComputeReloadSequence( pStudioHdr );
	ComputeGrenadeSequence( pStudioHdr );
}

void CHiddenPlayerAnimState::ClearAnimationLayers( void )
{
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( NUM_LAYERS_WANTED );
	for ( int i = 0; i < m_pOuter->GetNumAnimOverlays(); i++ )
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
}

void CHiddenPlayerAnimState::ComputeFireSequence( CStudioHdr *pStudioHdr )
{
#ifdef CLIENT_DLL
	UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false );
#endif
}
