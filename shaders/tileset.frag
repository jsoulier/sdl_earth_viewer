struct Output
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
    float2 Overlay0 : TEXCOORD1;
};

Texture2D ColorTexture : register(t0, space2);
SamplerState ColorSampler : register(s0, space2);

cbuffer OverlayBuffer : register(b0, space3)
{
    float2 Translation;
    float2 Scale;
};

cbuffer TextureSelectBuffer : register(b1, space3)
{
    int TileType;
};

float4 main(Output input) : SV_Target
{
    float2 texcoord;
    if (TileType == 0)
    {
        texcoord = input.TexCoord;
    }
    else if (TileType == 1)
    {
        texcoord = input.Overlay0 * Scale + Translation;
        texcoord.y = 1.0 - texcoord.y;
    }
    else
    {
        texcoord = float2(0.0f, 0.0f);
    }
    return ColorTexture.Sample(ColorSampler, texcoord);
}
