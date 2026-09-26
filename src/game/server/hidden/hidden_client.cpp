//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== hidden_client.cpp ====================================================

  Hidden client/server game specific stuff (from hl2mp_client.cpp)

*/

#include "cbase.h"
#include "hidden_player.h"
#include "hidden_gamerules.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "team.h"
#include "viewport_panel_names.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

ConVar sv_motd_unload_on_dismissal( "sv_motd_unload_on_dismissal", "0", 0, "If enabled, the MOTD contents will be unloaded when the player closes the MOTD." );

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

void FinishClientPutInServer( CHL2MP_Player *pPlayer )
{
	pPlayer->InitialSpawn();
	pPlayer->Spawn();


	char sName[128];
	Q_strncpy( sName, pPlayer->GetPlayerName(), sizeof( sName ) );
	
	// First parse the name and remove any %'s
	for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
	{
		// Replace it with a space
		if ( *pApersand == '%' )
				*pApersand = ' ';
	}

	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>" );

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, "You are on team %s1\n", pPlayer->GetTeam()->GetName() );
	}

	const ConVar *hostname = cvar->FindVar( "hostname" );
	const char *title = (hostname) ? hostname->GetString() : "MESSAGE OF THE DAY";

	KeyValues *data = new KeyValues("data");
	data->SetString( "title", title );		// info panel title
	data->SetString( "type", "1" );			// show userdata from stringtable entry
	data->SetString( "msg",	"motd" );		// use this stringtable entry
	data->SetInt( "cmd", TEXTWINDOW_CMD_CHOOSETEAM );	// then the team menu, as Beta 4b's "classmenu"
	data->SetBool( "unload", sv_motd_unload_on_dismissal.GetBool() );

	pPlayer->ShowViewPortPanel( PANEL_INFO, true, data );

	data->deleteThis();
}

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CHidden_Player *pPlayer = CHidden_Player::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CHL2MP_Player *pPlayer = ToHL2MPPlayer( CBaseEntity::Instance( pEdict ) );
	FinishClientPutInServer( pPlayer );
}


/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Hidden : Source";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	CBaseEntity::PrecacheModel("models/player.mdl");

	// The Hidden player models; players are created after the precache phase.
	CBaseEntity::PrecacheModel( "models/player/iris.mdl" );
	CBaseEntity::PrecacheModel( "models/player/iris_supply.mdl" );
	CBaseEntity::PrecacheModel( "models/manor/mn_fixture1.mdl" );
	CBaseEntity::PrecacheModel( "models/player/hidden.mdl" );
	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );
	CBaseEntity::PrecacheModel ("models/weapons/v_hands.mdl");

	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );

	CBaseEntity::PrecacheScriptSound( "FX_AntlionImpact.ShellImpact" );
	CBaseEntity::PrecacheScriptSound( "Missile.ShotDown" );
	CBaseEntity::PrecacheScriptSound( "Bullets.DefaultNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.GunshipNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.StriderNearmiss" );
	
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepHigh" );
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepLow" );
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( pEdict );

	if ( pPlayer )
	{
		if ( gpGlobals->curtime > pPlayer->GetDeathTime() + DEATH_ANIMATION_TIME )
		{		
			// respawn player
			pPlayer->Spawn();			
		}
		else
		{
			pPlayer->SetNextThink( gpGlobals->curtime + 0.1f );
		}
	}
}

void Bot_RunAll( void );	// hidden_bot.cpp

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);

	Bot_RunAll();
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	CreateGameRulesObject( "CHiddenRules" );
}


//=========================================================
// Beta 4b's server commands (docs/spec/cvars.md)
//=========================================================
CON_COMMAND_F( hdn_restartround, "Restarts the round", FCVAR_GAMEDLL )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	HiddenRules()->RestartRound();
}

// A debug leftover anyone can use: stuns yourself, which blurs your screen.
CON_COMMAND( blur, "blurs the screen" )
{
	CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_GetCommandClient() );
	if ( pPlayer )
		pPlayer->Stun( pPlayer );
}

// The prefix a player had alive, without "(Dead)"; used for the text below.
const char *HiddenGetDeadChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return "";

	switch ( pPlayer->GetTeamNumber() )
	{
	case TEAM_HIDDEN:	return "(Hidden)";
	case TEAM_IRIS:		return "(IRIS)";
	default:			return "";
	}
}

extern bool g_bHiddenDeadSay;
extern void Host_Say( edict_t *pEdict, const CCommand &args, bool teamonly );

// setposx and setposy aren't position commands: the client sends what its player was typing when
// they died through these, to everyone (x) or the team (y), as if they'd said it alive.
static void HiddenDeadSay( const CCommand &args, bool bTeamOnly )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( !pPlayer || !pPlayer->CanPlayerTalk() || args.ArgC() < 2 )
		return;

	CCommand sayArgs;
	char szSay[256];
	Q_snprintf( szSay, sizeof( szSay ), "%s %s", bTeamOnly ? "say_team" : "say", args.ArgS() );
	sayArgs.Tokenize( szSay );

	g_bHiddenDeadSay = true;
	Host_Say( pPlayer->edict(), sayArgs, bTeamOnly );
	g_bHiddenDeadSay = false;

	pPlayer->NotePlayerTalked();
}

CON_COMMAND( setposx, "" )
{
	HiddenDeadSay( args, false );
}

CON_COMMAND( setposy, "" )
{
	HiddenDeadSay( args, true );
}
