//========= Hidden: Source =====================================================//
//
// Purpose: Hidden server cvars, replicated to clients (see docs/spec/cvars.md).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mp_roundtime( "mp_roundtime", "240", FCVAR_REPLICATED | FCVAR_NOTIFY, "Length of a round, in seconds", true, 1.0f, false, 0.0f );
ConVar hdn_jointime( "hdn_jointime", "15", FCVAR_REPLICATED | FCVAR_NOTIFY, "Seconds to wait after a map change before the first round starts", true, 0.0f, false, 0.0f );
ConVar hdn_hiddenrounds( "hdn_hiddenrounds", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Rounds in a row the same player can be the Hidden before someone else is picked; 0 is unlimited", true, 0.0f, false, 0.0f );
ConVar hdn_selectmethod( "hdn_selectmethod", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "How the Hidden is picked: 0 by weighting points, 1 classic (whoever killed the Hidden), 2 random", true, 0.0f, true, 2.0f );
ConVar hdn_staminadrain( "hdn_staminadrain", "-0.095", FCVAR_REPLICATED | FCVAR_NOTIFY, "Stamina the Hidden gains per tick while clinging to a wall; negative, so clinging drains it" );
ConVar hdn_limitbombs( "hdn_limitbombs", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Give the Hidden half as many pipe bombs as there are marines (rounded up), less one, instead of 3" );
ConVar hdn_radio_limit( "hdn_radio_limit", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Seconds a player has to wait between radio messages and taunts", true, 0.0f, false, 0.0f );
ConVar hdn_hidehiddendecals( "hdn_hidehiddendecals", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Keep blood and bullet impact decals off the Hidden; Beta 4b let them show where he'd been hit" );
ConVar hdn_deathnotices( "hdn_deathnotices", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Show the Hidden's kills in the kill feed (the server log always has them)" );
ConVar sv_pigstick( "sv_pigstick", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Enables the Hidden's pigstick attack" );
ConVar hdn_deathwait( "hdn_deathwait", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "OverRun: seconds before a killed player comes back (four times as long after a marine's suicide)" );
ConVar hdn_survivaltime( "hdn_survivaltime", "15", FCVAR_REPLICATED | FCVAR_NOTIFY, "OverRun: seconds the last marine standing has to survive" );
