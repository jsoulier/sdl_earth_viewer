cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 View;
};

cbuffer UniformBuffer : register(b1, space1)
{
    float4x4 Proj;
};

cbuffer UniformBuffer : register(b2, space1)
{
    float4x4 Model;
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
    float4 worldPosition = mul(Model, float4(input.Position, 1.0f));
    output.Position = mul(Proj, mul(View, worldPosition));
    output.TexCoord = input.TexCoord;
    return output;
}
