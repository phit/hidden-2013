//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's screen effect shaders, under the names its materials use
//			(vgui/hud/blur, hdn_nightvision, svision, helmetcam, hdn_invert and
//			the FN2000 scope mask). Each draws the frame buffer copy
//			(_rt_FullFrameFB) over the screen. See docs/spec/client.md.
//
//=============================================================================//

#include "BaseVSShader.h"

#include "hdn_screenspace_vs20.inc"
#include "hdn_blur_ps20b.inc"
#include "hdn_scope_ps20b.inc"
#include "hdn_nightvision_ps20b.inc"
#include "hdn_desaturate_ps20b.inc"
#include "hdn_helmetcam_ps20b.inc"
#include "hdn_invert_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Shared shadow state: a full-screen quad with no depth.
static void HiddenScreenShadowState( IShaderShadow *pShaderShadow, int nSamplers )
{
	pShaderShadow->EnableDepthWrites( false );
	pShaderShadow->EnableDepthTest( false );
	for ( int i = 0; i < nSamplers; i++ )
		pShaderShadow->EnableTexture( (Sampler_t)( SHADER_SAMPLER0 + i ), true );
	pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );
}

// The blur offset in UV units: Beta 4b's vertex shader constant c38.
static void SetBlurOffset( IShaderDynamicAPI *pShaderAPI, float flOffset, int iRegister )
{
	int nWidth, nHeight;
	pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );

	float vOffset[4] = { flOffset / MAX( nWidth, 1 ), flOffset / MAX( nHeight, 1 ), 0.0f, 0.0f };
	pShaderAPI->SetPixelShaderConstant( iRegister, vOffset );
}

// The translation of a TextureScroll matrix.
static void SetUVOffset( IShaderDynamicAPI *pShaderAPI, IMaterialVar *pVar, int iRegister )
{
	float vOffset[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ( pVar->IsDefined() && pVar->GetType() == MATERIAL_VAR_TYPE_MATRIX )
	{
		const VMatrix &mat = pVar->GetMatrixValue();
		vOffset[0] = mat[0][3];
		vOffset[1] = mat[1][3];
	}
	pShaderAPI->SetPixelShaderConstant( iRegister, vOffset );
}

static void SetFloat( IShaderDynamicAPI *pShaderAPI, float flValue, int iRegister )
{
	float vValue[4] = { flValue, 0.0f, 0.0f, 0.0f };
	pShaderAPI->SetPixelShaderConstant( iRegister, vValue );
}

#define HIDDEN_SCREEN_FALLBACK \
	SHADER_FALLBACK \
	{ \
		if ( g_pHardwareConfig->GetDXSupportLevel() < 90 ) \
			return "Wireframe"; \
		return 0; \
	}

//-----------------------------------------------------------------------------
BEGIN_VS_SHADER_FLAGS( HDN_PostBlur, "Hidden: stun blur", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BLUROFFSET, SHADER_PARAM_TYPE_FLOAT, "2.5", "blur distance in pixels" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_INIT
	{
	}

	HIDDEN_SCREEN_FALLBACK

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			HiddenScreenShadowState( pShaderShadow, 1 );
			DECLARE_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_STATIC_PIXEL_SHADER( hdn_blur_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_blur_ps20b );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			SetBlurOffset( pShaderAPI, params[BLUROFFSET]->GetFloatValue(), 0 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_blur_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( hdn_blur_ps20b );
		}
		Draw();
	}
END_SHADER

//-----------------------------------------------------------------------------
BEGIN_VS_SHADER_FLAGS( HDN_Scope, "Hidden: FN2000 scope", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SCOPEMASK, SHADER_PARAM_TYPE_TEXTURE, "", "scope mask" )
		SHADER_PARAM( BLUROFFSET, SHADER_PARAM_TYPE_FLOAT, "6", "blur distance in pixels" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_INIT
	{
		if ( params[SCOPEMASK]->IsDefined() )
			LoadTexture( SCOPEMASK );
	}

	HIDDEN_SCREEN_FALLBACK

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			HiddenScreenShadowState( pShaderShadow, 2 );
			DECLARE_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_STATIC_PIXEL_SHADER( hdn_scope_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_scope_ps20b );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			BindTexture( SHADER_SAMPLER1, SCOPEMASK, -1 );
			SetBlurOffset( pShaderAPI, params[BLUROFFSET]->GetFloatValue(), 0 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_scope_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( hdn_scope_ps20b );
		}
		Draw();
	}
END_SHADER

//-----------------------------------------------------------------------------
BEGIN_VS_SHADER_FLAGS( HDN_Nightvision, "Hidden: night vision", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( NOISETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "noise" )
		SHADER_PARAM( UVOFFSET, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "noise scroll (TextureScroll)" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_INIT
	{
		if ( params[NOISETEXTURE]->IsDefined() )
			LoadTexture( NOISETEXTURE );
	}

	HIDDEN_SCREEN_FALLBACK

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			HiddenScreenShadowState( pShaderShadow, 2 );
			DECLARE_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_STATIC_PIXEL_SHADER( hdn_nightvision_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_nightvision_ps20b );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			BindTexture( SHADER_SAMPLER1, NOISETEXTURE, -1 );
			SetUVOffset( pShaderAPI, params[UVOFFSET], 0 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_nightvision_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( hdn_nightvision_ps20b );
		}
		Draw();
	}
END_SHADER

//-----------------------------------------------------------------------------
BEGIN_VS_SHADER_FLAGS( HDN_Desaturate, "Hidden: death cam", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( INTERLACETEX, SHADER_PARAM_TYPE_TEXTURE, "", "interlace lines" )
		SHADER_PARAM( SATURATION, SHADER_PARAM_TYPE_FLOAT, "0.7", "0 keeps the colour, 1 is grey" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_INIT
	{
		if ( params[INTERLACETEX]->IsDefined() )
			LoadTexture( INTERLACETEX );
	}

	HIDDEN_SCREEN_FALLBACK

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			HiddenScreenShadowState( pShaderShadow, 2 );
			DECLARE_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_STATIC_PIXEL_SHADER( hdn_desaturate_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_desaturate_ps20b );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			BindTexture( SHADER_SAMPLER1, INTERLACETEX, -1 );
			SetFloat( pShaderAPI, params[SATURATION]->GetFloatValue(), 0 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_desaturate_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( hdn_desaturate_ps20b );
		}
		Draw();
	}
END_SHADER

//-----------------------------------------------------------------------------
BEGIN_VS_SHADER_FLAGS( HDN_HelmetCam, "Hidden: spectator cameras", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( INTERLACETEX, SHADER_PARAM_TYPE_TEXTURE, "", "interlace lines" )
		SHADER_PARAM( NOISETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "noise" )
		SHADER_PARAM( SATURATION, SHADER_PARAM_TYPE_FLOAT, "0.7", "0 keeps the colour, 1 is grey" )
		SHADER_PARAM( UVOFFSET, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "noise scroll (TextureScroll)" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_INIT
	{
		if ( params[INTERLACETEX]->IsDefined() )
			LoadTexture( INTERLACETEX );
		if ( params[NOISETEXTURE]->IsDefined() )
			LoadTexture( NOISETEXTURE );
	}

	HIDDEN_SCREEN_FALLBACK

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			HiddenScreenShadowState( pShaderShadow, 3 );
			DECLARE_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_STATIC_PIXEL_SHADER( hdn_helmetcam_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_helmetcam_ps20b );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			BindTexture( SHADER_SAMPLER1, INTERLACETEX, -1 );
			BindTexture( SHADER_SAMPLER2, NOISETEXTURE, -1 );
			SetFloat( pShaderAPI, params[SATURATION]->GetFloatValue(), 0 );
			SetUVOffset( pShaderAPI, params[UVOFFSET], 1 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_helmetcam_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( hdn_helmetcam_ps20b );
		}
		Draw();
	}
END_SHADER

//-----------------------------------------------------------------------------
BEGIN_VS_SHADER_FLAGS( HDN_Invert, "Hidden: aura view", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_INIT
	{
	}

	HIDDEN_SCREEN_FALLBACK

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			HiddenScreenShadowState( pShaderShadow, 1 );
			DECLARE_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_STATIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_STATIC_PIXEL_SHADER( hdn_invert_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_invert_ps20b );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			SetFloat( pShaderAPI, params[ALPHA]->GetFloatValue(), 0 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdn_screenspace_vs20 );
			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_invert_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( hdn_invert_ps20b );
		}
		Draw();
	}
END_SHADER
