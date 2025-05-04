//#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec2 TexCoord;
in float WaveHeight;

out vec4 FragColor;

uniform vec3 CameraPosition;

uniform float FresnelStrength;
uniform float FresnelPower;

uniform vec4 TroughColor;
uniform vec4 SurfaceColor;
uniform vec4 PeakColor;
uniform float Opacity;

uniform float TroughLevel;
uniform float TroughBlend;

uniform float SurfaceLevel;
uniform float SurfaceBlend;

uniform float PeakLevel;
uniform float PeakBlend;

uniform float SandBaseHeight;
uniform float WaterBaseHeight;

vec3 CalculateWaterColor(float waveHeight)
{
    // Normalize waveHeight to [0,1] assuming expected amplitude range
    float normalizedHeight = clamp(waveHeight, 0.0, 1.0);

    // Compute blend factors using smoothstep
    float troughFactor = 1.0 - smoothstep(TroughLevel - TroughBlend, TroughLevel + TroughBlend, normalizedHeight);
    float peakFactor = smoothstep(PeakLevel - PeakBlend, PeakLevel + PeakBlend, normalizedHeight);

    float surfaceFactor = 1.0 - troughFactor - peakFactor;
    surfaceFactor = clamp(surfaceFactor, 0.0, 1.0);

    // Blend colors
    return TroughColor.rgb * troughFactor +
           SurfaceColor.rgb * surfaceFactor +
           PeakColor.rgb * peakFactor;
}


void main()
{
    vec3 vertexNormal = normalize(WorldNormal);
    vec3 viewDirection = normalize(WorldPosition - CameraPosition);
    vec3 reflectDirection = reflect(viewDirection, vertexNormal);
   
    // If you have SampleEnvironment, uncomment this line and comment the fake color
    vec3 reflectionColor = SampleEnvironment(reflectDirection, 1.0);

    float fresnel = FresnelStrength * pow(1.0 - clamp(dot(viewDirection, vertexNormal), 0.0, 1.0), FresnelPower);

    vec3 color = CalculateWaterColor(WaveHeight);

    color = mix(color, reflectionColor, fresnel);

    FragColor = vec4(color, Opacity);
}


