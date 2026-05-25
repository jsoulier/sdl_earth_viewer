struct Output
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float2 Overlay0 : TEXCOORD2;
};

Texture2D ColorTexture : register(t0, space2);
SamplerState ColorSampler : register(s0, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    float2 Translation;
    float2 Scale;
};

cbuffer UniformBuffer : register(b1, space3)
{
    int TileType;
};

cbuffer UniformBuffer : register(b2, space3)
{
    float4 SunDirection;
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
    float4 color = ColorTexture.Sample(ColorSampler, texcoord);
    float3 normal = normalize(input.WorldPosition);
    float ambient = 0.02;
    float lighting = max(0.0, dot(normal, SunDirection.xyz)) + ambient;
    return float4(color.rgb * lighting, color.a);
}
