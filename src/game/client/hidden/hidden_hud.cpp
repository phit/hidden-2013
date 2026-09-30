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
#include "hidden_cvars.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
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
// Beta 4b's health: HL2's number without the label, placed each frame by team
// in 640x480 units stretched to the screen (the Hidden at 40,445 in its tan,
// marines at 25,447 in the scheme colour), with HL2's health animations and
// its Damage message flash.
//-----------------------------------------------------------------------------
class CHudHealth : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CHudHealth, CHudNumericDisplay );

public:
	CHudHealth( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay( NULL, "HudHealth" ), m_iHealth( -1 )
	{
		SetHiddenBits( HIDDEN_HUD_HIDDEN_BITS );
	}

	virtual void Init( void );
	virtual void VidInit( void ) { Reset(); }

	virtual void Reset( void )
	{
		m_iHealth = -1;
		SetLabelText( L"" );
		SetDisplayValue( 100 );
	}

	void MsgFunc_Damage( bf_read &msg )
	{
		msg.ReadByte();	// armor
		const int iDamageTaken = msg.ReadByte();

		if ( iDamageTaken > 0 )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthDamageTaken" );
	}

protected:
	virtual void OnThink( void )
	{
		int iWide, iTall;
		g_pClientMode->GetViewport()->GetSize( iWide, iTall );

		if ( GetLocalTeam() == TEAM_HIDDEN )
		{
			SetPos( (int)( iWide * ( 40.0f / 640.0f ) ), (int)( iTall * ( 445.0f / 480.0f ) ) );
			SetFgColor( Color( 228, 207, 154, 200 ) );
		}
		else
		{
			SetPos( (int)( iWide * ( 25.0f / 640.0f ) ), (int)( iTall * ( 447.0f / 480.0f ) ) );
		}

		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		const int iHealth = pPlayer ? MAX( pPlayer->GetHealth(), 0 ) : 0;
		if ( iHealth == m_iHealth )
			return;

		m_iHealth = iHealth;

		if ( iHealth >= 20 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthIncreasedAbove20" );
		}
		else if ( iHealth > 0 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthIncreasedBelow20" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthLow" );
		}

		SetDisplayValue( iHealth );
	}

private:
	int m_iHealth;
};

DECLARE_HUDELEMENT( CHudHealth );
DECLARE_HUD_MESSAGE( CHudHealth, Damage );

void CHudHealth::Init( void )
{
	HOOK_HUD_MESSAGE( CHudHealth, Damage );
	Reset();
}

//-----------------------------------------------------------------------------
// The location name from the last location_brush the local player entered. The
// Hidden never sees it. Read from the player's networked location rather than the
// player_location event, which is missed when the player spawns inside the brush
// they were last in (no change, no event) or before the HUD is listening.
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
		// Centred: HudLayout.res's box (x 239, 160 wide) and its HudILocation frame (x 263, 115 wide)
		// share a centre, so the name sits in the middle of the frame.
		m_pLabel->SetContentAlignment( Label::a_center );
	}

protected:
	virtual void OnThink( void )
	{
		C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();

		SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetFgColor( Color( 184, 224, 232, 255 ) );
		m_pLabel->SetText( pPlayer ? pPlayer->GetCurrentLocation() : "" );
		m_pLabel->SetVisible( GetLocalTeam() != TEAM_HIDDEN );
		m_pLabel->SetSize( GetWide(), GetTall() );
	}

private:
	Label *m_pLabel;
};

DECLARE_HUDELEMENT( CHudLocation );

//-----------------------------------------------------------------------------
// The time left in the round (mm.ss), with Beta 4b's HudAnimations.txt events.
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
	// Beta 4b's timer was a CHudBaseTimer (its 2006 version), which prints the value as minutes and
	// seconds, "%02d.%02d": 03.42, not 222.
	virtual void PaintNumbers( HFont font, int xpos, int ypos, int value )
	{
		wchar_t szTime[16];
		V_snwprintf( szTime, ARRAYSIZE( szTime ), L"%02d.%02d", value / 60, value % 60 );

		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextPos( xpos, ypos );
		surface()->DrawUnicodeString( szTime );
	}

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

//-----------------------------------------------------------------------------
// The player under the crosshair: the Hidden sees anyone within 256 units as
// "Enemy", marines see other marines as "Friend" and nothing on the Hidden.
// hdn_targetnames 0 turns it off.
//-----------------------------------------------------------------------------
class CHudName : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudName, Panel );

public:
	CHudName( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudName" )
	{
		SetParent( g_pClientMode->GetViewport() );
		SetHiddenBits( HIDDEN_HUD_HIDDEN_BITS );
		m_pLabel = new Label( this, "NameLabel", "" );
	}

	virtual void Reset( void )
	{
		m_hTarget = NULL;
	}

protected:
	virtual void OnThink( void )
	{
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal )
			return;

		// What's under the crosshair.
		Vector vecForward;
		AngleVectors( pLocal->EyeAngles(), &vecForward );
		const float flRange = ( pLocal->GetTeamNumber() == TEAM_HIDDEN ) ? 256.0f : 8192.0f;
		const Vector vecEyes = pLocal->EyePosition();

		trace_t tr;
		UTIL_TraceLine( vecEyes, vecEyes + vecForward * flRange, MASK_SHOT, pLocal, COLLISION_GROUP_NONE, &tr );
		if ( hdn_targetnames.GetBool() && tr.m_pEnt && tr.m_pEnt->IsPlayer() && tr.m_pEnt != pLocal )
			m_hTarget = static_cast<C_BasePlayer *>( tr.m_pEnt );
		else
			Reset();

		SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );

		C_BasePlayer *pTarget = m_hTarget;
		wchar_t wszText[128] = L"";
		if ( pTarget )
		{
			const char *pszFormat = NULL;
			if ( pLocal->GetTeamNumber() == TEAM_HIDDEN )
				pszFormat = "Enemy: %s - %i";
			else if ( pTarget->GetTeamNumber() != TEAM_HIDDEN )
				pszFormat = "Friend: %s - %i";

			if ( pszFormat )
			{
				char szText[128];
				Q_snprintf( szText, sizeof( szText ), pszFormat, pTarget->GetPlayerName(), pTarget->GetHealth() );
				g_pVGuiLocalize->ConvertANSIToUnicode( szText, wszText, sizeof( wszText ) );
			}
		}

		m_pLabel->SetFgColor( Color( 227, 189, 0, 255 ) );
		m_pLabel->SetText( wszText );
		m_pLabel->SetVisible( pTarget != NULL );
		m_pLabel->SetSize( GetWide(), GetTall() );
	}

private:
	Label *m_pLabel;
	CHandle<C_BasePlayer> m_hTarget;
};

DECLARE_HUDELEMENT( CHudName );

//-----------------------------------------------------------------------------
// The marines' radar: the other living marines (highlighted for 5 s after they
// use the radio) and sonic alarms that went off, around the lower middle of
// the panel, turned with the view.
//-----------------------------------------------------------------------------
#define RADAR_REFRESH		0.1f	// how often the marine list is rebuilt
#define RADAR_SCALE			0.15f	// pixels per unit
#define RADAR_RANGE			130.0f	// pixels; the distance includes height
#define RADAR_ICON_SIZE		20
#define RADAR_FLASH_TIME	5.0f	// radio highlight and alarm lifetime

class CHudRadar : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudRadar, Panel );

public:
	CHudRadar( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudRadar" ),
		m_flNextRefresh( 0.0f )
	{
		SetParent( g_pClientMode->GetViewport() );
		SetHiddenBits( HIDDEN_HUD_HIDDEN_BITS );

		for ( int i = 0; i < ARRAYSIZE( m_pIcons ); i++ )
			m_pIcons[i] = NULL;
		m_pSonicIcon = NULL;
		Reset();
	}

	virtual void Init( void )
	{
		ListenForGameEvent( "iris_radio" );
		ListenForGameEvent( "alarm_trigger" );
	}

	virtual void Reset( void )
	{
		m_Marines.RemoveAll();
		m_Alarms.RemoveAll();
		m_flNextRefresh = 0.0f;
		for ( int i = 0; i < ARRAYSIZE( m_flRadioTime ); i++ )
			m_flRadioTime[i] = 0.0f;
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		if ( FStrEq( event->GetName(), "iris_radio" ) )
		{
			C_BasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
			if ( pPlayer )
				m_flRadioTime[pPlayer->entindex()] = gpGlobals->curtime + RADAR_FLASH_TIME;
			return;
		}

		// A sonic alarm: a new icon, or the one already there shows for another 5 s.
		const Vector vecPos( event->GetFloat( "posx" ), event->GetFloat( "posy" ), event->GetFloat( "posz" ) );
		for ( int i = 0; i < m_Alarms.Count(); i++ )
		{
			if ( m_Alarms[i].vecPos == vecPos )
			{
				m_Alarms[i].flExpires = gpGlobals->curtime + RADAR_FLASH_TIME;
				return;
			}
		}

		RadarAlarm_t alarm;
		alarm.vecPos = vecPos;
		alarm.flExpires = gpGlobals->curtime + RADAR_FLASH_TIME;
		m_Alarms.AddToTail( alarm );
	}

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_pIcons[0] = gHUD.GetIcon( "AMarineIcon" );
		m_pIcons[1] = gHUD.GetIcon( "SMarineIcon" );
		m_pIcons[2] = gHUD.GetIcon( "AMarineEIcon" );
		m_pIcons[3] = gHUD.GetIcon( "SMarineEIcon" );
		m_pSonicIcon = gHUD.GetIcon( "SonicIcon" );
		SetPaintBackgroundEnabled( false );
	}

	virtual void OnThink( void )
	{
		if ( gpGlobals->curtime < m_flNextRefresh )
			return;

		m_flNextRefresh = gpGlobals->curtime + RADAR_REFRESH;
		m_Marines.RemoveAll();

		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			C_Hidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || pPlayer == pLocal || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() == TEAM_HIDDEN )
				continue;

			RadarMarine_t marine;
			marine.vecPos = pPlayer->GetAbsOrigin();
			marine.iClass = pPlayer->GetPlayerClass();
			marine.iIndex = i;
			m_Marines.AddToTail( marine );
		}
	}

	virtual void Paint( void )
	{
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal || pLocal->GetTeamNumber() != TEAM_IRIS )
			return;

		for ( int i = 0; i < m_Marines.Count(); i++ )
		{
			const RadarMarine_t &marine = m_Marines[i];
			const bool bRadio = m_flRadioTime[marine.iIndex] > gpGlobals->curtime;
			const int iIcon = ( marine.iClass == HIDDEN_CLASS_SUPPORT ? 1 : 0 ) + ( bRadio ? 2 : 0 );
			DrawBlip( pLocal, marine.vecPos, m_pIcons[iIcon], gHUD.m_clrNormal );
		}

		for ( int i = m_Alarms.Count() - 1; i >= 0; i-- )
		{
			if ( m_Alarms[i].flExpires <= gpGlobals->curtime )
				m_Alarms.Remove( i );
			else
				DrawBlip( pLocal, m_Alarms[i].vecPos, m_pSonicIcon, Color( 255, 255, 255, 255 ) );
		}
	}

private:
	struct RadarMarine_t
	{
		Vector vecPos;
		int iClass;
		int iIndex;
	};

	struct RadarAlarm_t
	{
		Vector vecPos;
		float flExpires;
	};

	// Forward is up, centred at half the width and 80 % of the height.
	void DrawBlip( C_BasePlayer *pLocal, const Vector &vecPos, CHudTexture *pIcon, const Color &clr )
	{
		if ( !pIcon )
			return;

		const Vector vecDelta = ( vecPos - pLocal->GetAbsOrigin() ) * RADAR_SCALE;
		if ( vecDelta.Length() > RADAR_RANGE )
			return;

		float flSin, flCos;
		SinCos( DEG2RAD( pLocal->EyeAngles()[YAW] - 90.0f ), &flSin, &flCos );
		const float flX = vecDelta.x * flCos + vecDelta.y * flSin;
		const float flY = vecDelta.x * flSin - vecDelta.y * flCos;

		const int x = (int)( GetWide() * 0.5f + flX ) - RADAR_ICON_SIZE / 2;
		const int y = (int)( GetTall() * 0.8f + flY ) - RADAR_ICON_SIZE / 2;
		pIcon->DrawSelf( x, y, RADAR_ICON_SIZE, RADAR_ICON_SIZE, clr );
	}

	CUtlVector<RadarMarine_t> m_Marines;
	CUtlVector<RadarAlarm_t> m_Alarms;
	float m_flRadioTime[MAX_PLAYERS + 1];
	float m_flNextRefresh;

	CHudTexture *m_pIcons[4];	// assault, support, and both highlighted
	CHudTexture *m_pSonicIcon;
};

DECLARE_HUDELEMENT( CHudRadar );
