//#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec2 TexCoord;
in float WaveHeight;
in vec4 ClipSpace;

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

uniform float PeakLevel;
uniform float PeakBlend;

uniform float SandBaseHeight;
uniform float WaterBaseHeight;

uniform sampler2D ReflectionTexture;
uniform vec2 ReflectionTextureScale;

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
    //vec3 cubeReflectionColor = SampleEnvironment(reflectDirection, 1.0);

    //vec2 ndc = (ClipSpace.xy / ClipSpace.w) / 2.0 + 0.5;
    //vec2 reflectionTextureCoords = vec2(ndc.x, 1.0 - ndc.y);
    //vec4 reflectionColor = texture(ReflectionTexture, TexCoord * reflectionTextureCoords * ReflectionTextureScale);


    vec2 ndc = ((ClipSpace.xy / ClipSpace.w) * 0.5) + 0.5;
    
    vec2 flippedTextCoord = vec2(1.0 - TexCoord.x, 1.0 - TexCoord.y);
    vec4 reflectionColor = texture(ReflectionTexture, flippedTextCoord * ndc * ReflectionTextureScale);

    float fresnel = FresnelStrength * pow(1.0 - clamp(dot(viewDirection, vertexNormal), 0.0, 1.0), FresnelPower);

    vec3 color = CalculateWaterColor(WaveHeight);

    color = mix(color, reflectionColor.xyz, fresnel);
    //color = mix(color, cubeReflectionColor.xyz, fresnel);

    //FragColor = vec4(reflectionColor.rgb, 1.0);

    FragColor = vec4(color, Opacity);
}


