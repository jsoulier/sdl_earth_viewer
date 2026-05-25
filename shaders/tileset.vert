cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 ModelViewProjection;
};

struct Input
{
    float3 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
};

struct Output
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(ModelViewProjection, float4(input.Position, 1.0f));
    output.Position.y *= -1.0;
    output.TexCoord = input.TexCoord;
    return output;
}
