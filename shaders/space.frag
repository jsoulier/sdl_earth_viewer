struct Input
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

cbuffer UniformBuffer : register(b0, space3)
{
    float4x4 InverseViewProjection;
};

cbuffer UniformBuffer : register(b1, space3)
{
    float4 CameraPosition;
};

cbuffer UniformBuffer : register(b2, space3)
{
    float3 SunDirection;
    float Padding0;
};

float Hash(float3 value)
{
    value = frac(value * float3(443.897, 441.423, 437.195));
    value += dot(value, value.yzx + 19.19);
    return frac((value.x + value.y) * value.z);
}

float3 GetStarColor(float h)
{
    static const float3 kBlueWhite = float3(0.55, 0.75, 1.0);
    static const float3 kWhite = float3(1.0, 1.0, 1.0);
    static const float3 kYellow = float3(1.0, 0.95, 0.8);
    static const float3 kOrangeRed = float3(1.0, 0.65, 0.4);
    if (h < 0.25)
    {
        return lerp(kBlueWhite, kWhite, h / 0.25);
    }
    else if (h < 0.50)
    {
        return lerp(kWhite, kYellow, (h - 0.25) / 0.25);
    }
    else
    {
        return lerp(kYellow, kOrangeRed, (h - 0.5) / 0.5);
    }
}

float4 main(Input input) : SV_Target
{
    float2 screenPosition = input.TexCoord * 2.0 - 1.0;
    float3 rayOrigin = CameraPosition.xyz;
    float4 farWorldFull = mul(InverseViewProjection, float4(screenPosition, 0.0, 1.0));
    float3 rayDirection;
    if (farWorldFull.w == 0.0)
    {
        rayDirection = normalize(farWorldFull.xyz);
    }
    else
    {
        rayDirection = normalize(farWorldFull.xyz / farWorldFull.w - rayOrigin);
    }
    float3 color = float3(0, 0, 0);
    float3 starDirection = rayDirection;
    for (int i = 0; i < 3; i++)
    {
        float scale = 140.0 * (i + 1);
        float3 position = starDirection * scale;
        float3 id = floor(position);
        float hash = Hash(id);
        float starThreshold = 0.965;
        if (hash > starThreshold)
        {
            float3 starPosition = id + 0.5;
            float offset = length(position - starPosition);
            float brightness = (hash - starThreshold) / (1.0 - starThreshold);
            float3 starColor = GetStarColor(Hash(id + 0.1));
            color += starColor * brightness * 4.0 * exp(-offset * 18.0); 
        }
    }
    float cosTheta = dot(rayDirection, SunDirection);
    float3 sunBaseColor = GetStarColor(0.45);
    float sunDisc = smoothstep(0.9998, 0.9999, cosTheta);
    color += sunDisc * sunBaseColor * 20.0;
    for (int j = 0; j < 3; j++)
    {
        static const float kPowers[3] = {8000.0, 800.0, 20.0};
        static const float kBVs[3] = {0.4, 0.7, 0.95};
        static const float kIntensities[3] = {10.0, 1.5, 0.3};
        color += pow(max(0.0, cosTheta), kPowers[j]) * GetStarColor(kBVs[j]) * kIntensities[j];
    }
    return float4(color, 1.0);
}
