cbuffer UniformBuffer : register(b0, space1)
{
    float4 Positions[6];
};

struct Output
{
    float4 Position : SV_Position;
    float4 Color : TEXCOORD0;
};

static const float4 kColors[6] =
{
    float4(1, 0, 0, 1), float4(1, 0, 0, 1),
    float4(0, 1, 0, 1), float4(0, 1, 0, 1),
    float4(0, 0, 1, 1), float4(0, 0, 1, 1)
};

Output main(uint VertexIndex : SV_VertexID)
{
    Output output;
    output.Position = Positions[VertexIndex];
    output.Color = kColors[VertexIndex];
    return output;
}
