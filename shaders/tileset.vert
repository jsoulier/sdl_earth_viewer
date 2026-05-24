cbuffer SceneBuffer : register(b0, space1)
{
    float4x4 View;
    float4x4 Proj;
};

cbuffer ModelBuffer : register(b1, space1)
{
    float4x4 Model;
};

struct Input
{
    float3 Position : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

struct Output
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

Output main(Input input)
{
    Output output;
    float4 worldPos = mul(Model, float4(input.Position, 1.0f));
    float4 viewPos = mul(View, worldPos);
    output.Position = mul(Proj, viewPos);
    output.UV = input.UV;
    return output;
}
