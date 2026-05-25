// Modified from Alan Zucconi:
// https://www.alanzucconi.com/2017/10/10/atmospheric-scattering-1/

struct Input
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Texture2D DepthTexture : register(t0, space2);
SamplerState DepthSampler : register(s0, space2);

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

static const float kPI = 3.14159265359;
static const float kSunIntensity = 20.0;
static const float3 kScatteringCoefficient = float3(5.8, 13.5, 33.1) * 1e-6;
static const int kViewSamples = 16;
static const int kLightSamples = 8;
static const float3 kPlanetCenter = float3(0, 0, 0);
static const float kPlanetRadius = 6378137.0;
static const float kAtmosphereRadius = 6478137.0;
static const float kScaleHeight = 8000.0;

bool RayIntersect(float3 O, float3 D, float3 C, float R, out float t0, out float t1)
{
    t0 = 0.0;
    t1 = 0.0;
    float3 L = C - O;
    float tca = dot(L, D);
    float d2 = dot(L, L) - tca * tca;
    if (d2 > R * R)
    {
        return false;
    }
    float thc = sqrt(max(0.0, R * R - d2));
    t0 = tca - thc;
    t1 = tca + thc;
    return true;
}

bool LightSampling(float3 P, float3 S, out float opticalDepthCP)
{
    float t0;
    float t1;
    if (!RayIntersect(P, S, kPlanetCenter, kAtmosphereRadius, t0, t1))
    {
        opticalDepthCP = 0.0;
        return false;
    }
    float distanceToExit = t1;
    float ds = distanceToExit / (float)kLightSamples;
    float time = 0;
    opticalDepthCP = 0;
    for (int i = 0; i < kLightSamples; i++)
    {
        float3 Q = P + S * (time + ds * 0.5);
        float height = length(Q - kPlanetCenter) - kPlanetRadius;
        if (height < 0.0)
        {
            return false;
        }
        opticalDepthCP += exp(-height / kScaleHeight) * ds;
        time += ds;
    }
    return true;
}

float3 CalculateAtmosphericScattering(float3 O, float3 D, float tA, float tB, float3 S)
{
    float cosTheta = dot(D, S);
    float phase = (3.0 / (16.0 * kPI)) * (1.0 + cosTheta * cosTheta);
    float3 totalViewSamples = float3(0, 0, 0);
    float opticalDepthPA = 0;
    float time = tA;
    float ds = (tB - tA) / (float)kViewSamples;
    for (int i = 0; i < kViewSamples; i++)
    {
        float3 P = O + D * (time + ds * 0.5);
        float height = length(P - kPlanetCenter) - kPlanetRadius;
        float opticalDepthSegment = exp(-max(0.0, height) / kScaleHeight) * ds;
        opticalDepthPA += opticalDepthSegment;
        float opticalDepthCP = 0;
        if (LightSampling(P, S, opticalDepthCP))
        {
            float3 transmittance = exp(-kScatteringCoefficient * (opticalDepthCP + opticalDepthPA));
            totalViewSamples += transmittance * opticalDepthSegment;
        }
        time += ds;
    }
    return kSunIntensity * kScatteringCoefficient * phase * totalViewSamples;
}

float4 main(Input input) : SV_Target
{
    float rawDepth = DepthTexture.Sample(DepthSampler, input.TexCoord).r;
    float2 screenPosition = input.TexCoord * 2.0 - 1.0;
    screenPosition.y = -screenPosition.y;
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
    float sceneDistance;
    if (rawDepth == 0.0)
    {
        sceneDistance = 1e12;
    }
    else
    {
        float4 worldPositionFull = mul(InverseViewProjection, float4(screenPosition, rawDepth, 1.0));
        sceneDistance = length(worldPositionFull.xyz / worldPositionFull.w - rayOrigin);
    }
    float t0;
    float t1;
    if (!RayIntersect(rayOrigin, rayDirection, kPlanetCenter, kAtmosphereRadius, t0, t1))
    {
        return float4(0, 0, 0, 0);
    }
    float tA = max(0.0, t0);
    float tB = min(t1, sceneDistance);
    float tp0;
    float tp1;
    if (RayIntersect(rayOrigin, rayDirection, kPlanetCenter, kPlanetRadius, tp0, tp1))
    {
        if (tp0 > 0.0)
        {
            tB = min(tB, tp0);
        }
    }
    if (tA >= tB)
    {
        return float4(0, 0, 0, 0);
    }
    float3 scattering = CalculateAtmosphericScattering(rayOrigin, rayDirection, tA, tB, SunDirection);
    scattering = clamp(scattering, 0.0, 10.0);
    return float4(scattering, 1.0);
}
