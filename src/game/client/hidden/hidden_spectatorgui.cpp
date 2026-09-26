//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's spectator overlay (Resource/UI/Spectator.res): the round
//			clock, "Location :" with the camera's location or the watched marine's
//			name, and the blinking REC. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "hl2mptextwindow.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include "hidden_gamerules.h"
#include "hidden_spectator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

class CHiddenSpectatorGUI : public CHL2MPSpectatorGUI
{
	DECLARE_CLASS_SIMPLE( CHiddenSpectatorGUI, CHL2MPSpectatorGUI );

public:
	CHiddenSpectatorGUI( IViewPort *pViewPort ) : CHL2MPSpectatorGUI( pViewPort ) {}

	virtual void Update( void );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void OnThink( void );

private:
	void SetLabelStyle( IScheme *pScheme, const char *pszName, Color color );
};

void CHiddenSpectatorGUI::SetLabelStyle( IScheme *pScheme, const char *pszName, Color color )
{
	Label *pLabel = dynamic_cast<Label *>( FindChildByName( pszName ) );
	if ( !pLabel )
		return;

	pLabel->SetFgColor( color );
	pLabel->SetFont( pScheme->GetFont( "SpecNumbers", IsProportional() ) );
}

void CHiddenSpectatorGUI::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetLabelStyle( pScheme, "timerlabel", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( pScheme, "timerclock", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( pScheme, "location", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( pScheme, "cameralocation", Color( 0, 255, 0, 150 ) );
}

void CHiddenSpectatorGUI::Update( void )
{
	BaseClass::Update();

	// Beta 4b never filled in the player label; the location shows who's being watched instead.
	m_pPlayerLabel->SetVisible( false );

	const char *pszLocation = "";

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseEntity *pTarget = pLocalPlayer ? pLocalPlayer->GetObserverTarget() : NULL;
	if ( pTarget && pTarget->IsPlayer() )
	{
		pszLocation = ToBasePlayer( pTarget )->GetPlayerName();
	}
	else
	{
		C_HiddenSpectatorPoint *pCamera = dynamic_cast<C_HiddenSpectatorPoint *>( pTarget );
		if ( pCamera )
			pszLocation = pCamera->GetLocation();
	}

	SetLabelText( "cameralocation", pszLocation );
}

void CHiddenSpectatorGUI::OnThink( void )
{
	// The round clock, every frame.
	CHiddenRules *pRules = HiddenRules();
	const int iSeconds = pRules ? MAX( pRules->GetRoundTimerRemain(), 0 ) : 0;

	wchar_t szTime[64];
	V_swprintf_safe( szTime, L"%d:%02d", iSeconds / 60, iSeconds % 60 );
	SetLabelText( "timerlabel", szTime );

	BaseClass::OnThink();
}

IViewPortPanel *CreateHiddenSpectatorGUI( IViewPort *pViewPort )
{
	return new CHiddenSpectatorGUI( pViewPort );
}
