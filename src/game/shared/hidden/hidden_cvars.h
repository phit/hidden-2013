//========= Hidden: Source =====================================================//
//
// Purpose: Hidden server cvars, replicated to clients (see docs/spec/cvars.md).
//
//=============================================================================//

#ifndef HIDDEN_CVARS_H
#define HIDDEN_CVARS_H
#pragma once

#include "convar.h"

extern ConVar mp_roundtime;
extern ConVar hdn_jointime;
extern ConVar hdn_hiddenrounds;
extern ConVar hdn_selectmethod;
extern ConVar hdn_staminadrain;
extern ConVar hdn_limitbombs;
extern ConVar hdn_radio_limit;
extern ConVar hdn_deathnotices;
extern ConVar sv_pigstick;
extern ConVar hdn_deathwait;
extern ConVar hdn_survivaltime;

#endif // HIDDEN_CVARS_H
