//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden player: marine class and character, loadout, team setup
//			and Hidden selection state. See docs/spec/teams-classes.md.
//
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "hidden_player.h"
#include "hidden_gamerules.h"
#include "hidden_cvars.h"
#include "team.h"
#include "weapon_hiddenbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIDDEN_MODEL_MARINE			"models/player/iris.mdl"
#define HIDDEN_MODEL_MARINE_SUPPORT	"models/player/iris_supply.mdl"
#define HIDDEN_MODEL_HIDDEN			"models/manor/mn_fixture1.mdl"	// the cloaked Hidden, see docs/spec/client.md
#define HIDDEN_MODEL_HIDDEN_RAGDOLL	"models/player/hidden.mdl"

#define HIDDEN_MARINE_SPEED			180.0f
#define HIDDEN_HIDDEN_SPEED			220.0f
#define HIDDEN_HIDDEN_FOV			110

#define HIDDEN_BOOST_SPEED			250.0f
#define HIDDEN_MARINE_WALK_SPEED	120.0f
#define HIDDEN_HIDDEN_WALK_SPEED	160.0f
#define HIDDEN_BOOST_TIME			10.0f
#define HIDDEN_BOOST_CHARGES		3

LINK_ENTITY_TO_CLASS( player, CHidden_Player );

//-----------------------------------------------------------------------------
// Player animation events (the SDK template's), sent to the clients that can see the player.
//-----------------------------------------------------------------------------
class CTEHiddenPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEHiddenPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEHiddenPlayerAnimEvent( const char *pszName ) : CBaseTempEntity( pszName ) {}

	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEHiddenPlayerAnimEvent, DT_TEHiddenPlayerAnimEvent )
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( HIDDEN_ANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

static CTEHiddenPlayerAnimEvent g_TEHiddenPlayerAnimEvent( "HiddenPlayerAnimEvent" );

static void TE_HiddenPlayerAnimEvent( CBasePlayer *pPlayer, HiddenPlayerAnimEvent_t event )
{
	CPVSFilter filter( pPlayer->EyePosition() );

	g_TEHiddenPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEHiddenPlayerAnimEvent.m_iEvent = event;
	g_TEHiddenPlayerAnimEvent.Create( filter, 0 );
}

LINK_ENTITY_TO_CLASS( info_marine_spawn, CPointEntity );
LINK_ENTITY_TO_CLASS( info_hidden_spawn, CPointEntity );

// The last spawn point used per team; spawning walks them round-robin, as in Beta 4b.
static EHANDLE g_hLastMarineSpawn;
static EHANDLE g_hLastHiddenSpawn;

IMPLEMENT_SERVERCLASS_ST( CHidden_Player, DT_Hidden_Player )
	// The clients animate players themselves, as the SDK template does (HL2MP already leaves out
	// the pose parameters).
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_ServerAnimationData", "m_flCycle" ),
	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
	SendPropInt( SENDINFO( m_iPlayerClass ), 3 ),
	SendPropInt( SENDINFO( m_iCharacter ), 5 ),
	SendPropInt( SENDINFO( m_iPrimary ), 5 ),
	SendPropInt( SENDINFO( m_iSecondary ), 5 ),
	SendPropInt( SENDINFO( m_iEquipment ), 5 ),
	SendPropBool( SENDINFO( m_bNoHidden ) ),
	SendPropInt( SENDINFO( m_iWeighting ), 16 ),
	SendPropBool( SENDINFO( m_bSafety ) ),
	SendPropBool( SENDINFO( m_bZoom ) ),
	SendPropInt( SENDINFO( m_iShotsFired ), 8, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bStunned ) ),
	SendPropFloat( SENDINFO( m_flBlur ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bLAM ) ),
	SendPropBool( SENDINFO( m_bNightVision ) ),
	SendPropFloat( SENDINFO( m_flStamina ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bClinging ) ),
	SendPropBool( SENDINFO( m_bAura ) ),
	SendPropInt( SENDINFO( m_iThrowGrenadeCounter ), HIDDEN_THROWGRENADE_COUNTER_BITS, SPROP_UNSIGNED ),
	SendPropString( SENDINFO( m_szCurrentLocation ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CHidden_Player )
END_DATADESC()

CHidden_Player::CHidden_Player()
{
	m_iPlayerClass = HIDDEN_CLASS_NONE;
	m_iCharacter = -1;
	m_iPrimary = HIDDEN_LOADOUT_NONE;
	m_iSecondary = HIDDEN_LOADOUT_NONE;
	m_iEquipment = HIDDEN_LOADOUT_NONE;
	m_bNoHidden = false;
	m_iWeighting = 0;
	m_bReadyToPlay = false;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;
	m_bHadHidden = false;
	m_bLAM = false;
	m_bNightVision = false;
	m_iBoostCount = 0;
	m_flBoostTimer = 0.0f;
	m_bBoosted = false;
	m_iThrowGrenadeCounter = 0;
	m_iSpawnTime = 0;
	m_bSpawnQueued = false;
	m_szCurrentLocation.GetForModify()[0] = '\0';
	ResetStun();

	// The animation runs on the clients; the server keeps its own for the hitboxes.
	m_pHiddenAnimState = CreateHiddenPlayerAnimState( this, this );
	UseClientSideAnimation();
}

CHidden_Player::~CHidden_Player()
{
	m_pHiddenAnimState->Release();
}

void CHidden_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( HIDDEN_MODEL_MARINE );
	PrecacheModel( HIDDEN_MODEL_MARINE_SUPPORT );
	PrecacheModel( HIDDEN_MODEL_HIDDEN );
	PrecacheModel( HIDDEN_MODEL_HIDDEN_RAGDOLL );

	PrecacheScriptSound( "IRIS.DeployNV" );
	PrecacheScriptSound( "IRIS.UnDeployNV" );
}

void CHidden_Player::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	// New players watch until they pick a class and character and the next round starts.
	// Setting the team first also stops CHL2MP_Player::Spawn from picking one.
	ChangeTeam( TEAM_SPECTATOR );
}

void CHidden_Player::Spawn( void )
{
	m_iWeighting = 0;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;
	m_bNightVision = false;
	m_bLAM = false;
	m_iBoostCount = 0;
	m_flStamina = 0.0f;
	m_bClinging = false;
	m_bAura = false;
	m_bWalking = false;
	ResetStun();

	const int iTeam = GetTeamNumber();
	const bool bMarine = ( iTeam == TEAM_IRIS && HasValidCharacter() && m_iPlayerClass != HIDDEN_CLASS_NONE );
	const bool bPlaying = m_bReadyToPlay && ( bMarine || iTeam == TEAM_HIDDEN );

	// The model decides the hull, so set it before the base spawn. Players who sit out still need a
	// valid model: the client's player code expects one.
	if ( bPlaying && iTeam == TEAM_HIDDEN )
		SetModel( HIDDEN_MODEL_HIDDEN );
	else if ( bPlaying && m_iPlayerClass == HIDDEN_CLASS_SUPPORT )
		SetModel( HIDDEN_MODEL_MARINE_SUPPORT );
	else
		SetModel( HIDDEN_MODEL_MARINE );

	BaseClass::Spawn();

	if ( iTeam == TEAM_SPECTATOR )
		return;	// CBasePlayer::Spawn already started observer mode

	if ( !bPlaying )
	{
		BecomeObserver();
		return;
	}

	// Drop what CHL2MP_Player::Spawn gave out and hand out our own.
	RemoveAllItems( false );
	EquipSuit();

	if ( iTeam == TEAM_HIDDEN )
		SetupHidden();
	else
		SetupMarine();
}

void CHidden_Player::SetupMarine( void )
{
	m_nSkin = m_iCharacter;
	m_nBody = m_iCharacter;
	SetBodygroup( 2, 2 );
	SetMaxSpeed( HIDDEN_MARINE_SPEED );
	SetFOV( this, 0 );
	SetCollisionGroup( HIDDEN_COLLISION_GROUP_MARINE );

	GiveMarineLoadout();
}

void CHidden_Player::SetupHidden( void )
{
	m_nSkin = 1;
	m_nBody = 1;
	SetMaxSpeed( HIDDEN_HIDDEN_SPEED );
	m_flStamina = HIDDEN_STAMINA_MAX;
	SetFOV( this, HIDDEN_HIDDEN_FOV );
	SetCollisionGroup( HIDDEN_COLLISION_GROUP_HIDDEN );

	GiveHiddenLoadout();
}

void CHidden_Player::GiveMarineLoadout( void )
{
	// Nothing chosen yet, or "9": pick at random.
	if ( m_iPrimary < 0 || m_iPrimary >= HIDDEN_PRIMARY_COUNT )
		m_iPrimary = random->RandomInt( 0, HIDDEN_PRIMARY_COUNT - 1 );
	if ( m_iSecondary < 0 || m_iSecondary >= HIDDEN_SECONDARY_COUNT )
		m_iSecondary = random->RandomInt( 0, HIDDEN_SECONDARY_COUNT - 1 );
	if ( m_iEquipment < 0 || m_iEquipment >= HIDDEN_EQUIPMENT_COUNT )
		m_iEquipment = random->RandomInt( 0, HIDDEN_EQUIPMENT_COUNT - 1 );

	// Support marines can't carry the FN2000 or the shotgun.
	if ( m_iPlayerClass == HIDDEN_CLASS_SUPPORT &&
		( m_iPrimary == HIDDEN_PRIMARY_FN2000 || m_iPrimary == HIDDEN_PRIMARY_SHOTGUN ) )
	{
		m_iPrimary = HIDDEN_PRIMARY_P90;
	}

	// Ammo is given in rounds and clamped to the ammo type's magazine count (docs/spec/weapons.md).
	switch ( m_iSecondary )
	{
	case HIDDEN_SECONDARY_PISTOL:
		GiveNamedItem( "weapon_pistol" );
		CBasePlayer::GiveAmmo( 48, "AMMO_PISTOL" );
		break;
	case HIDDEN_SECONDARY_PISTOL2:
		GiveNamedItem( "weapon_pistol2" );
		CBasePlayer::GiveAmmo( 48, "AMMO_9MM" );
		break;
	}

	switch ( m_iEquipment )
	{
	case HIDDEN_EQUIPMENT_LASER:
		LaserTurnOn();	// marines start with the laser on
		break;
	case HIDDEN_EQUIPMENT_SONIC:
		GiveNamedItem( "weapon_sonic" );
		break;
	case HIDDEN_EQUIPMENT_BOOST:
		m_iBoostCount = HIDDEN_BOOST_CHARGES;
		break;
	}

	if ( m_iPlayerClass == HIDDEN_CLASS_SUPPORT )
	{
		if ( m_iEquipment != HIDDEN_EQUIPMENT_SONIC )
			GiveNamedItem( "weapon_sonic" );
		CBasePlayer::GiveAmmo( m_iEquipment == HIDDEN_EQUIPMENT_SONIC ? 3 : 2, "sonic" );
	}

	switch ( m_iPrimary )
	{
	case HIDDEN_PRIMARY_FN2000:
		GiveNamedItem( "weapon_fn2000" );
		CBasePlayer::GiveAmmo( 60, "AMMO_556" );
		break;
	case HIDDEN_PRIMARY_P90:
		GiveNamedItem( "weapon_p90" );
		CBasePlayer::GiveAmmo( 100, "AMMO_BULLETS" );
		break;
	case HIDDEN_PRIMARY_SHOTGUN:
		GiveNamedItem( "weapon_shotgun" );
		CBasePlayer::GiveAmmo( 24, "AMMO_BUCKSHOT" );
		break;
	case HIDDEN_PRIMARY_FN303:
		GiveNamedItem( "weapon_fn303" );
		CBasePlayer::GiveAmmo( 45, "XBowBolt" );
		break;
	}
}

void CHidden_Player::GiveHiddenLoadout( void )
{
	// Pipe bombs: none in OverRun; three without hdn_limitbombs; otherwise half the marines (rounded
	// up, as Beta 4b's FPU rounding mode does), less one.
	int iBombs = 3;
	if ( HiddenRules()->GetGameType() == HIDDEN_GAMETYPE_OVERRUN )
	{
		iBombs = 0;
	}
	else if ( hdn_limitbombs.GetBool() )
	{
		CTeam *pIRIS = GetGlobalTeam( TEAM_IRIS );
		const int iMarines = pIRIS ? pIRIS->GetNumPlayers() : 0;
		iBombs = (int)ceilf( iMarines * 0.5f ) - 1;
	}

	if ( iBombs > 0 )
	{
		GiveNamedItem( "weapon_grenade" );
		CBasePlayer::GiveAmmo( iBombs - 1, "AMMO_GRENADE" );	// the weapon itself holds one
	}

	GiveNamedItem( "weapon_knife" );
}

bool CHidden_Player::SetCurrentLocation( const char *pszLocation )
{
	if ( !Q_strcmp( m_szCurrentLocation, pszLocation ) )
		return false;

	Q_strncpy( m_szCurrentLocation.GetForModify(), pszLocation, HIDDEN_LOCATION_LENGTH );
	return true;
}

void CHidden_Player::BecomeObserver( void )
{
	if ( IsObserver() )
		return;

	RemoveAllItems( true );
	State_Transition( STATE_OBSERVER_MODE );
}

void CHidden_Player::ChangeTeam( int iTeam )
{
	// Skip CHL2MP_Player::ChangeTeam: it kills players who switch teams and picks HL2 models.
	CHL2_Player::ChangeTeam( iTeam );

	if ( iTeam == TEAM_SPECTATOR )
		BecomeObserver();
}

void CHidden_Player::Event_Killed( const CTakeDamageInfo &info )
{
	// The Hidden dies as the visible hidden.mdl instead of the cloaked mn_fixture1. As in Beta 4b the
	// player takes the model too, so the client poses the ragdoll from the same skeleton (hidden.mdl
	// has 21 bones, mn_fixture1 42).
	m_bClinging = false;

	const bool bHidden = ( GetTeamNumber() == TEAM_HIDDEN );
	if ( bHidden )
	{
		SetModel( HIDDEN_MODEL_HIDDEN_RAGDOLL );
		m_nSkin = 0;
		m_nBody = 0;
	}

	BaseClass::Event_Killed( info );

	// The corpse keeps the marine's skin and body groups; the Hidden's shows skin 2, body 1.
	CBaseAnimating *pRagdoll = m_hRagdoll ? m_hRagdoll->GetBaseAnimating() : NULL;
	if ( pRagdoll )
	{
		pRagdoll->m_nSkin = bHidden ? 2 : m_nSkin.Get();
		pRagdoll->m_nBody = bHidden ? 1 : m_nBody.Get();
	}
}

void CHidden_Player::PlayerDeathThink( void )
{
	BaseClass::PlayerDeathThink();

	// No respawning during a round: the dead watch until the next one.
	if ( !IsObserver() && gpGlobals->curtime > GetDeathTime() + DEATH_ANIMATION_TIME )
		BecomeObserver();
}

void CHidden_Player::PreThink( void )
{
	// +walk: pressing it (unless ducking) walks, letting go runs again.
	if ( ( m_afButtonPressed | m_afButtonReleased ) & IN_WALK )
	{
		if ( m_bWalking && !( m_afButtonPressed & IN_WALK ) )
			StopWalking();
		else if ( !m_bWalking && ( m_afButtonPressed & IN_WALK ) && !( m_nButtons & IN_DUCK ) )
			StartWalking();
	}

	// The Hidden's aura follows the vision key.
	if ( GetTeamNumber() == TEAM_HIDDEN && ( ( m_afButtonPressed | m_afButtonReleased ) & IN_GRENADE1 ) )
		m_bAura = ( m_afButtonPressed & IN_GRENADE1 ) != 0;

	BaseClass::PreThink();
}

// Beta 4b's CSDKPlayer walk speeds. They replace whatever speed was set, so walking ends a boost.
void CHidden_Player::StartWalking( void )
{
	SetMaxSpeed( GetTeamNumber() == TEAM_IRIS ? HIDDEN_MARINE_WALK_SPEED : HIDDEN_HIDDEN_WALK_SPEED );
	m_bWalking = true;
}

void CHidden_Player::StopWalking( void )
{
	SetMaxSpeed( GetTeamNumber() == TEAM_IRIS ? HIDDEN_MARINE_SPEED : HIDDEN_HIDDEN_SPEED );
	m_bWalking = false;
}

void CHidden_Player::ItemPostFrame( void )
{
	// The Hidden gets stamina back on the ground unless the aura is on, which drains it instead
	// (Beta 4b drains in PostThink, just after this).
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

void CHidden_Player::PostThink( void )
{
	BaseClass::PostThink();

	const QAngle angEyes = GetAnimEyeAngles();
	m_pHiddenAnimState->Update( angEyes[YAW], angEyes[PITCH] );

	if ( m_bStunned )
		UpdateStun();

	// The boost wears off back to walking speed.
	if ( m_bBoosted && m_flBoostTimer < gpGlobals->curtime )
	{
		SetMaxSpeed( HIDDEN_MARINE_SPEED );
		m_bBoosted = false;
	}
}

void CHidden_Player::SetAnimation( PLAYER_ANIM playerAnim )
{
	// Beta 4b's weapons set the base player animations, which the SDK anim state ignores; only a
	// reload (CWeaponSDKBase::SendReloadEvents) reaches it. Firing and throws send their own events,
	// and nothing sends the jump.
	if ( playerAnim == PLAYER_RELOAD )
		DoAnimationEvent( HIDDEN_ANIMEVENT_RELOAD );
}

void CHidden_Player::DoAnimationEvent( HiddenPlayerAnimEvent_t event )
{
	if ( event == HIDDEN_ANIMEVENT_THROW_GRENADE )
	{
		// Events can arrive late or not at all, so the clients watch this counter instead.
		m_iThrowGrenadeCounter = ( m_iThrowGrenadeCounter + 1 ) % ( 1 << HIDDEN_THROWGRENADE_COUNTER_BITS );
		return;
	}

	m_pHiddenAnimState->DoAnimationEvent( event );
	TE_HiddenPlayerAnimEvent( this, event );
}

CWeaponHiddenBase *CHidden_Player::HiddenAnim_GetActiveWeapon( void )
{
	return dynamic_cast<CWeaponHiddenBase *>( GetActiveWeapon() );
}

void CHidden_Player::ImpulseCommands( void )
{
	if ( GetImpulse() != 100 )
	{
		BaseClass::ImpulseCommands();
		return;
	}

	// The flashlight key uses the equipment. Like Beta 4b, this doesn't check the team, so a Hidden who
	// picked night vision as a marine can still toggle it.
	switch ( m_iEquipment )
	{
	case HIDDEN_EQUIPMENT_LASER:
		DevMsg( 1, "laser toggle\n" );
		if ( LaserIsOn() )
			LaserTurnOff();
		else
			LaserTurnOn();
		break;

	case HIDDEN_EQUIPMENT_FLASHLIGHT:
		DevMsg( 1, "Flash Light toggle\n" );
		if ( FlashlightIsOn() )
			FlashlightTurnOff();
		else
			FlashlightTurnOn();
		break;

	case HIDDEN_EQUIPMENT_NIGHTVISION:
		DevMsg( 1, "Night vision toggle\n" );
		EmitSound( m_bNightVision ? "IRIS.UnDeployNV" : "IRIS.DeployNV" );
		SetNightVision( !m_bNightVision );
		break;

	case HIDDEN_EQUIPMENT_BOOST:
		DevMsg( 1, "Boost values : %i - %i\n", m_bBoosted ? 1 : 0, m_iBoostCount );
		if ( !m_bBoosted && m_iBoostCount > 0 )
		{
			Boost();
			m_iBoostCount--;

			// The rush blurs the marine's view.
			Shockwave( 2.0f, 10.0f );
		}
		break;
	}

	ClearImpulse();
}

void CHidden_Player::Boost( void )
{
	m_flBoostTimer = gpGlobals->curtime + HIDDEN_BOOST_TIME;
	SetMaxSpeed( HIDDEN_BOOST_SPEED );
	m_bBoosted = true;
}

int CHidden_Player::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

void CHidden_Player::FlashlightTurnOn( void )
{
	if ( GetTeamNumber() == TEAM_IRIS )
		AddEffects( EF_DIMLIGHT );
}

void CHidden_Player::FlashlightTurnOff( void )
{
	if ( GetTeamNumber() == TEAM_IRIS )
		RemoveEffects( EF_DIMLIGHT );
}

void CHidden_Player::Stun( CBasePlayer *pStunner )
{
	StunTracker_t tracker;
	tracker.hStunner = pStunner;
	tracker.flExpires = gpGlobals->curtime + 10.0f;
	m_Stunners.AddToTail( tracker );

	if ( !m_bStunned )
	{
		m_bStunned = true;
		m_flBlur = 6.0f;
		m_flNextStunUpdate = gpGlobals->curtime + 0.2f;
	}

	// Each stun blurs a little more, up to 10; the first leaves it at 7.
	if ( m_flBlur < 6.0f )
		m_flBlur = 6.0f;
	if ( m_flBlur < 10.0f )
		m_flBlur += 1.0f;
}

void CHidden_Player::Shockwave( float flAmount, float flDuration )
{
	m_flStunTime = MAX( m_flStunTime, gpGlobals->curtime ) + flDuration;
	m_bStunned = true;

	if ( m_flBlur > 1.0f && m_flBlur < 10.0f )
		m_flBlur += flAmount;
	else
		m_flBlur = flAmount;

	m_flNextStunUpdate = gpGlobals->curtime + 0.2f;
}

void CHidden_Player::UpdateStun( void )
{
	if ( m_flNextStunUpdate < gpGlobals->curtime )
	{
		// Every stunner hurts the Hidden a little, 1.5 health a second each.
		if ( GetTeamNumber() == TEAM_HIDDEN )
		{
			for ( int i = 0; i < m_Stunners.Count(); i++ )
			{
				CBaseEntity *pStunner = m_Stunners[i].hStunner;
				CTakeDamageInfo info( pStunner, pStunner, GetAbsVelocity(), GetAbsOrigin(), 0.3f, DMG_DIRECT );
				TakeDamage( info );
			}
		}

		m_flNextStunUpdate = gpGlobals->curtime + 0.2f;

		if ( m_flBlur > 1.0f )
			m_flBlur -= 0.2f;

		// Beta 4b steps past the entry after a removed one; it catches up on the next update.
		for ( int i = 0; i < m_Stunners.Count(); i++ )
		{
			if ( m_Stunners[i].flExpires < gpGlobals->curtime )
				m_Stunners.Remove( i );
		}
	}

	if ( m_flStunTime < gpGlobals->curtime && m_Stunners.Count() == 0 )
		m_bStunned = false;
}

void CHidden_Player::ResetStun( void )
{
	m_bStunned = false;
	m_flBlur = 0.0f;
	m_Stunners.RemoveAll();
	m_flStunTime = 0.0f;
	m_flNextStunUpdate = 0.0f;
}

CBaseEntity *CHidden_Player::EntSelectSpawnPoint( void )
{
	const bool bHidden = ( GetTeamNumber() == TEAM_HIDDEN );
	const char *pszSpawn = bHidden ? "info_hidden_spawn" : "info_marine_spawn";
	EHANDLE &hLast = bHidden ? g_hLastHiddenSpawn : g_hLastMarineSpawn;

	// Beta 4b walks the spawn points in order and doesn't check whether they're occupied.
	CBaseEntity *pSpot = gEntList.FindEntityByClassname( hLast.Get(), pszSpawn );
	if ( !pSpot )
		pSpot = gEntList.FindEntityByClassname( NULL, pszSpawn );

	if ( !pSpot )
		return BaseClass::EntSelectSpawnPoint();

	hLast = pSpot;
	return pSpot;
}

void CHidden_Player::AddWeighting( int iAmount )
{
	m_iWeighting += iAmount;
	DevMsg( 1, "weighting : %i\n", m_iWeighting.Get() );
}

bool CHidden_Player::TakeCharacter( int iCharacter )
{
	CHiddenRules *pRules = HiddenRules();

	if ( iCharacter == m_iCharacter )
		return true;

	// 9 takes the first free slot.
	if ( iCharacter == HIDDEN_NUM_CHARACTERS )
	{
		for ( int i = 0; i < HIDDEN_NUM_CHARACTERS; i++ )
		{
			if ( !pRules->IsCharacterTaken( i ) )
			{
				iCharacter = i;
				break;
			}
		}
	}

	if ( pRules->IsCharacterTaken( iCharacter ) )
		return false;

	ReleaseCharacter();
	pRules->SetCharacterTaken( iCharacter, true );
	m_iCharacter = iCharacter;
	return true;
}

void CHidden_Player::ReleaseCharacter( void )
{
	if ( HasValidCharacter() )
		HiddenRules()->SetCharacterTaken( m_iCharacter, false );

	m_iCharacter = -1;
}

bool CHidden_Player::ClientCommand( const CCommand &args )
{
	const char *pszCmd = args[0];
	const int iArg = ( args.ArgC() > 1 ) ? atoi( args[1] ) : 0;

	if ( FStrEq( pszCmd, "changeclass" ) )
	{
		m_iPlayerClass = ( iArg >= 0 && iArg < HIDDEN_CLASS_COUNT ) ? iArg : HIDDEN_CLASS_NONE;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "changemarine" ) )
	{
		if ( iArg == -2 )
			return true;	// keep the current character

		if ( !TakeCharacter( iArg ) )
		{
			ClientPrint( this, HUD_PRINTCENTER, "That marine is already taken" );
			return true;
		}

		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "primary" ) )
	{
		m_iPrimary = iArg;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "secondary" ) )
	{
		m_iSecondary = iArg;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "equip" ) )
	{
		m_iEquipment = iArg;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "enter" ) )
	{
		if ( !HasValidCharacter() )
		{
			m_bReadyToPlay = false;
			ClientPrint( this, HUD_PRINTCENTER, "You will not spawn, use classmenu to pick a valid player" );
			return true;
		}

		// Spectators join IRIS; the next round decides who is the Hidden.
		if ( GetTeamNumber() == TEAM_SPECTATOR || GetTeamNumber() == TEAM_UNASSIGNED )
			ChangeTeam( TEAM_IRIS );
		return true;
	}
	else if ( FStrEq( pszCmd, "choosehidden" ) )
	{
		m_bNoHidden = ( iArg != 0 );
		DevMsg( 1, m_bNoHidden ? "I DON'T Want to be the Hidden\n" : "I Do Want to be the Hidden\n" );
		return true;
	}
	else if ( FStrEq( pszCmd, "spectate" ) )
	{
		ReleaseCharacter();
		m_bReadyToPlay = false;
		HandleCommand_JoinTeam( TEAM_SPECTATOR );
		return true;
	}
	else if ( FStrEq( pszCmd, "jointeam" ) || FStrEq( pszCmd, "joingame" ) || FStrEq( pszCmd, "vguicancel" ) )
	{
		return true;	// the game rules pick teams
	}

	return BaseClass::ClientCommand( args );
}
