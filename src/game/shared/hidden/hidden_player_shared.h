//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden player under one name in shared code.
//
//=============================================================================//

#ifndef HIDDEN_PLAYER_SHARED_H
#define HIDDEN_PLAYER_SHARED_H
#pragma once

#ifdef CLIENT_DLL
	#include "c_hidden_player.h"
	#define CHidden_Player C_Hidden_Player
#else
	#include "hidden_player.h"
#endif

#endif // HIDDEN_PLAYER_SHARED_H
