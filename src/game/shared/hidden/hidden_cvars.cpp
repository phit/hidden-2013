//========= Hidden: Source =====================================================//
//
// Purpose: Hidden server cvars, replicated to clients (see docs/spec/cvars.md).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mp_roundtime( "mp_roundtime", "240", FCVAR_REPLICATED | FCVAR_NOTIFY, "length of a round in seconds", true, 1.0f, false, 0.0f );
ConVar hdn_jointime( "hdn_jointime", "15", FCVAR_REPLICATED | FCVAR_NOTIFY, "delay, in seconds, before attempting to start the first round after a map change", true, 0.0f, false, 0.0f );
ConVar hdn_hiddenrounds( "hdn_hiddenrounds", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "0 is unlimited", true, 0.0f, false, 0.0f );
ConVar hdn_selectmethod( "hdn_selectmethod", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "0 = weighted, 1 = classic, 2 = random - defaults to weighted", true, 0.0f, true, 2.0f );
ConVar hdn_staminadrain( "hdn_staminadrain", "-0.095", FCVAR_REPLICATED | FCVAR_NOTIFY, "should be negative" );
ConVar hdn_limitbombs( "hdn_limitbombs", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "limits the number of hidden bombs if enabled" );
ConVar hdn_radio_limit( "hdn_radio_limit", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "time limit, in seconds, between radio messages / taunts", true, 0.0f, false, 0.0f );
ConVar hdn_deathnotices( "hdn_deathnotices", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "display death notices" );
ConVar sv_pigstick( "sv_pigstick", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "enables the Hidden's pigstick attack" );
ConVar hdn_deathwait( "hdn_deathwait", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Time between respawns in OverRun" );
ConVar hdn_survivaltime( "hdn_survivaltime", "15", FCVAR_REPLICATED | FCVAR_NOTIFY, "survival time limit" );
