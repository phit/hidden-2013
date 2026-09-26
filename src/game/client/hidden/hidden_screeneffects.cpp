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
#include "tier1/callqueue.h"
#include "c_hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_hvision( "cl_hvision", "1", FCVAR_ARCHIVE, "toggles the hiddens view shader." );

#define HIDDEN_BLUR_TRAIL_INTERVAL	0.05f	// how often the stun trail takes a frame

enum HiddenOverlay_t
{
	OVERLAY_SCOPE,
	OVERLAY_NIGHTVISION,
	OVERLAY_BLUR,
	OVERLAY_DEATHCAM,
	OVERLAY_HELMETCAM,
	OVERLAY_HVISION,
	OVERLAY_INVERT,			// the Hidden's aura
	OVERLAY_FRONTBUFFER,	// the stun trail's copy material

	OVERLAY_COUNT
};

static const char *s_pszOverlayMaterials[OVERLAY_COUNT] =
{
	"models/weapons/v_fn2000/scopemask",
	"vgui/hud/hdn_nightvision",
	"vgui/hud/blur",
	"vgui/hud/svision",
	"vgui/hud/helmetcam",
	"vgui/hud/hvision",
	"vgui/hud/hdn_invert",
	"frontbuffer",
};

class CHiddenScreenEffects : public IScreenSpaceEffect
{
public:
	CHiddenScreenEffects() : m_flNextTrailFrame( 0.0f ) {}

	virtual void Init( void ) {}
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params ) {}
	virtual void Enable( bool bEnable ) {}
	virtual bool IsEnabled( void ) { return true; }

	virtual void Render( int x, int y, int w, int h );

private:
	IMaterial *GetMaterial( HiddenOverlay_t nOverlay );
	void DrawOverlay( HiddenOverlay_t nOverlay, int x, int y, int w, int h );
	void DrawBlurTrail( float flAlpha, int x, int y, int w, int h );
	static void SetBlurOffset( IMaterial *pBlur, float flOffset );
	static void RenderBlurTrail( IMaterial *pFrontBuffer, ITexture *pFrame, ITexture *pAccum, float flCaptureAlpha, int x, int y, int w, int h );

	CMaterialReference m_Materials[OVERLAY_COUNT];	// held, so they stay loaded between frames
	float m_flNextTrailFrame;
};

ADD_SCREENSPACE_EFFECT( CHiddenScreenEffects, hidden_screen_effects );

void CHiddenScreenEffects::Shutdown( void )
{
	for ( int i = 0; i < OVERLAY_COUNT; i++ )
		m_Materials[i].Shutdown();
}

IMaterial *CHiddenScreenEffects::GetMaterial( HiddenOverlay_t nOverlay )
{
	if ( !m_Materials[nOverlay].IsValid() )
		m_Materials[nOverlay].Init( s_pszOverlayMaterials[nOverlay], TEXTURE_GROUP_CLIENT_EFFECTS );

	IMaterial *pMaterial = m_Materials[nOverlay];
	return ( pMaterial && !pMaterial->IsErrorMaterial() ) ? pMaterial : NULL;
}

void CHiddenScreenEffects::Render( int x, int y, int w, int h )
{
	C_Hidden_Player *pPlayer = C_Hidden_Player::GetLocalHiddenPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->IsAlive() )
	{
		if ( pPlayer->GetZoom() )
			DrawOverlay( OVERLAY_SCOPE, x, y, w, h );

		if ( pPlayer->NightVisionEnabled() )
			DrawOverlay( OVERLAY_NIGHTVISION, x, y, w, h );

		if ( pPlayer->IsStunned() && pPlayer->GetBlur() > 0.0f )
		{
			IMaterial *pBlur = GetMaterial( OVERLAY_BLUR );
			if ( pBlur )
			{
				CMatRenderContextPtr pRenderContext( materials );
				ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
				if ( pCallQueue )
					pCallQueue->QueueCall( SetBlurOffset, pBlur, pPlayer->GetBlur() );
				else
					SetBlurOffset( pBlur, pPlayer->GetBlur() );
			}

			DrawOverlay( OVERLAY_BLUR, x, y, w, h );
			DrawBlurTrail( 0.7f / pPlayer->GetBlur(), x, y, w, h );
		}
	}
	// Beta 4b's observer mode 1 (the value of SDK's deathcam) watched the map's cameras, which is
	// OBS_MODE_FIXED here; its mode 2 watched marines through their helmet cams (OBS_MODE_IN_EYE).
	else if ( pPlayer->GetObserverMode() == OBS_MODE_FIXED || pPlayer->GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		DrawOverlay( OVERLAY_DEATHCAM, x, y, w, h );
	}
	else if ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		DrawOverlay( OVERLAY_HELMETCAM, x, y, w, h );
	}

	// The Hidden sees the world inverted while the aura works (standing still), which turns the
	// marines' aura trails green, orange or red; otherwise through vgui/hud/hvision.
	if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_HIDDEN )
	{
		if ( pPlayer->IsAuraActive() )
			DrawOverlay( OVERLAY_INVERT, x, y, w, h );
		else if ( cl_hvision.GetBool() )
			DrawOverlay( OVERLAY_HVISION, x, y, w, h );
	}
}

void CHiddenScreenEffects::DrawOverlay( HiddenOverlay_t nOverlay, int x, int y, int w, int h )
{
	IMaterial *pMaterial = GetMaterial( nOverlay );
	if ( !pMaterial )
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
// The timing is decided here; the drawing changes the shared frontbuffer material's variables
// between draws, so like the engine's post-processing it runs on the render thread when queued.
void CHiddenScreenEffects::DrawBlurTrail( float flAlpha, int x, int y, int w, int h )
{
	IMaterial *pFrontBuffer = GetMaterial( OVERLAY_FRONTBUFFER );
	if ( !pFrontBuffer )
		return;

	// A clock jump (a new map, a demo seek) starts the trail over.
	if ( fabs( gpGlobals->curtime - m_flNextTrailFrame ) > 0.5f )
		m_flNextTrailFrame = 0.0f;

	DevMsg( 2, "bluralpha : %f\n", flAlpha );

	float flCaptureAlpha = 0.0f;	// no new frame for the trail this time
	if ( gpGlobals->curtime >= m_flNextTrailFrame )
	{
		flCaptureAlpha = ( m_flNextTrailFrame == 0.0f ) ? 1.0f : flAlpha;
		m_flNextTrailFrame = gpGlobals->curtime + HIDDEN_BLUR_TRAIL_INTERVAL;
	}

	ITexture *pFrame = GetFullFrameFrameBufferTexture( 0 );
	ITexture *pAccum = GetFullFrameFrameBufferTexture( 1 );

	CMatRenderContextPtr pRenderContext( materials );
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	if ( pCallQueue )
		pCallQueue->QueueCall( RenderBlurTrail, pFrontBuffer, pFrame, pAccum, flCaptureAlpha, x, y, w, h );
	else
		RenderBlurTrail( pFrontBuffer, pFrame, pAccum, flCaptureAlpha, x, y, w, h );
}

void CHiddenScreenEffects::SetBlurOffset( IMaterial *pBlur, float flOffset )
{
	bool bFound = false;
	IMaterialVar *pOffset = pBlur->FindVar( "$bluroffset", &bFound, false );
	if ( bFound )
		pOffset->SetFloatValue( flOffset );
}

void CHiddenScreenEffects::RenderBlurTrail( IMaterial *pFrontBuffer, ITexture *pFrame, ITexture *pAccum, float flCaptureAlpha, int x, int y, int w, int h )
{
	bool bFound = false;
	IMaterialVar *pAlpha = pFrontBuffer->FindVar( "$alpha", &bFound, false );
	IMaterialVar *pBaseTexture = pFrontBuffer->FindVar( "$basetexture", NULL, false );
	if ( !bFound || !pBaseTexture )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	const int nFrameW = pFrame->GetActualWidth();
	const int nFrameH = pFrame->GetActualHeight();
	const int nAccumW = pAccum->GetActualWidth();
	const int nAccumH = pAccum->GetActualHeight();

	// Explicit sizes rather than DrawScreenSpaceQuad, which reads the current render target when the
	// queue runs; flushed while jpeg switched the queued material system off, that crashed.
	if ( flCaptureAlpha > 0.0f )
	{
		pRenderContext->CopyRenderTargetToTexture( pFrame );
		pAlpha->SetFloatValue( flCaptureAlpha );
		pBaseTexture->SetTextureValue( pFrame );

		pRenderContext->PushRenderTargetAndViewport( pAccum );
		pRenderContext->DrawScreenSpaceRectangle( pFrontBuffer, 0, 0, nAccumW, nAccumH, 0, 0, nFrameW - 1, nFrameH - 1, nFrameW, nFrameH );
		pRenderContext->PopRenderTargetAndViewport();
	}

	pAlpha->SetFloatValue( 1.0f );
	pBaseTexture->SetTextureValue( pAccum );
	pRenderContext->DrawScreenSpaceRectangle( pFrontBuffer, x, y, w, h, 0, 0, nAccumW - 1, nAccumH - 1, nAccumW, nAccumH );
	pBaseTexture->SetTextureValue( pFrame );
}
