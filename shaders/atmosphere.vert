struct Output
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Output main(uint VertexID : SV_VertexID)
{
    Output output;
    output.TexCoord = float2((VertexID << 1) & 2, VertexID & 2);
    output.Position = float4(output.TexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
    output.Position.y *= -1.0;
    return output;
}
