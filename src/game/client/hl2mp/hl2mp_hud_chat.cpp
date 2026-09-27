//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hl2mp_hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "c_team.h"
#include "c_playerresource.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "ihudlcd.h"
#ifdef HIDDEN
#include <vgui_controls/ScrollBar.h>
#endif



DECLARE_HUDELEMENT( CHudChat );

DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );


//=====================
//CHudChatLine
//=====================

void CHudChatLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//=====================
//CHudChatInputLine
//=====================

void CHudChatInputLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

#ifdef HIDDEN
	// Beta 4b's chat had no boxes: only the text shows.
	GetInputPanel()->SetPaintBackgroundEnabled( false );
	GetInputPanel()->SetPaintBorderEnabled( false );
	GetPrompt()->SetPaintBackgroundEnabled( false );
#endif
}

//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	
}

void CHudChat::CreateChatInputLine( void )
{
	m_pChatInput = new CHudChatInputLine( this, "ChatInputLine" );
	m_pChatInput->SetVisible( false );
}

void CHudChat::CreateChatLines( void )
{
	m_ChatLine = new CHudChatLine( this, "ChatLine1" );
	m_ChatLine->SetVisible( false );	
}

void CHudChat::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

#ifdef HIDDEN
	// Beta 4b's CHudChat cleared its colours: no box behind the chat, the history or the input.
	SetPaintBorderEnabled( false );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetFgColor( Color( 0, 0, 0, 0 ) );
	if ( GetChatHistory() )
	{
		GetChatHistory()->SetPaintBorderEnabled( false );
		GetChatHistory()->SetBgColor( Color( 0, 0, 0, 0 ) );

		// The history's scrollbar (Beta 4b's chat had none) takes Beta 4b's ClientScheme
		// scrollbars instead of ChatScheme's grey track: orange line-art arrows and slider.
		for ( int i = 0; i < GetChatHistory()->GetChildCount(); i++ )
		{
			vgui::ScrollBar *pScrollBar = dynamic_cast< vgui::ScrollBar * >( GetChatHistory()->GetChild( i ) );
			if ( pScrollBar && pScrollBar->GetScheme() != vgui::scheme()->GetScheme( "ClientScheme" ) )
			{
				pScrollBar->SetScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );
				pScrollBar->InvalidateLayout( false, true );
			}
		}
	}
#endif
}

#ifdef HIDDEN
// FadeChatHistory sets the box colours every tick, and Beta 4b had no chat filters.
void CHudChat::OnTick( void )
{
	BaseClass::OnTick();

	SetBgColor( Color( 0, 0, 0, 0 ) );
	if ( GetChatHistory() )
		GetChatHistory()->SetBgColor( Color( 0, 0, 0, 0 ) );
	if ( m_pFiltersButton )
		m_pFiltersButton->SetVisible( false );
}
#endif


void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
}

int CHudChat::GetChatInputOffset( void )
{
	if ( m_pChatInput->IsVisible() )
	{
		return m_iFontHeight;
	}
	else
		return 0;
}

Color CHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorYellow;
	}
	else if( g_PR )
	{
		switch ( g_PR->GetTeam( clientIndex ) )
		{
		case TEAM_COMBINE	: return g_ColorBlue;
		case TEAM_REBELS	: return g_ColorRed;
		default	: return g_ColorYellow;
		}
	}

	return g_ColorYellow;
}