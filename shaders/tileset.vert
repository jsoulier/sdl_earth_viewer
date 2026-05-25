cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 ModelViewProjection;
};

cbuffer UniformBuffer : register(b1, space1)
{
    float4x4 Model;
};

struct Input
{
    float3 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float2 Overlay0 : TEXCOORD2;
};

struct Output
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float2 Overlay0 : TEXCOORD2;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(ModelViewProjection, float4(input.Position, 1.0f));
    output.Position.y *= -1.0;
    output.WorldPosition = mul(Model, float4(input.Position, 1.0f)).xyz;
    output.TexCoord = input.TexCoord;
    output.Overlay0 = input.Overlay0;
    return output;
}
