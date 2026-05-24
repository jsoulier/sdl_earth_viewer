struct Output
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Texture2D OverlayTexture : register(t0, space2);
SamplerState OverlaySampler : register(s0, space2);

cbuffer OverlayBuffer : register(b0, space3)
{
    float2 Translation;
    float2 Scale;
};

float4 main(Output input) : SV_Target
{
    float2 overlayUV = input.TexCoord * Scale + Translation;
    float2 texcoord = float2(overlayUV.x, 1.0 - overlayUV.y);
    return OverlayTexture.Sample(OverlaySampler, texcoord);
}
