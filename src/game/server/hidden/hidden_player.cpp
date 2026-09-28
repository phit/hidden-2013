//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden player: marine class and character, loadout, team setup
//			and Hidden selection state. See docs/spec/teams-classes.md.
//
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "hidden_corpse.h"
#include "gib.h"
#include "player_pickup.h"
#include "hidden_player.h"
#include "plugins/hidden_plugins.h"
#include "hidden_gamerules.h"
#include "hidden_cvars.h"
#include "team.h"
#include "weapon_hiddenbase.h"
#include "hidden_spectator.h"
#include "soundent.h"
#include "world.h"
#include "inetchannelinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Deviation: Beta 4b's spectators only had the cameras and the marines' helmet cams. An admin can
// lift that for one player, from the server console or rcon (or a listen server's host for anyone).
// The server console or rcon, or a listen server's own player. UTIL_IsCommandIssuedByServerAdmin
// assumes the host is slot 1, which bots added before the host joins can take.
bool HiddenIsCommandIssuedByServerAdmin( const char *pszCommand )
{
	CBasePlayer *pCaller = UTIL_GetCommandClient();
	if ( !pCaller )
		return true;

	INetChannelInfo *pNetInfo = engine->GetPlayerNetInfo( pCaller->entindex() );
	if ( !engine->IsDedicatedServer() && pNetInfo && pNetInfo->IsLoopback() )
		return true;

	ClientPrint( pCaller, HUD_PRINTCONSOLE, UTIL_VarArgs( "%s: only the server can use this\n", pszCommand ) );
	return false;
}

CON_COMMAND_F( hdn_spec_unrestricted, "hdn_spec_unrestricted <name|#userid> [0|1]: let a player also watch the Hidden and use the chase and free cameras", FCVAR_GAMEDLL )
{
	if ( !HiddenIsCommandIssuedByServerAdmin( "hdn_spec_unrestricted" ) )
		return;

	CHidden_Player *pTarget = NULL;
	if ( args.ArgC() >= 2 )
	{
		const char *pszWho = args[1];
		pTarget = ToHiddenPlayer( pszWho[0] == '#' ? UTIL_PlayerByUserId( atoi( pszWho + 1 ) ) : UTIL_PlayerByName( pszWho ) );
	}
	else
	{
		pTarget = ToHiddenPlayer( UTIL_GetCommandClient() );	// a listen server's host, on themselves
	}

	if ( !pTarget )
	{
		Msg( "hdn_spec_unrestricted: no such player\n" );
		return;
	}

	const bool bOn = args.ArgC() >= 3 ? atoi( args[2] ) != 0 : !pTarget->IsSpecUnrestricted();
	pTarget->SetSpecUnrestricted( bOn );
	Msg( "%s: spectating %s\n", pTarget->GetPlayerName(), bOn ? "unrestricted" : "restricted" );
	ClientPrint( pTarget, HUD_PRINTCONSOLE, bOn ? "Spectating unrestricted\n" : "Spectating restricted\n" );

	if ( !bOn && pTarget->IsObserver() )
	{
		pTarget->SetObserverMode( pTarget->GetObserverMode() );
		pTarget->ValidateCurrentObserverTarget();
	}
}

#define HIDDEN_MODEL_MARINE			"models/player/iris.mdl"
#define HIDDEN_MODEL_MARINE_SUPPORT	"models/player/iris_supply.mdl"
#define HIDDEN_MODEL_HIDDEN			"models/manor/mn_fixture1.mdl"	// the cloaked Hidden, see docs/spec/client.md
#define HIDDEN_MODEL_HIDDEN_RAGDOLL	"models/player/hidden.mdl"

#define HIDDEN_HIDDEN_FOV			110

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
	SendPropBool( SENDINFO( m_bRequestAmmo ) ),
	SendPropBool( SENDINFO( m_bRevealed ) ),
	SendPropInt( SENDINFO( m_iSpawnCount ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iSpeedMode ), 2, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bWalking ) ),
	SendPropFloat( SENDINFO( m_flBoostEnd ), 0, SPROP_NOSCALE ),
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
	m_bSpecUnrestricted = false;
	m_iWeighting = 0;
	m_bReadyToPlay = false;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;
	m_bHadHidden = false;
	m_bLAM = false;
	m_bNightVision = false;
	m_iBoostCount = 0;
	m_iSpeedMode = HIDDEN_SPEED_RUN;
	m_bWalking = false;
	m_flBoostEnd = 0.0f;
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
	PrecacheHiddenCorpses();

	// Beta 4b's CSDKPlayer::Precache list.
	static const char *s_pszSounds[] =
	{
		"Hidden.BehindYou", "Hidden.ImHere", "Hidden.ISeeYou", "Hidden.LookUp", "Hidden.TurnAround",
		"Hidden.OverHere", "Hidden.YouAreNext", "Hidden.FreshMeat", "Hidden.ComingForYou",
		"IRIS.Damage", "IRIS.RoundStart", "IRIS.RoundEnd", "IRIS.UnDeployNV", "IRIS.DeployNV",
		"IRIS.DeploySonicAlarm", "IRIS.EnemySighted", "IRIS.RequestingAmmo", "IRIS.Affirmative",
		"IRIS.ReportIn", "IRIS.ReportingIn", "IRIS.AgentDown", "IRIS.IfItBleeds", "IRIS.OneUglyMother",
		"IRIS.WhereAreYou", "IRIS.ComeOnOut", "IRIS.BringIt",
		"Player.KnifePigstick.Death", "Player.KnifeSlash.Death", "Player.FriendlyFire.Death",
		// Played here but missing from Beta 4b's list.
		"Hidden.Death",
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszSounds ); i++ )
		PrecacheScriptSound( s_pszSounds[i] );
}

void CHidden_Player::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	// The cameras spectators cycle through; without any, the first Hidden spawn.
	m_Cameras.RemoveAll();
	for ( CBaseEntity *pCamera = gEntList.FindEntityByClassname( NULL, "info_spectator" ); pCamera; pCamera = gEntList.FindEntityByClassname( pCamera, "info_spectator" ) )
		m_Cameras.AddToTail( pCamera );

	DevMsg( 1, "Camera Count : %i\n", m_Cameras.Count() );
	if ( !m_Cameras.Count() )
	{
		CBaseEntity *pSpawn = gEntList.FindEntityByClassname( NULL, "info_hidden_spawn" );
		if ( pSpawn )
			m_Cameras.AddToTail( pSpawn );
	}

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
	m_iSpeedMode = HIDDEN_SPEED_RUN;
	m_bWalking = false;
	m_flBoostEnd = 0.0f;
	m_bUseDroppedObject = false;
	m_bRequestAmmo = false;
	m_bAmmoReceived = false;
	m_bRevealed = false;
	m_iSpawnCount = ( m_iSpawnCount + 1 ) % 8;
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

	// CHL2MP_Player::Spawn clears the HUD bits StartObserverMode set, which showed HL2's health
	// and suit readouts to spectators.
	if ( IsObserver() )
		m_Local.m_iHideHUD = HIDEHUD_HEALTH;

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

	// Whoever was watching this player through the helmet cam can't keep watching the Hidden.
	if ( iTeam == TEAM_HIDDEN )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pObserver = UTIL_PlayerByIndex( i );
			if ( pObserver && pObserver != this && pObserver->IsObserver() && pObserver->GetObserverTarget() == this )
				pObserver->ValidateCurrentObserverTarget();
		}
	}
}

void CHidden_Player::Event_Killed( const CTakeDamageInfo &info )
{
	// The Hidden dies as the visible hidden.mdl instead of the cloaked mn_fixture1; the corpse copies
	// the player's model, so the player takes it first, as in Beta 4b.
	m_bClinging = false;

	if ( GetTeamNumber() == TEAM_HIDDEN )
	{
		SetModel( HIDDEN_MODEL_HIDDEN_RAGDOLL );
		m_nSkin = 0;
		m_nBody = 0;
	}

	m_KillInfo = info;
	BaseClass::Event_Killed( info );
}

// Deviation: a Hidden killed holding its pipe bombs leaves none behind. Beta 4b dropped them like
// any active weapon, where only another Hidden could pick them up.
void CHidden_Player::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget, const Vector *pVelocity )
{
	if ( pWeapon && !IsAlive() && FClassnameIs( pWeapon, "weapon_grenade" ) )
	{
		Weapon_Detach( pWeapon );
		UTIL_Remove( pWeapon );
		return;
	}

	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );
}

// Beta 4b's player came straight from CBasePlayer, so this is its OnTakeDamage_Alive (without HL2's
// drowning and burning sounds) plus Beta 4b's additions: the player_hurt event's damage and "hidden",
// a hurt log line, the attacker's weighting and IRIS.Damage. The built-in plugins can change or drop
// the damage first, and hear about it after.
int CHidden_Player::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;
	if ( !HiddenPlugins_OnTakeDamage( this, info ) )
		return 0;

	m_bitsDamageType |= info.GetDamageType();

	if ( !CBaseCombatCharacter::OnTakeDamage_Alive( info ) )
		return 0;

	CBaseEntity *pAttacker = info.GetAttacker();
	if ( !pAttacker )
		return 0;

	Vector vecDir = vec3_origin;
	if ( info.GetInflictor() )
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector( 0, 0, 10 ) - WorldSpaceCenter();
		VectorNormalize( vecDir );
	}

	if ( info.GetInflictor() && GetMoveType() == MOVETYPE_WALK && !pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) )
	{
		// CBasePlayer's knockback (its DamageForce is file-static).
		const Vector vecSize = WorldAlignSize();
		float flForce = MIN( info.GetBaseDamage() * ( ( 32 * 32 * 72.0f ) / ( vecSize.x * vecSize.y * vecSize.z ) ) * 5, 1000.0f );
		Vector vecForce = vecDir * -flForce;
		if ( vecForce.z > 250.0f )
			vecForce.z = 250.0f;
		ApplyAbsVelocityImpulse( vecForce );
	}

	IGameEvent *event = gameeventmanager->CreateEvent( "player_hurt" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "health", MAX( 0, m_iHealth ) );
		event->SetFloat( "damage", info.GetDamage() );
		event->SetInt( "priority", 5 );

		if ( pAttacker->IsPlayer() )
		{
			CBasePlayer *pPlayer = ToBasePlayer( pAttacker );
			event->SetInt( "attacker", pPlayer->GetUserID() );

			const char *pszWeapon = "world";
			if ( pPlayer->GetActiveWeapon() )
			{
				pszWeapon = pPlayer->GetActiveWeapon()->GetClassname();
				if ( !Q_strncmp( pszWeapon, "weapon_", 7 ) )
					pszWeapon += 7;
			}
			event->SetBool( "hidden", pPlayer->GetTeamNumber() == TEAM_HIDDEN || GetTeamNumber() == TEAM_HIDDEN );
			event->SetString( "weapon", pszWeapon );

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" hurt \"%s<%i><%s><%s>\" for \"<%i>\" with \"%s\"\n",
				pPlayer->GetPlayerName(), pPlayer->GetUserID(), pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam() ? pPlayer->GetTeam()->GetName() : "",
				GetPlayerName(), GetUserID(), GetNetworkIDString(), GetTeam() ? GetTeam()->GetName() : "",
				RoundFloatToInt( info.GetDamage() ), pszWeapon );

			// Hurting the other team counts towards being picked as the Hidden; hurting your own
			// (yourself included) counts against it.
			CHidden_Player *pHiddenAttacker = ToHiddenPlayer( pPlayer );
			if ( pHiddenAttacker )
			{
				const float flDamage = ( pPlayer->GetTeamNumber() == GetTeamNumber() ) ? -info.GetDamage() : info.GetDamage();
				pHiddenAttacker->AddWeighting( RoundFloatToInt( flDamage ) );
			}
		}
		else
		{
			event->SetInt( "attacker", 0 );
			event->SetBool( "hidden", false );
			event->SetString( "weapon", "world" );
		}

		gameeventmanager->FireEvent( event );
	}

	if ( GetTeamNumber() == TEAM_IRIS )
		EmitSound( "IRIS.Damage" );

	if ( pAttacker->IsNPC() )
		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 512, 0.5, this );

	HiddenPlugins_PlayerHurt( this, info );

	return 1;
}

void CHidden_Player::DeathSound( const CTakeDamageInfo &info )
{
	if ( m_bitsDamageType & DMG_FALL )
	{
		EmitSound( "Player.FallGib" );
		return;
	}

	if ( GetTeamNumber() == TEAM_IRIS )
	{
		// The pigstick (925) tells apart from a slash; exactly DMG_SLASH, so not a thrown knife's.
		if ( info.GetDamageType() == DMG_SLASH )
			EmitSound( info.GetDamage() <= 100.0f ? "Player.KnifeSlash.Death" : "Player.KnifePigstick.Death" );
		else
			EmitSound( "Player.FriendlyFire.Death" );
		return;
	}

	EmitSound( "Hidden.Death" );
}

// Corpses are server ragdolls the Hidden can feed on (HL2MP's are client-side). In OverRun the dead
// leave only gibs.
void CHidden_Player::CreateRagdollEntity( void )
{
	if ( HiddenRules()->GetGameType() == HIDDEN_GAMETYPE_OVERRUN )
	{
		static const char *s_pszGibs[] = { "models/gibs/iris_gibs1.mdl", "models/gibs/iris_gibs2.mdl", "models/gibs/iris_gibs3.mdl",
			"models/gibs/iris_gibs4.mdl", "models/gibs/iris_gibs6.mdl", "models/gibs/iris_gibs7.mdl" };
		for ( int i = 0; i < ARRAYSIZE( s_pszGibs ); i++ )
			CGib::SpawnSpecificGibs( this, 1, 750.0f, 1500.0f, s_pszGibs[i], 5.0f );
		return;
	}

	CreateHiddenCorpse( this, m_nForceBone, m_KillInfo, COLLISION_GROUP_DEBRIS );
}

// Beta 4b made ragdolls usable (and players, for the support marine's ammo hand-out).
bool CHidden_Player::IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps )
{
	if ( BaseClass::IsUseableEntity( pEntity, requiredCaps ) )
		return true;

	CBaseAnimating *pAnimating = pEntity ? pEntity->GetBaseAnimating() : NULL;
	return pAnimating && ( pAnimating->IsRagdoll() || pAnimating->IsPlayer() );
}

void CHidden_Player::PlayerUse( void )
{
	// HL2's +use lets go of a held object on the press; the release mustn't pick it straight up again.
	if ( m_afButtonPressed & IN_USE )
		m_bUseDroppedObject = ( m_hUseEntity != NULL );

	BaseClass::PlayerUse();

	// Letting go of +use on a ragdoll picks it up (CRagdollProp::Use in Beta 4b); on a marine who
	// called for ammo, a support marine hands some over.
	if ( ( m_afButtonReleased & IN_USE ) && !m_bUseDroppedObject && m_hUseEntity == NULL )
	{
		CBaseEntity *pEntity = FindUseEntity();
		CBaseAnimating *pAnimating = pEntity ? pEntity->GetBaseAnimating() : NULL;
		if ( pAnimating && pAnimating->IsRagdoll() )
		{
			PickupObject( pAnimating );
		}
		else if ( pEntity && pEntity->IsPlayer() && GetTeamNumber() == TEAM_IRIS && m_iPlayerClass == HIDDEN_CLASS_SUPPORT )
		{
			CHidden_Player *pTarget = ToHiddenPlayer( pEntity );
			if ( pTarget && pTarget->GetTeamNumber() == TEAM_IRIS && pTarget->IsRequestingAmmo() )
				pTarget->GiveRequestedAmmo( this );
		}
	}
}

// A magazine's worth by weapon: the secondary's, then the primary's.
void CHidden_Player::GiveRequestedAmmo( CHidden_Player *pGiver )
{
	static const struct { int iAmount; const char *pszAmmo; } s_Secondary[] =
	{
		{ 20, "AMMO_PISTOL" },		// HIDDEN_SECONDARY_PISTOL
		{ 16, "AMMO_9MM" },			// HIDDEN_SECONDARY_PISTOL2
	}, s_Primary[] =
	{
		{ 30, "AMMO_556" },			// HIDDEN_PRIMARY_FN2000
		{ 50, "AMMO_BULLETS" },		// HIDDEN_PRIMARY_P90
		{ 8, "AMMO_BUCKSHOT" },		// HIDDEN_PRIMARY_SHOTGUN
		{ 15, "XBowBolt" },			// HIDDEN_PRIMARY_FN303
	};

	if ( m_iSecondary >= 0 && m_iSecondary < ARRAYSIZE( s_Secondary ) )
		CBasePlayer::GiveAmmo( s_Secondary[m_iSecondary].iAmount, s_Secondary[m_iSecondary].pszAmmo );
	if ( m_iPrimary >= 0 && m_iPrimary < ARRAYSIZE( s_Primary ) )
		CBasePlayer::GiveAmmo( s_Primary[m_iPrimary].iAmount, s_Primary[m_iPrimary].pszAmmo );

	m_bRequestAmmo = false;
	m_bAmmoReceived = true;
	pGiver->AddWeighting( 20 );
}

bool CHidden_Player::Radio( int iMessage )
{
	// Not while dead or within hdn_radio_limit of the last call (then the command goes unhandled, so
	// the console says it's unknown, as in Beta 4b).
	if ( !IsAlive() || gpGlobals->curtime < m_flRadioTimer )
		return false;

	m_flRadioTimer = gpGlobals->curtime + hdn_radio_limit.GetFloat();

	if ( GetTeamNumber() == TEAM_HIDDEN )
	{
		static const char *s_pszTaunts[] = { "Hidden.BehindYou", "Hidden.ImHere", "Hidden.ISeeYou", "Hidden.LookUp",
			"Hidden.TurnAround", "Hidden.OverHere", "Hidden.FreshMeat", "Hidden.YouAreNext" };
		if ( iMessage >= 0 && iMessage < ARRAYSIZE( s_pszTaunts ) )
		{
			// The sound script's random pick, made here so the plugins learn which line it was.
			CSoundParameters params;
			if ( GetParametersForSound( s_pszTaunts[iMessage], params, NULL ) )
			{
				CPASAttenuationFilter filter( this, params.soundlevel );
				EmitSound_t ep( params );
				EmitSound( filter, entindex(), ep );
				HiddenPlugins_HiddenTaunt( this, params.soundname );
			}
		}
		return true;
	}

	if ( GetTeamNumber() != TEAM_IRIS )
		return true;

	// Taunts: a sound and a line in open chat, no radio call.
	switch ( iMessage )
	{
	case 5:	EmitSound( "IRIS.OneUglyMother" ); engine->ClientCommand( edict(), "say You are one UGLY mother!\n" ); return true;
	case 6:	EmitSound( "IRIS.IfItBleeds" ); engine->ClientCommand( edict(), "say If it bleeds, we can kill it!\n" ); return true;
	case 7:	EmitSound( "IRIS.BringIt" ); engine->ClientCommand( edict(), "say Bring it!\n" ); return true;
	}

	// Radio calls: a team chat line (which carries the location), and every other number too sends
	// the iris_radio event the clients play the call from.
	static const char *s_pszReports[] = { "say_team Reporting In\n", "say_team Still Here!\n", "say_team Checking In\n" };
	switch ( iMessage )
	{
	case 0:	engine->ClientCommand( edict(), "say_team Agent Down\n" ); break;
	case 1:	engine->ClientCommand( edict(), "say_team Subject Sighted\n" ); break;
	case 2:	engine->ClientCommand( edict(), "say_team Affirmative\n" ); break;
	case 3:
		if ( m_bAmmoReceived )
			return true;
		engine->ClientCommand( edict(), "say_team I need ammo!\n" );
		m_bRequestAmmo = true;
		break;
	case 4:	engine->ClientCommand( edict(), "say_team Report In\n" ); break;
	case 10: engine->ClientCommand( edict(), s_pszReports[random->RandomInt( 0, ARRAYSIZE( s_pszReports ) - 1 )] ); break;
	}

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "iris_radio" );
	if ( pEvent )
	{
		pEvent->SetInt( "message", iMessage );
		pEvent->SetInt( "userid", GetUserID() );
		gameeventmanager->FireEvent( pEvent );
	}

	return true;
}

// Only the Hidden carries things (with HL2's pickup controller): ragdolls of any weight, other
// models up to 45 kg and 192 units (HL2: 35 and 128), brushes unlimited.
void CHidden_Player::PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize )
{
	if ( GetTeamNumber() != TEAM_HIDDEN || GetGroundEntity() == pObject )
		return;

	CBaseAnimating *pAnimating = pObject->GetBaseAnimating();
	if ( bLimitMassAndSize && pAnimating && !pAnimating->IsRagdoll() && !CBasePlayer::CanPickupObject( pObject, 45.0f, 192.0f ) )
		return;

	if ( IsHoldingEntity( pObject ) )
	{
		ClearUseEntity();
		return;
	}

	if ( pObject->HasNPCsOnIt() )
		return;

	PlayerPickupObject( this, pObject );
}

// Beta 4b had only its two spectator modes; anything else (roaming, chase, the last mode HL2MP
// remembers) becomes the cameras.
bool CHidden_Player::SetObserverMode( int mode )
{
	// hdn_spec_unrestricted (deviation, for admins) also allows the chase and free cameras.
	const bool bAllowed = mode == OBS_MODE_NONE || mode == OBS_MODE_DEATHCAM || mode == OBS_MODE_IN_EYE ||
		( m_bSpecUnrestricted && ( mode == OBS_MODE_CHASE || mode == OBS_MODE_ROAMING ) );
	if ( !bAllowed )
		mode = OBS_MODE_FIXED;

	if ( !BaseClass::SetObserverMode( mode ) )
		return false;

	// The cameras don't follow anyone; SDK 2013 would leave the view where the player died.
	if ( mode == OBS_MODE_FIXED && !IsValidObserverTarget( m_hObserverTarget ) )
	{
		CBaseEntity *pCamera = FindNextObserverTarget( false );
		if ( pCamera )
			SetObserverTarget( pCamera );
	}

	return true;
}

// On a camera, the view is the camera's "position" attachment, as it's turned, with its FOV.
bool CHidden_Player::SetObserverTarget( CBaseEntity *target )
{
	if ( !BaseClass::SetObserverTarget( target ) )
		return false;

	if ( GetObserverMode() == OBS_MODE_FIXED )
	{
		Vector vecOrigin = target->GetAbsOrigin();
		QAngle angles = target->GetAbsAngles();

		CBaseAnimating *pAnimating = target->GetBaseAnimating();
		if ( pAnimating && pAnimating->LookupAttachment( "position" ) > 0 )
			pAnimating->GetAttachment( "position", vecOrigin, angles );

		JumptoPosition( vecOrigin, angles );

		CHiddenSpectatorPoint *pCamera = dynamic_cast<CHiddenSpectatorPoint *>( target );
		if ( pCamera && pCamera->GetFOV() > 0.0f )
			SetFOV( this, (int)pCamera->GetFOV() );
	}
	else
	{
		// Beta 4b moves the observer to whoever it watches (which keeps them in its PVS).
		SetAbsOrigin( target->GetAbsOrigin() );
	}

	return true;
}

// The cameras, or the marines; with no marine left to watch, back to the cameras.
CBaseEntity *CHidden_Player::FindNextObserverTarget( bool bReverse )
{
	if ( GetObserverMode() == OBS_MODE_IN_EYE || GetObserverMode() == OBS_MODE_CHASE )
	{
		// Start from the watched player, or from ourselves when watching a camera (whose index is
		// past the players', so it must not be the loop's end marker).
		int i = ( m_hObserverTarget && m_hObserverTarget->IsPlayer() ) ? m_hObserverTarget->entindex() : entindex();
		for ( int iTries = 0; iTries < gpGlobals->maxClients; iTries++ )
		{
			i += bReverse ? -1 : 1;
			if ( i > gpGlobals->maxClients )
				i = 1;
			else if ( i < 1 )
				i = gpGlobals->maxClients;

			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( IsValidObserverTarget( pPlayer ) )
				return pPlayer;
		}

		ForceObserverMode( OBS_MODE_FIXED );
	}

	if ( !m_Cameras.Count() )
		return NULL;

	int iCurrent = -1;
	for ( int i = 0; i < m_Cameras.Count(); i++ )
	{
		if ( m_Cameras[i] == m_hObserverTarget )
		{
			iCurrent = i;
			break;
		}
	}

	if ( iCurrent < 0 )
		return m_Cameras[0];

	const int iCount = m_Cameras.Count();
	return m_Cameras[( iCurrent + ( bReverse ? iCount - 1 : 1 ) ) % iCount];
}

// The cameras, and living marines who aren't observers or hidden from view.
bool CHidden_Player::IsValidObserverTarget( CBaseEntity *target )
{
	if ( !target )
		return false;

	for ( int i = 0; i < m_Cameras.Count(); i++ )
	{
		if ( m_Cameras[i] == target )
			return true;
	}

	if ( !target->IsPlayer() )
		return false;

	CBasePlayer *pPlayer = ToBasePlayer( target );
	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN && !m_bSpecUnrestricted )
		return false;
	return pPlayer != this && !pPlayer->IsObserver() && !pPlayer->IsEffectActive( EF_NODRAW ) && pPlayer->IsAlive();
}

void CHidden_Player::PlayerDeathThink( void )
{
	BaseClass::PlayerDeathThink();

	// No respawning during a round: the dead watch until the next one.
	if ( !IsObserver() && gpGlobals->curtime > GetDeathTime() + DEATH_ANIMATION_TIME )
		BecomeObserver();
}

// Beta 4b's player came from the SDK template, which ignores +zoom (its default z bind): the FN2000's
// scope is the secondary attack. HL2's player zooms the suit on it and blocks attacking while it's held.
void CHidden_Player::PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	ucmd->buttons &= ~IN_ZOOM;
	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}

void CHidden_Player::PreThink( void )
{
	// The Hidden's aura follows the vision key.
	if ( GetTeamNumber() == TEAM_HIDDEN && ( ( m_afButtonPressed | m_afButtonReleased ) & IN_GRENADE1 ) )
		m_bAura = ( m_afButtonPressed & IN_GRENADE1 ) != 0;

	BaseClass::PreThink();
}

// HL2's calls; +walk itself is handled per command in UpdateMaxSpeed.
void CHidden_Player::StartWalking( void )
{
	m_bWalking = true;
	m_iSpeedMode = HIDDEN_SPEED_WALK;
}

void CHidden_Player::StopWalking( void )
{
	m_bWalking = false;
	m_iSpeedMode = HIDDEN_SPEED_RUN;
}

void CHidden_Player::ItemPostFrame( void )
{
	// The Hidden gets stamina back on the ground unless the aura is on, which drains it instead
	// (Beta 4b drains in PostThink, just after this).
	if ( GetTeamNumber() == TEAM_HIDDEN && m_flStamina < HIDDEN_STAMINA_MAX && GetGroundEntity() != NULL && !m_bAura )
		SetStamina( HIDDEN_STAMINA_REGEN * HiddenStaminaTickScale() );

	if ( m_bAura )
	{
		SetStamina( -HIDDEN_AURA_STAMINA * HiddenStaminaTickScale() );
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
		DevMsg( 1, "Boost values : %i - %i\n", IsBoosted() ? 1 : 0, m_iBoostCount );
		if ( !IsBoosted() && m_iBoostCount > 0 )
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

// Not predicted (impulses run on the server only), so the client corrects once when it starts.
void CHidden_Player::Boost( void )
{
	m_flBoostEnd = gpGlobals->curtime + HIDDEN_BOOST_TIME;
	m_iSpeedMode = HIDDEN_SPEED_BOOST;
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
	{
		pSpot = BaseClass::EntSelectSpawnPoint();

		// Beta 4b returned NULL here and the spawn code used it: htr_tutorial has no
		// info_marine_spawn, so everyone connecting (spectators search it too) crashed the server.
		// Fall back to the other team's spawn, a spectator camera, then the world's origin.
		if ( !pSpot )
			pSpot = gEntList.FindEntityByClassname( NULL, bHidden ? "info_marine_spawn" : "info_hidden_spawn" );
		if ( !pSpot )
			pSpot = gEntList.FindEntityByClassname( NULL, "info_spectator" );
		if ( !pSpot )
			pSpot = GetWorldEntity();
		return pSpot;
	}

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

	if ( FStrEq( pszCmd, "radio" ) )
	{
		return Radio( iArg );
	}
	else if ( FStrEq( pszCmd, "spec_next" ) || FStrEq( pszCmd, "spec_prev" ) )
	{
		// The cameras cycle too (SDK 2013 only cycles the modes that follow players).
		if ( IsObserver() )
		{
			CBaseEntity *pTarget = FindNextObserverTarget( FStrEq( pszCmd, "spec_prev" ) );
			if ( pTarget )
				SetObserverTarget( pTarget );
		}
		return true;
	}
	else if ( FStrEq( pszCmd, "spec_mode" ) )
	{
		// Beta 4b's two modes: toggle, or 1 for the cameras and 2 for the marines. Players an admin
		// unrestricted (hdn_spec_unrestricted) also get 3, the chase camera, and 4, the free camera.
		static const int s_iModes[] = { OBS_MODE_FIXED, OBS_MODE_IN_EYE, OBS_MODE_CHASE, OBS_MODE_ROAMING };
		const int iModes = m_bSpecUnrestricted ? 4 : 2;

		int iMode;
		if ( args.ArgC() > 1 )
		{
			iMode = s_iModes[clamp( iArg, 1, iModes ) - 1];
		}
		else
		{
			int iCurrent = 0;
			for ( int i = 0; i < iModes; i++ )
			{
				if ( s_iModes[i] == GetObserverMode() )
					iCurrent = i;
			}
			iMode = s_iModes[( iCurrent + 1 ) % iModes];
		}

		m_iObserverLastMode = iMode;
		engine->ClientCommand( edict(), "cl_spec_mode %d", iMode );

		if ( IsObserver() && SetObserverMode( iMode ) && iMode != OBS_MODE_ROAMING )
		{
			CBaseEntity *pTarget = FindNextObserverTarget( false );
			if ( pTarget )
				SetObserverTarget( pTarget );
		}
		return true;
	}
	else if ( FStrEq( pszCmd, "changeclass" ) )
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
