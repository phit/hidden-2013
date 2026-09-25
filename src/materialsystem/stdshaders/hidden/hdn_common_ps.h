//========= Hidden: Source =====================================================//
//
// Purpose: Shared by Beta 4b's screen effect pixel shaders (docs/spec/client.md).
//
//=============================================================================//

#include "common_ps_fxc.h"

struct PS_INPUT
{
	float2 uv					: TEXCOORD0;
};

float HiddenLuminance( float3 rgb )
{
	return dot( rgb, float3( 0.222f, 0.707f, 0.071f ) );
}

// The average of the 8 neighbours at +-offset: a 3x3 box without the centre.
float4 HiddenBoxBlur( sampler frame, float2 uv, float2 offset )
{
	float4 sum = tex2D( frame, uv + float2( 0.0f, offset.y ) );
	sum += tex2D( frame, uv + float2( -offset.x, offset.y ) );
	sum += tex2D( frame, uv + float2( offset.x, offset.y ) );
	sum += tex2D( frame, uv + float2( 0.0f, -offset.y ) );
	sum += tex2D( frame, uv + float2( -offset.x, -offset.y ) );
	sum += tex2D( frame, uv + float2( offset.x, -offset.y ) );
	sum += tex2D( frame, uv + float2( -offset.x, 0.0f ) );
	sum += tex2D( frame, uv + float2( offset.x, 0.0f ) );
	return sum * 0.125f;
}
