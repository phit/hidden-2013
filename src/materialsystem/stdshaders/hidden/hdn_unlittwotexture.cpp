//========= Hidden: Source =====================================================//
//
// Purpose: Replaces the stock UnlitTwoTexture, whose SDK 2013 pixel shader writes
//			alpha 1: $translucent materials on it, like Beta 4b's HUD frames
//			(vgui/hud/hdn_iris*, hdn_hdn*: frame times scrolling hdn_cam_noise),
//			drew opaque instead of mostly see-through. A shader in a mod's shader
//			DLL takes the place of the stock one with the same name.
//			This is SDK 2013's unlittwotexture_dx9.cpp without the cloak pass and
//			the ps20 path, on the stock vertex shader.
//
//=============================================================================//

#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"

#include "unlittwotexture_vs20.inc"
#include "hdn_unlittwotexture_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( UnlitTwoTexture, "UnlitTwoTexture with the textures' alpha kept" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( TEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "second texture" )
		SHADER_PARAM( FRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $texture2" )
		SHADER_PARAM( TEXTURE2TRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$texture2 texcoord transform" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() )
			LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		if ( params[TEXTURE2]->IsDefined() )
			LoadTexture( TEXTURE2, TEXTUREFLAGS_SRGB );
	}

	SHADER_DRAW
	{
		// Unlit: nothing to add in a flashlight pass.
		if ( pShaderShadow == NULL && pShaderAPI != NULL && pShaderAPI->InFlashlightMode() )
		{
			Draw( false );
			return;
		}

		BlendType_t nBlendType = EvaluateBlendRequirements( BASETEXTURE, true );
		bool bFullyOpaque = ( nBlendType != BT_BLENDADD ) && ( nBlendType != BT_BLEND ) && !IS_FLAG_SET( MATERIAL_VAR_ALPHATEST );

		SHADOW_STATE
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );
			pShaderShadow->EnableSRGBWrite( true );

			bool bTranslucent = IsAlphaModulating() || TextureIsTranslucent( BASETEXTURE, true ) ||
				TextureIsTranslucent( TEXTURE2, true );

			if ( bTranslucent )
			{
				if ( IS_FLAG_SET( MATERIAL_VAR_ADDITIVE ) )
					EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
				else
					EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				if ( IS_FLAG_SET( MATERIAL_VAR_ADDITIVE ) )
					EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
				else
					DisableAlphaBlending();
			}

			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			if ( IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR ) )
				flags |= VERTEX_COLOR;
			pShaderShadow->VertexShaderVertexFormat( flags, 1, NULL, 0 );

			DECLARE_STATIC_VERTEX_SHADER( unlittwotexture_vs20 );
			SET_STATIC_VERTEX_SHADER( unlittwotexture_vs20 );

			DECLARE_STATIC_PIXEL_SHADER( hdn_unlittwotexture_ps20b );
			SET_STATIC_PIXEL_SHADER( hdn_unlittwotexture_ps20b );

			DefaultFog();

			pShaderShadow->EnableAlphaWrites( bFullyOpaque );
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, BASETEXTURE, FRAME );
			BindTexture( SHADER_SAMPLER1, TEXTURE2, FRAME2 );
			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );
			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, TEXTURE2TRANSFORM );
			SetModulationPixelShaderDynamicState_LinearColorSpace( 1 );

			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
			int fogIndex = ( fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ? 1 : 0;

			DECLARE_DYNAMIC_VERTEX_SHADER( unlittwotexture_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( DOWATERFOG, fogIndex );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( unlittwotexture_vs20 );

			DECLARE_DYNAMIC_PIXEL_SHADER( hdn_unlittwotexture_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo() );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
			SET_DYNAMIC_PIXEL_SHADER( hdn_unlittwotexture_ps20b );
		}
		Draw();
	}
END_SHADER
