//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENTMODE_HLNORMAL_H )
#define CLIENTMODE_HLNORMAL_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/Cursor.h>

class CHudViewport;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class ClientModeHL2MPNormal : public ClientModeShared
{
public:
	DECLARE_CLASS( ClientModeHL2MPNormal, ClientModeShared );

	ClientModeHL2MPNormal();
	~ClientModeHL2MPNormal();

	virtual void	Init();
	virtual int		GetDeathMessageStartHeight( void );
#ifdef HIDDEN
	// Beta 4b's client mode (the 2006 SDK template's ClientModeSDKNormal) draws viewmodels at 74,
	// whatever viewmodel_fov says; HL2MP's 54 shows them too close and too low.
	virtual float	GetViewModelFOV( void ) { return 74.0f; }
#endif
};

extern IClientMode *GetClientModeNormal();
extern vgui::HScheme g_hVGuiCombineScheme;

extern ClientModeHL2MPNormal* GetClientModeHL2MPNormal();

#endif // CLIENTMODE_HLNORMAL_H
