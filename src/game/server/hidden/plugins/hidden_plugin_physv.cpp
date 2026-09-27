//========= Hidden: Source =====================================================//
//
// Purpose: Physics Vs (Paegus's hsm_physv 1.0.0): the Hidden fights with props
//			only, and marines are held to set weapons. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78952
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "ammodef.h"
#include "team.h"
#include "hidden_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool HiddenIsCommandIssuedByServerAdmin( const char *pszCommand );

enum
{
	PV_OFF = 0,
	PV_ON,
	PV_AUTOMATIC,
};

#define PV_PRIMARY_NONE		4
#define PV_PRIMARY_ANY		5
#define PV_SECONDARY_NONE	2
#define PV_SECONDARY_ANY	3

static void PhysVModeChanged( IConVar *var, const char *pOldValue, float flOldValue );

static ConVar hsm_pv_mode( "hsm_pv_mode", "0", FCVAR_NOTIFY, "Physics Vs mode. 0: Disabled, 1: Enabled, 2: Automatic.", true, 0.0f, true, 2.0f, PhysVModeChanged );
static ConVar hsm_pv_amax( "hsm_pv_amax", "1", 0, "Maximum IRIS players for automatic mode.", true, 1.0f, true, 9.0f );
static ConVar hsm_pv_primaries( "hsm_pv_primaries", "4", 0, "The Primary weapon number. 0: Fn2000, 1: P90, 2: Shotgun, 3: Fn303, 4: None. 5: Any.", true, 0.0f, true, 5.0f );
static ConVar hsm_pv_secondaries( "hsm_pv_secondaries", "3", 0, "The secondary weapon number. 0; FiveseveN, 1: FNP9, 2: None, 3: Any.", true, 0.0f, true, 3.0f );
static ConVar hsm_pv_regen( "hsm_pv_regen", "0.25", 0, "Fraction of damage to restore to hidden as health. 0: None. 1: 100%.", true, 0.0f, true, 1.0f );

static const char *s_pszModes[] = { "Off", "On", "Automatic" };
static const char *s_pszPrimaries[] = { "FN2000", "P90", "Shotgun", "Fn303", "None", "Any" };
static const char *s_pszSecondaries[] = { "FiveseveN", "FNP9", "None", "Any" };

// Weapons, and the ammo a marine given one gets (magazines; the shotgun's are shells).
static const struct { const char *pszWeapon; const char *pszAmmo; int iAmmo; } s_Primaries[] =
{
	{ "weapon_fn2000", "AMMO_556", 2 },
	{ "weapon_p90", "AMMO_BULLETS", 2 },
	{ "weapon_shotgun", "AMMO_BUCKSHOT", 24 },
	{ "weapon_fn303", "XBowBolt", 4 },
}, s_Secondaries[] =
{
	{ "weapon_pistol", "AMMO_PISTOL", 3 },
	{ "weapon_pistol2", "AMMO_9MM", 3 },
};

// No primary and no pistol would leave the marines unarmed.
static void CheckLoadout( void )
{
	if ( hsm_pv_primaries.GetInt() == PV_PRIMARY_NONE && hsm_pv_secondaries.GetInt() == PV_SECONDARY_NONE )
		hsm_pv_secondaries.SetValue( PV_SECONDARY_ANY );
}

class CHiddenPluginPhysV : public CHiddenPlugin
{
public:
	CHiddenPluginPhysV() { m_bActive = false; }

	virtual void LevelInit( void ) { m_bActive = false; }

	virtual void RoundStart( void )
	{
		m_bActive = false;
		if ( hsm_pv_mode.GetInt() == PV_OFF )
			return;

		CTeam *pIRIS = GetGlobalTeam( TEAM_IRIS );
		if ( hsm_pv_mode.GetInt() == PV_AUTOMATIC )
		{
			if ( pIRIS && pIRIS->GetNumPlayers() > hsm_pv_amax.GetInt() )
			{
				UTIL_ClientPrintAll( HUD_PRINTTALK, "[PhysV] Physics Vs is in Automatic mode but there are too many players." );
				return;
			}

			// The plugin printed this whenever it was on, automatic or not.
			UTIL_ClientPrintAll( HUD_PRINTTALK, "[PhysV] Physics Vs is in Automatic mode." );
		}

		m_bActive = true;
		CheckLoadout();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
				SetMarineLoadout( pPlayer );
			else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
				HiddenPlugins_RemoveWeapon( pPlayer, "weapon_grenade" );
		}
	}

	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
	{
		if ( !m_bActive )
			return true;

		CHidden_Player *pHidden = ToHiddenPlayer( info.GetAttacker() );
		if ( !pHidden || pHidden == pVictim || pHidden->GetTeamNumber() != TEAM_HIDDEN )
			return true;

		// The knife only works on props.
		if ( info.GetDamageType() == DMG_SLASH )
		{
			HiddenPlugins_PrintChat( pHidden, "[PhysV] Physics Vs mode is Active. You cannot attack with the knife." );
			HiddenPlugins_PrintConsole( pHidden, "[PhysV] Physics Vs mode is Active. You cannot attack with the knife." );
			HiddenPlugins_PrintCenter( pHidden, "   [PhysV]\nCannot Attack\n   w/Knife" );
			return false;
		}

		return true;
	}

	// Every other hit is announced and feeds the Hidden.
	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		if ( !m_bActive )
			return;

		CHidden_Player *pHidden = ToHiddenPlayer( info.GetAttacker() );
		if ( !pHidden || pHidden == pVictim || pHidden->GetTeamNumber() != TEAM_HIDDEN )
			return;

		const int iDamage = RoundFloatToInt( info.GetDamage() );

		char szMessage[256];
		Q_snprintf( szMessage, sizeof( szMessage ), "[PhysV] %s hit %s for %i", pHidden->GetPlayerName(), pVictim->GetPlayerName(), iDamage );
		UTIL_ClientPrintAll( HUD_PRINTTALK, szMessage );

		if ( pHidden->GetHealth() < 100 )
			pHidden->SetHealth( MIN( pHidden->GetHealth() + RoundFloatToInt( info.GetDamage() * hsm_pv_regen.GetFloat() ), 100 ) );
	}

	void ModeChanged( void )
	{
		// Takes effect with a new round (as hdn_restartround), once a map is running.
		m_bActive = false;
		if ( HiddenRules() )
			HiddenRules()->RestartRound();
	}

private:
	void SetMarineLoadout( CHidden_Player *pPlayer )
	{
		const int iPrimary = hsm_pv_primaries.GetInt();
		const int iSecondary = hsm_pv_secondaries.GetInt();

		if ( iPrimary == PV_PRIMARY_NONE )
		{
			for ( int i = 0; i < ARRAYSIZE( s_Primaries ); i++ )
				HiddenPlugins_RemoveWeapon( pPlayer, s_Primaries[i].pszWeapon );

			SwitchToSecondary( pPlayer );
			pPlayer->SetPlayerClass( HIDDEN_CLASS_SUPPORT );
		}
		else if ( iPrimary < PV_PRIMARY_NONE )
		{
			if ( pPlayer->GetPrimary() != iPrimary )
			{
				for ( int i = 0; i < ARRAYSIZE( s_Primaries ); i++ )
					HiddenPlugins_RemoveWeapon( pPlayer, s_Primaries[i].pszWeapon );

				pPlayer->SetPrimary( iPrimary );
				pPlayer->GiveNamedItem( s_Primaries[iPrimary].pszWeapon );
				pPlayer->SetAmmoCount( s_Primaries[iPrimary].iAmmo, GetAmmoDef()->Index( s_Primaries[iPrimary].pszAmmo ) );

				CBaseCombatWeapon *pWeapon = pPlayer->Weapon_OwnsThisType( s_Primaries[iPrimary].pszWeapon );
				if ( pWeapon )
					pPlayer->Weapon_Switch( pWeapon );
			}

			pPlayer->SetPlayerClass( HIDDEN_CLASS_SUPPORT );
		}

		if ( iPrimary != PV_PRIMARY_NONE && iSecondary == PV_SECONDARY_NONE )
		{
			for ( int i = 0; i < ARRAYSIZE( s_Secondaries ); i++ )
				HiddenPlugins_RemoveWeapon( pPlayer, s_Secondaries[i].pszWeapon );
		}
		else if ( iSecondary < PV_SECONDARY_NONE && pPlayer->GetSecondary() != iSecondary )
		{
			for ( int i = 0; i < ARRAYSIZE( s_Secondaries ); i++ )
				HiddenPlugins_RemoveWeapon( pPlayer, s_Secondaries[i].pszWeapon );

			pPlayer->SetSecondary( iSecondary );
			pPlayer->GiveNamedItem( s_Secondaries[iSecondary].pszWeapon );
			pPlayer->SetAmmoCount( s_Secondaries[iSecondary].iAmmo, GetAmmoDef()->Index( s_Secondaries[iSecondary].pszAmmo ) );

			if ( iPrimary == PV_PRIMARY_NONE )
				SwitchToSecondary( pPlayer );
		}
	}

	void SwitchToSecondary( CHidden_Player *pPlayer )
	{
		for ( int i = 0; i < ARRAYSIZE( s_Secondaries ); i++ )
		{
			CBaseCombatWeapon *pWeapon = pPlayer->Weapon_OwnsThisType( s_Secondaries[i].pszWeapon );
			if ( pWeapon )
			{
				pPlayer->Weapon_Switch( pWeapon );
				return;
			}
		}
	}

	bool m_bActive;		// on this round
};

static CHiddenPluginPhysV s_PhysV;

static void PhysVModeChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( hsm_pv_mode.GetInt() != (int)flOldValue )
		s_PhysV.ModeChanged();
}

static void PhysVReply( const char *pszFormat, ... ) FMTFUNCTION( 1, 2 );
static void PhysVReply( const char *pszFormat, ... )
{
	char szText[1024];
	va_list args;
	va_start( args, pszFormat );
	Q_vsnprintf( szText, sizeof( szText ), pszFormat, args );
	va_end( args );

	CBasePlayer *pCaller = UTIL_GetCommandClient();
	if ( pCaller )
		ClientPrint( pCaller, HUD_PRINTCONSOLE, szText );
	else
		Msg( "%s", szText );
}

CON_COMMAND_F( hsm_pv, "Shows or adjusts Physics vs mode: hsm_pv [m(ode) #|a(utomax) #|p(rimary) #|s(econdary) #]", FCVAR_GAMEDLL )
{
	if ( !HiddenIsCommandIssuedByServerAdmin( "hsm_pv" ) )
		return;

	if ( args.ArgC() < 2 )
	{
		PhysVReply( "[PhysV] Status:\nMode         : %s\nAutomax      : At most %i IRIS to automatic mode.\nPrimary      : %s.\nSecondary    : %s.\nRegeneration : 1/%.2f of damage done.\n",
			s_pszModes[hsm_pv_mode.GetInt()], hsm_pv_amax.GetInt(), s_pszPrimaries[hsm_pv_primaries.GetInt()],
			s_pszSecondaries[hsm_pv_secondaries.GetInt()], hsm_pv_regen.GetFloat() > 0.0f ? 1.0f / hsm_pv_regen.GetFloat() : 0.0f );
		return;
	}

	bool bUsage = false;
	for ( int i = 1; i < args.ArgC() && !bUsage; i += 2 )
	{
		if ( i + 1 >= args.ArgC() )
		{
			bUsage = true;
			break;
		}

		const int iArg = atoi( args[i + 1] );
		switch ( args[i][0] )
		{
		case 'M': case 'm':
			if ( iArg < 0 || iArg > 2 )
			{
				bUsage = true;
				break;
			}
			PhysVReply( "[PhysV] hsm_pv_mode set to %s\n", s_pszModes[iArg] );
			hsm_pv_mode.SetValue( iArg );
			break;

		case 'A': case 'a':
			if ( iArg < 1 || iArg > 9 )
			{
				bUsage = true;
				break;
			}
			PhysVReply( "[PhysV] hsm_pv_amax set to %i\n", iArg );
			hsm_pv_amax.SetValue( iArg );
			break;

		case 'P': case 'p':
			if ( iArg < 0 || iArg > 5 )
			{
				bUsage = true;
				break;
			}
			PhysVReply( "[PhysV] hsm_pv_primaries set to %s\n", s_pszPrimaries[iArg] );
			hsm_pv_primaries.SetValue( iArg );
			break;

		case 'S': case 's':
		{
			if ( iArg < 0 || iArg > 3 )
			{
				bUsage = true;
				break;
			}

			int iSecondary = iArg;
			if ( iSecondary == PV_SECONDARY_NONE && hsm_pv_primaries.GetInt() == PV_PRIMARY_NONE )
			{
				PhysVReply( "[PhysV] hsm_pv_primaries is %s, hsm_pv_secondaries cannot be %s as well. Setting to %s.\n",
					s_pszPrimaries[PV_PRIMARY_NONE], s_pszSecondaries[PV_SECONDARY_NONE], s_pszSecondaries[PV_SECONDARY_ANY] );
				iSecondary = PV_SECONDARY_ANY;
			}
			else
			{
				PhysVReply( "[PhysV] hsm_pv_secondaries set to %s\n", s_pszSecondaries[iSecondary] );
			}
			hsm_pv_secondaries.SetValue( iSecondary );
			break;
		}

		default:
			bUsage = true;
			break;
		}
	}

	if ( bUsage )
	{
		PhysVReply( "[PhysV] Usage: hsm_pv [options]\n(m)ode #      : 0: %s, 1: %s, 2: %s.\n                Automatic mode will enable or disable depending on the number of playes.\n(a)utomax #   : Number of players for automatic mode.\n(p)rimary #   : Primary weapon.\n                0: %s, 1: %s, 2: %s, 3: %s, 4: %s, 5: %s.\n(s)econdary # : Secondary weapon.\n                0: %s, 1: %s, 2: %s, 3: %s.\n",
			s_pszModes[0], s_pszModes[1], s_pszModes[2],
			s_pszPrimaries[0], s_pszPrimaries[1], s_pszPrimaries[2], s_pszPrimaries[3], s_pszPrimaries[4], s_pszPrimaries[5],
			s_pszSecondaries[0], s_pszSecondaries[1], s_pszSecondaries[2], s_pszSecondaries[3] );
	}

	CheckLoadout();
}
