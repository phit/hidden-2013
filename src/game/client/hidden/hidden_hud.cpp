//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's HUD elements: the themed frames behind the stock health,
//			ammo and timer numbers, the Hidden's stamina bar, the location name
//			and the round timer. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "c_hidden_player.h"
#include "hidden_gamerules.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Every Beta 4b element hides with the stock health: while dead, without the suit or with the
// health hidden.
#define HIDDEN_HUD_HIDDEN_BITS	( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT )

static int GetLocalTeam( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	return pPlayer ? pPlayer->GetTeamNumber() : TEAM_UNASSIGNED;
}

//-----------------------------------------------------------------------------
// A frame: one hud_textures.txt icon stretched over the panel, for one team.
//-----------------------------------------------------------------------------
class CHudHiddenFrame : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudHiddenFrame, Panel );

public:
	CHudHiddenFrame( const char *pElementName, const char *pszPanelName, const char *pszIcon, int iTeam ) :
		CHudElement( pElementName ), BaseClass( NULL, pszPanelName ), m_pszIcon( pszIcon ), m_iTeam( iTeam ), m_pIcon( NULL )
	{
		SetParent( g_pClientMode->GetViewport() );
		SetHiddenBits( HIDDEN_HUD_HIDDEN_BITS );
	}

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_pIcon = gHUD.GetIcon( m_pszIcon );
		SetPaintBackgroundEnabled( false );
	}

	virtual void Paint( void )
	{
		if ( GetLocalTeam() == m_iTeam && m_pIcon )
			m_pIcon->DrawSelf( 0, 0, GetWide(), GetTall(), gHUD.m_clrNormal );
	}

	const char *m_pszIcon;
	int m_iTeam;
	CHudTexture *m_pIcon;
};

#define DECLARE_HIDDEN_FRAME( className, pszPanelName, pszIcon, iTeam )											\
	class className : public CHudHiddenFrame																\
	{																										\
	public:																									\
		className( const char *pElementName ) : CHudHiddenFrame( pElementName, pszPanelName, pszIcon, iTeam ) {}	\
	};																										\
	DECLARE_HUDELEMENT( className );

DECLARE_HIDDEN_FRAME( CHudHAmmo, "HudHAmmo", "HiddenAmmo", TEAM_HIDDEN );
DECLARE_HIDDEN_FRAME( CHudHHealth, "HudHHealth", "HiddenHealth", TEAM_HIDDEN );
DECLARE_HIDDEN_FRAME( CHudHTimer, "HudHTimer", "HiddenTimer", TEAM_HIDDEN );
DECLARE_HIDDEN_FRAME( CHudHStamina, "HudHStamina", "HiddenStaminaBG", TEAM_HIDDEN );
DECLARE_HIDDEN_FRAME( CHudIHealth, "HudIHealth", "IrisHealth", TEAM_IRIS );
DECLARE_HIDDEN_FRAME( CHudITimer, "HudITimer", "IrisTimer", TEAM_IRIS );
DECLARE_HIDDEN_FRAME( CHudILocation, "HudILocation", "IrisLocation", TEAM_IRIS );

//-----------------------------------------------------------------------------
// The Hidden's stamina: the bar texture, cropped to the stamina left, in white.
//-----------------------------------------------------------------------------
class CHudHStaminabar : public CHudHiddenFrame
{
public:
	CHudHStaminabar( const char *pElementName ) : CHudHiddenFrame( pElementName, "HudHStaminabar", "HiddenStaminabar", TEAM_HIDDEN ) {}

protected:
	virtual void Paint( void )
	{
		C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
		if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_HIDDEN || !m_pIcon )
			return;

		const float flFraction = pPlayer->GetStamina() * 0.01f;
		surface()->DrawSetTexture( m_pIcon->textureId );
		surface()->DrawSetColor( 255, 255, 255, 255 );
		surface()->DrawTexturedSubRect( 0, 0, (int)( GetWide() * flFraction ), GetTall(), 0.0f, 0.0f, flFraction, 1.0f );
	}
};

DECLARE_HUDELEMENT( CHudHStaminabar );

//-----------------------------------------------------------------------------
// The location name from the last location_brush the local player entered. The
// Hidden never sees it.
//-----------------------------------------------------------------------------
class CHudLocation : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudLocation, Panel );

public:
	CHudLocation( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudLocation" )
	{
		SetParent( g_pClientMode->GetViewport() );
		SetHiddenBits( HIDDEN_HUD_HIDDEN_BITS );
		m_pLabel = new Label( this, "LocationLabel", "" );
		m_szLocation[0] = '\0';
	}

	virtual void Init( void )
	{
		ListenForGameEvent( "player_location" );
	}

	virtual void LevelInit( void )
	{
		m_szLocation[0] = '\0';
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		C_BasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
		if ( !pPlayer || pPlayer != C_BasePlayer::GetLocalPlayer() )
			return;

		Q_strncpy( m_szLocation, event->GetString( "location" ), sizeof( m_szLocation ) );
		DevMsg( 1, "location is : %s\n", m_szLocation );
	}

protected:
	virtual void OnThink( void )
	{
		SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetFgColor( Color( 184, 224, 232, 255 ) );
		m_pLabel->SetText( m_szLocation );
		m_pLabel->SetVisible( GetLocalTeam() != TEAM_HIDDEN );
		m_pLabel->SetSize( GetWide(), GetTall() );
	}

private:
	Label *m_pLabel;
	char m_szLocation[HIDDEN_LOCATION_LENGTH];
};

DECLARE_HUDELEMENT( CHudLocation );

//-----------------------------------------------------------------------------
// The seconds left in the round, with Beta 4b's HudAnimations.txt events.
//-----------------------------------------------------------------------------
class CHudRoundTimer : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CHudRoundTimer, CHudNumericDisplay );

public:
	CHudRoundTimer( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay( NULL, "HudRoundTimer" ),
		m_flRoundStart( -1.0f ), m_iLastSeconds( -1 )
	{
		SetHiddenBits( HIDDEN_HUD_HIDDEN_BITS );
		SetPaintEnabled( false );
	}

	virtual void LevelInit( void )
	{
		m_flRoundStart = -1.0f;
		m_iLastSeconds = -1;
		SetPaintEnabled( false );
	}

protected:
	virtual void OnThink( void )
	{
		if ( GetLocalTeam() == TEAM_HIDDEN )
			SetFgColor( Color( 228, 207, 154, 200 ) );

		CHiddenRules *pRules = HiddenRules();
		if ( !pRules )
			return;

		const int iSeconds = pRules->GetRoundTimerRemain();
		if ( iSeconds == m_iLastSeconds || ( iSeconds < 0 && m_iLastSeconds < 0 ) )
			return;

		if ( iSeconds < 0 )
		{
			SetPaintEnabled( false );
			SetPaintBackgroundEnabled( false );
			m_iLastSeconds = -1;
			return;
		}

		// A new round.
		if ( pRules->GetRoundStart() != m_flRoundStart || m_iLastSeconds < 0 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundTimerInit" );
			SetPaintEnabled( true );
			SetPaintBackgroundEnabled( false );
			m_flRoundStart = pRules->GetRoundStart();
		}

		if ( m_iLastSeconds == 20 )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundTimerBelow20" );
		else if ( iSeconds < 10 )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundTimerPulse" );

		// Beta 4b's name for it; its HudAnimations.txt calls the event RoundTimerBelow5, so it
		// never plays.
		if ( iSeconds == 5 )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundimerBelow5" );

		m_iLastSeconds = iSeconds;
		SetDisplayValue( iSeconds );
	}

private:
	float m_flRoundStart;
	int m_iLastSeconds;
};

DECLARE_HUDELEMENT( CHudRoundTimer );
