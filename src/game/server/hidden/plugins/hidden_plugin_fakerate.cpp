//========= Hidden: Source =====================================================//
//
// Purpose: Fake Tickrate (Paegus's hsm_fakerate 1.0.1): the Hidden's stamina
//			regenerates and drains as at another tickrate. See
//			docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=87856
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The plugin topped the stamina up every frame from the server. The stamina is predicted now, so
// the rates themselves are scaled instead (HiddenStaminaTickScale), and the client has this cvar too.
// The plugin defaulted to the server's own tickrate; 0 means that here.
ConVar hsm_fr_tick( "hsm_fr_tick", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "The effective tickrate the Hidden's stamina flows at. 0: the server's own", true, 0.0f, true, 100.0f );
