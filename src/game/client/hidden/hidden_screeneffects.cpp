//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's full-screen overlays, drawn after the scene in its order:
//			scope, night vision, stun blur and its trail, the death and spectator
//			cameras, and the Hidden's view. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "ScreenSpaceEffects.h"
#include "view_scene.h"
#include "materialsystem/imaterialvar.h"
#include "c_hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_hvision( "cl_hvision", "1", FCVAR_ARCHIVE, "toggles the hiddens view shader." );

#define HIDDEN_BLUR_TRAIL_INTERVAL	0.05f	// how often the stun trail takes a frame

class CHiddenScreenEffects : public IScreenSpaceEffect
{
public:
	CHiddenScreenEffects() : m_flNextTrailFrame( 0.0f ) {}

	virtual void Init( void ) {}
	virtual void Shutdown( void ) {}
	virtual void SetParameters( KeyValues *params ) {}
	virtual void Enable( bool bEnable ) {}
	virtual bool IsEnabled( void ) { return true; }

	virtual void Render( int x, int y, int w, int h );

private:
	void DrawOverlay( const char *pszMaterial, int x, int y, int w, int h );
	void DrawBlurTrail( float flAlpha );

	float m_flNextTrailFrame;
};

ADD_SCREENSPACE_EFFECT( CHiddenScreenEffects, hidden_screen_effects );

void CHiddenScreenEffects::Render( int x, int y, int w, int h )
{
	C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->IsAlive() )
	{
		if ( pPlayer->GetZoom() )
			DrawOverlay( "models/weapons/v_fn2000/scopemask", x, y, w, h );

		if ( pPlayer->NightVisionEnabled() )
			DrawOverlay( "vgui/hud/hdn_nightvision", x, y, w, h );

		if ( pPlayer->IsStunned() && pPlayer->GetBlur() > 0.0f )
		{
			IMaterial *pBlur = materials->FindMaterial( "vgui/hud/blur", TEXTURE_GROUP_CLIENT_EFFECTS, true );
			bool bFound = false;
			IMaterialVar *pOffset = pBlur->FindVar( "$bluroffset", &bFound, false );
			if ( bFound )
				pOffset->SetFloatValue( pPlayer->GetBlur() );

			DrawOverlay( "vgui/hud/blur", x, y, w, h );
			DrawBlurTrail( 0.7f / pPlayer->GetBlur() );
		}
	}
	else if ( pPlayer->GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		DrawOverlay( "vgui/hud/svision", x, y, w, h );
	}
	else if ( pPlayer->GetObserverMode() == OBS_MODE_FIXED )
	{
		DrawOverlay( "vgui/hud/helmetcam", x, y, w, h );
	}

	// The Hidden's aura (HDN_Invert while standing still) comes with the aura itself; without it
	// the Hidden always sees through vgui/hud/hvision.
	if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_HIDDEN && cl_hvision.GetBool() )
		DrawOverlay( "vgui/hud/hvision", x, y, w, h );
}

void CHiddenScreenEffects::DrawOverlay( const char *pszMaterial, int x, int y, int w, int h )
{
	IMaterial *pMaterial = materials->FindMaterial( pszMaterial, TEXTURE_GROUP_CLIENT_EFFECTS, true );
	if ( !pMaterial || pMaterial->IsErrorMaterial() )
		return;

	if ( pMaterial->NeedsPowerOfTwoFrameBufferTexture() )
	{
		// Refract materials read the power-of-two copy, as the underwater overlay does.
		UpdateRefractTexture( x, y, w, h, true );

		CMatRenderContextPtr pRenderContext( materials );
		ITexture *pTexture = GetPowerOfTwoFrameBufferTexture();
		const int sw = pTexture->GetActualWidth();
		const int sh = pTexture->GetActualHeight();
		pRenderContext->DrawScreenSpaceRectangle( pMaterial, x, y, w, h, 0, 0, sw - 1, sh - 1, sw, sh );
		return;
	}

	DrawScreenEffectMaterial( pMaterial, x, y, w, h );
}

// The stun's motion trail: every 0.05 s the current frame is blended into an accumulation target
// at flAlpha, and that target covers the screen every frame, so a strong blur leaves long trails.
void CHiddenScreenEffects::DrawBlurTrail( float flAlpha )
{
	IMaterial *pFrontBuffer = materials->FindMaterial( "frontbuffer", TEXTURE_GROUP_OTHER, true );
	if ( !pFrontBuffer || pFrontBuffer->IsErrorMaterial() )
		return;

	bool bFound = false;
	IMaterialVar *pAlpha = pFrontBuffer->FindVar( "$alpha", &bFound, false );
	IMaterialVar *pBaseTexture = pFrontBuffer->FindVar( "$basetexture", NULL, false );
	if ( !bFound || !pBaseTexture )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *pAccum = GetFullFrameFrameBufferTexture( 1 );
	ITexture *pFrame = GetFullFrameFrameBufferTexture( 0 );

	// A clock jump (a new map, a demo seek) starts the trail over.
	if ( fabs( gpGlobals->curtime - m_flNextTrailFrame ) > 0.5f )
		m_flNextTrailFrame = 0.0f;

	DevMsg( 2, "bluralpha : %f\n", flAlpha );

	if ( gpGlobals->curtime >= m_flNextTrailFrame )
	{
		pRenderContext->CopyRenderTargetToTexture( pFrame );
		pAlpha->SetFloatValue( m_flNextTrailFrame == 0.0f ? 1.0f : flAlpha );
		pBaseTexture->SetTextureValue( pFrame );

		pRenderContext->PushRenderTargetAndViewport( pAccum );
		pRenderContext->DrawScreenSpaceQuad( pFrontBuffer );
		pRenderContext->PopRenderTargetAndViewport();

		m_flNextTrailFrame = gpGlobals->curtime + HIDDEN_BLUR_TRAIL_INTERVAL;
	}

	pAlpha->SetFloatValue( 1.0f );
	pBaseTexture->SetTextureValue( pAccum );
	pRenderContext->DrawScreenSpaceQuad( pFrontBuffer );
	pBaseTexture->SetTextureValue( pFrame );
}
