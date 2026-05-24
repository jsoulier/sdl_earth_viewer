struct Output
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Texture2D OverlayTexture : register(t0, space2);
SamplerState OverlaySampler : register(s0, space2);

float4 main(Output input) : SV_Target
{
    return OverlayTexture.Sample(OverlaySampler, input.TexCoord);
}
