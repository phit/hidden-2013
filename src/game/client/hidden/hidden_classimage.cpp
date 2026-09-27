//========= Hidden: Source =====================================================//
//
// Purpose: The team and weapon menus' 3D marine preview: Beta 4b kept
//			Counter-Strike: Source's CSClassImagePanel and UpdateClassImageEntity
//			(client.dll 0x24204da0, 0x241e6440) with its own model, weapon and
//			animations. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_classimage.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui/IPanel.h>
#include "c_baseanimatingoverlay.h"
#include "istudiorender.h"
#include "mathlib/lightdesc.h"
#include "view_shared.h"
#include "ivrenderview.h"
#include "tier0/vprof.h"
#include "model_types.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define CLASSIMAGE_WEAPON_MODEL		"models/weapons/f2000/w_f2000.mdl"
#define CLASSIMAGE_WEAPON_SEQUENCE	"Idle_Upper_Aug"
#define CLASSIMAGE_LOWER_SEQUENCE	"walk_lower"

class CSClassImagePanel;
static CUtlVector<CSClassImagePanel *> s_ClassImagePanels;

// An image panel whose "3DModel" key names the model drawn over it.
class CSClassImagePanel : public ImagePanel
{
	DECLARE_CLASS_SIMPLE( CSClassImagePanel, ImagePanel );

public:
	CSClassImagePanel( Panel *pParent, const char *pszName ) : ImagePanel( pParent, pszName )
	{
		m_szModelName[0] = '\0';
		s_ClassImagePanels.AddToTail( this );
	}

	~CSClassImagePanel()
	{
		s_ClassImagePanels.FindAndRemove( this );
	}

	// Beta 4b also scaled the position and size from 640x480 here, as its menus weren't
	// proportional; ours are, which does the same.
	virtual void ApplySettings( KeyValues *pResourceData )
	{
		const char *pszModel = pResourceData->GetString( "3DModel", NULL );
		if ( pszModel )
			Q_strncpy( m_szModelName, pszModel, sizeof( m_szModelName ) );

		BaseClass::ApplySettings( pResourceData );
	}

	const char *GetModelName( void ) const { return m_szModelName; }

private:
	char m_szModelName[128];
};

DECLARE_BUILD_FACTORY( CSClassImagePanel );

static CHandle<C_BaseAnimatingOverlay> s_hClassImagePlayer;
static CHandle<C_BaseAnimating> s_hClassImageWeapon;

// A panel only shows when it and all its parents do.
static bool WillPanelBeVisible( VPANEL hPanel )
{
	for ( ; hPanel; hPanel = ipanel()->GetParent( hPanel ) )
	{
		if ( !ipanel()->IsVisible( hPanel ) )
			return false;
	}
	return true;
}

static bool ShouldRecreateClassImageEntity( C_BaseAnimating *pEnt, const char *pszNewModelName )
{
	if ( !pszNewModelName || !pszNewModelName[0] )
		return false;

	if ( !pEnt || !pEnt->GetModel() )
		return true;

	const char *pszName = modelinfo->GetModelName( pEnt->GetModel() );
	return !pszName || Q_stricmp( pszName, pszNewModelName ) != 0;
}

static void UpdateClassImageEntity( const char *pszModelName, int x, int y, int width, int height )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	MDLCACHE_CRITICAL_SECTION();

	C_BaseAnimatingOverlay *pPlayerModel = s_hClassImagePlayer.Get();
	const bool bNewPlayer = ShouldRecreateClassImageEntity( pPlayerModel, pszModelName );
	if ( bNewPlayer )
	{
		if ( pPlayerModel )
			pPlayerModel->Remove();

		pPlayerModel = new C_BaseAnimatingOverlay;
		pPlayerModel->InitializeAsClientEntity( pszModelName, RENDER_GROUP_OPAQUE_ENTITY );
		pPlayerModel->AddEffects( EF_NODRAW );	// only drawn here

		pPlayerModel->SetSequence( pPlayerModel->LookupSequence( "Idle_lower" ) );
		pPlayerModel->SetPoseParameter( 0, 0.0f );
		pPlayerModel->SetPoseParameter( 1, 10.0f );	// looking up a little
		pPlayerModel->SetPoseParameter( 2, 0.0f );
		pPlayerModel->SetPoseParameter( 3, 0.0f );
		pPlayerModel->SetPoseParameter( 4, 0.0f );

		s_hClassImagePlayer = pPlayerModel;
	}

	C_BaseAnimating *pWeaponModel = s_hClassImageWeapon.Get();
	if ( bNewPlayer || ShouldRecreateClassImageEntity( pWeaponModel, CLASSIMAGE_WEAPON_MODEL ) )
	{
		if ( pWeaponModel )
			pWeaponModel->Remove();

		pWeaponModel = new C_BaseAnimating;
		pWeaponModel->InitializeAsClientEntity( CLASSIMAGE_WEAPON_MODEL, RENDER_GROUP_OPAQUE_ENTITY );
		pWeaponModel->AddEffects( EF_NODRAW );
		pWeaponModel->FollowEntity( pPlayerModel );	// bone merged into the hands

		s_hClassImageWeapon = pWeaponModel;
	}

	// The model stands somewhere near the local player where there's room: up and back from the
	// eyes, or, if that's blocked, forward and down from where it hit.
	Vector vecOrigin = pLocalPlayer->EyePosition();
	Vector vecLightOrigin = vecOrigin;

	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + Vector( -100, 0, 100 ), MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction == 1.0f )
	{
		vecLightOrigin = tr.endpos;
	}
	else
	{
		vecLightOrigin = tr.endpos + Vector( 1, 0, -1 );
		UTIL_TraceLine( vecLightOrigin, vecLightOrigin + Vector( 100, 0, -100 ), MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
		vecOrigin = tr.endpos;
	}

	// Beta 4b lit it with a white dynamic light at vecLightOrigin (only models, 400 units), brighter
	// where the world was darker. A dynamic light doesn't reach a model drawn over the HUD in SDK
	// 2013, and it would light players standing nearby too, so the model is lit directly: the
	// world's light where it stands, and a white light from there.
	Vector vecAmbient = engine->GetLightForPoint( vecOrigin, true );
	Vector vecAmbientCube[6];
	for ( int i = 0; i < 6; i++ )
		vecAmbientCube[i] = vecAmbient;

	LightDesc_t light;
	light.InitPoint( vecLightOrigin, Vector( 1.0f, 1.0f, 1.0f ) );

	pPlayerModel->SetAbsOrigin( vecOrigin );
	pPlayerModel->SetAbsAngles( QAngle( 0, 210, 0 ) );

	// Walking legs with the FN2000 held up, as two layers.
	const int iLowerSequence = pPlayerModel->LookupSequence( CLASSIMAGE_LOWER_SEQUENCE );
	pPlayerModel->m_SequenceTransitioner.CheckForSequenceChange( pPlayerModel->GetModelPtr(), iLowerSequence, false, true );
	pPlayerModel->m_SequenceTransitioner.UpdateCurrent( pPlayerModel->GetModelPtr(), iLowerSequence, pPlayerModel->GetCycle(),
		pPlayerModel->GetPlaybackRate(), gpGlobals->realtime );

	pPlayerModel->SetNumAnimOverlays( 2 );
	for ( int i = 0; i < pPlayerModel->GetNumAnimOverlays(); i++ )
	{
		C_AnimationLayer *pLayer = pPlayerModel->GetAnimOverlay( i );
		pLayer->m_flCycle = pPlayerModel->GetCycle();
		pLayer->m_nSequence = pPlayerModel->LookupSequence( i ? CLASSIMAGE_WEAPON_SEQUENCE : CLASSIMAGE_LOWER_SEQUENCE );
		pLayer->m_flPlaybackRate = 1.0f;
		pLayer->m_flWeight = 1.0f;
		pLayer->SetOrder( i );
	}

	pPlayerModel->FrameAdvance( gpGlobals->frametime );

	// Seen from 110 units away, level with the middle of the model.
	CViewSetup view;
	view.x = x;
	view.y = y;
	view.width = width;
	view.height = height;
	view.m_bOrtho = false;
	view.fov = 54.0f;
	view.m_flAspectRatio = (float)width / (float)MAX( height, 1 );
	view.origin = vecOrigin + Vector( -110, -5, -5 );

	Vector vecMins, vecMaxs;
	pPlayerModel->C_BaseAnimating::GetRenderBounds( vecMins, vecMaxs );
	view.origin.z += ( vecMins.z + vecMaxs.z ) * 0.55f;
	view.angles.Init();
	view.zNear = 7.0f;
	view.zFar = 1000.0f;

	// Beta 4b's engine set this view up with ViewSetup3D; SDK 2013 pushes and pops it. The depth
	// is cleared, since the scene's has nothing to do with this view.
	Frustum frustum;
	render->Push3DView( view, VIEW_CLEAR_DEPTH, NULL, frustum );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetLightingOrigin( vecOrigin );
	g_pStudioRender->SetAmbientLightColors( vecAmbientCube );
	g_pStudioRender->SetLocalLights( 1, &light );
	modelrender->SuppressEngineLighting( true );

	const float flColor[3] = { 1.0f, 1.0f, 1.0f };
	render->SetColorModulation( flColor );
	render->SetBlend( 1.0f );

	pPlayerModel->DrawModel( STUDIO_RENDER );
	if ( pWeaponModel )
		pWeaponModel->DrawModel( STUDIO_RENDER );

	modelrender->SuppressEngineLighting( false );
	render->PopView( frustum );
}

// Beta 4b's ClientModeSDKNormal::PostRenderVGui: the first visible panel gets the model.
//
// It didn't land on the panel. The CS:S code turned the panel's position in its parent into screen
// space with the panel's own LocalToScreen, which adds that position twice, and Beta 4b's engine
// counted the 3D view's y from the bottom of the screen. For the menus' panel (169,106 320x223 in
// 640x480) that put the model at 341,50 318x213: standing in the photo frame on the right-hand page,
// as screenshots of Beta 4b show. This puts it there on purpose, relative to the menu.
void HiddenClassImage_PostRenderVGui( void )
{
	VPROF( "HiddenClassImage_PostRenderVGui" );

	for ( int i = 0; i < s_ClassImagePanels.Count(); i++ )
	{
		CSClassImagePanel *pPanel = s_ClassImagePanels[i];
		if ( !WillPanelBeVisible( pPanel->GetVPanel() ) || !pPanel->GetParent() )
			continue;

		int x, y, w, h;
		pPanel->GetBounds( x, y, w, h );

		int nMenuX = 0, nMenuY = 0, nMenuWide, nMenuTall;
		pPanel->GetParent()->LocalToScreen( nMenuX, nMenuY );
		pPanel->GetParent()->GetSize( nMenuWide, nMenuTall );

		// Inside the panel's border, as the CS:S code had it: 3 and 5 pixels in, 2 and 10 smaller.
		const int nViewWide = w - 2;
		const int nViewTall = h - 10;
		const int nViewX = nMenuX + 2 * x + 3;
		const int nViewY = nMenuY + nMenuTall - ( 2 * y + 5 ) - nViewTall;
		UpdateClassImageEntity( pPanel->GetModelName(), nViewX, nViewY, nViewWide, nViewTall );
		return;
	}
}
