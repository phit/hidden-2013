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
	virtual void PerformLayout( void );
	virtual void OnThink( void );

private:
	void SetLabelStyle( IScheme *pScheme, const char *pszName, Color color );
	void PlaceAfterCaption( const char *pszCaption, const char *pszValue );
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

	InvalidateLayout();	// the styles and the captions' widths, in PerformLayout
}

// Spectator.res leaves 60 (clock) and 90 (location) units of 640 for the captions, less than
// "Time :" and "Location :" take in the 30-tall OratorStd, so the values ran into them. Keep
// Beta 4b's positions where they fit, otherwise start the value just after its caption.
void CHiddenSpectatorGUI::PlaceAfterCaption( const char *pszCaption, const char *pszValue )
{
	Label *pCaption = dynamic_cast<Label *>( FindChildByName( pszCaption ) );
	Panel *pValue = FindChildByName( pszValue );
	if ( !pCaption || !pValue )
		return;

	int iCaptionX, iCaptionY, iCaptionWide, iCaptionTall, iValueX, iValueY;
	pCaption->GetPos( iCaptionX, iCaptionY );
	pCaption->GetContentSize( iCaptionWide, iCaptionTall );
	pValue->GetPos( iValueX, iValueY );

	const int iMinX = iCaptionX + iCaptionWide + scheme()->GetProportionalScaledValueEx( GetScheme(), 4 );
	if ( iValueX < iMinX )
	{
		pValue->SetPos( iMinX, iValueY );
		iValueX = iMinX;
	}

	// Room to the right edge, so long location names aren't cut short.
	pValue->SetWide( MAX( pValue->GetWide(), GetWide() - iValueX ) );
}

void CHiddenSpectatorGUI::PerformLayout( void )
{
	BaseClass::PerformLayout();

	// Here rather than in ApplySchemeSettings: the labels' own scheme pass runs after ours and
	// turned them orange. Beta 4b's: white captions and clock, the location in green.
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	SetLabelStyle( pScheme, "timerlabel", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( pScheme, "timerclock", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( pScheme, "location", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( pScheme, "cameralocation", Color( 0, 255, 0, 150 ) );

	PlaceAfterCaption( "timerclock", "timerlabel" );	// Beta 4b's names: timerclock is the caption
	PlaceAfterCaption( "location", "cameralocation" );
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
