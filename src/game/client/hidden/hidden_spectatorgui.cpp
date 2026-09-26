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
#include <vgui/ISurface.h>
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
	CHiddenSpectatorGUI( IViewPort *pViewPort ) : CHL2MPSpectatorGUI( pViewPort ), m_hFont( INVALID_FONT ) {}

	virtual void Update( void );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PerformLayout( void );
	virtual void OnThink( void );

private:
	void StyleLabels( void );
	void SetLabelStyle( const char *pszName, Color color );
	void PlaceAfterCaption( const char *pszCaption, const char *pszValue );

	HFont m_hFont;
};

void CHiddenSpectatorGUI::SetLabelStyle( const char *pszName, Color color )
{
	Label *pLabel = dynamic_cast<Label *>( FindChildByName( pszName ) );
	if ( !pLabel )
		return;

	if ( m_hFont != INVALID_FONT && pLabel->GetFont() != m_hFont )
	{
		pLabel->SetFont( m_hFont );
		pLabel->InvalidateLayout();	// fit the text to the new font, or it's cut short
	}
	pLabel->SetFgColor( color );
}

// Beta 4b's white captions and clock, the location in green. Its panel styled the labels after
// their own scheme pass; in SDK 2013 theirs runs last and resets them to the scheme's label colour
// and font, so this runs again every think.
void CHiddenSpectatorGUI::StyleLabels( void )
{
	SetLabelStyle( "timerlabel", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( "timerclock", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( "location", Color( 255, 255, 255, 255 ) );
	SetLabelStyle( "cameralocation", Color( 0, 255, 0, 150 ) );
}

void CHiddenSpectatorGUI::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Beta 4b asks for SpecNumbers (OratorStd 30) unscaled: 30 pixels at any resolution. Deviation:
	// scale it with the screen, the size Beta 4b's had at 800x600 (the scheme's own scaling, from
	// 480 lines, came out a quarter bigger than that).
	int iScreenWide, iScreenTall;
	surface()->GetScreenSize( iScreenWide, iScreenTall );
	if ( m_hFont == INVALID_FONT )
		m_hFont = surface()->CreateFont();
	surface()->SetFontGlyphSet( m_hFont, "OratorStd", MAX( 30 * iScreenTall / 600, 12 ), 500, 0, 0,
		ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_ADDITIVE | ISurface::FONTFLAG_CUSTOM );

	InvalidateLayout();	// the styles and the captions' widths, in PerformLayout
}

// Spectator.res leaves 60 (clock) and 90 (location) units of 640 for the captions, less than
// "Time :" and "Location :" take in OratorStd at that size (Beta 4b cut the captions to "T..." and
// "Lo..." instead). Keep Beta 4b's positions where they fit, otherwise
// start the value just after its caption.
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

	StyleLabels();

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

	StyleLabels();

	BaseClass::OnThink();
}

IViewPortPanel *CreateHiddenSpectatorGUI( IViewPort *pViewPort )
{
	return new CHiddenSpectatorGUI( pViewPort );
}
