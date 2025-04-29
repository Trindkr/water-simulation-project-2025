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

uniform float TroughThreshold;
uniform float TroughTransition;
uniform float PeakThreshold;
uniform float PeakTransition;

vec3 CalculateWaterColor ()
{
    float troughToSurface = smoothstep(TroughThreshold-TroughTransition,TroughThreshold +TroughTransition, WorldNormal.y);
    float surfaceToPeak = smoothstep(PeakThreshold-PeakTransition, PeakThreshold + PeakTransition, WorldPosition.y);

    vec3 mixedColor1 = mix(TroughColor.rgb, SurfaceColor.rgb, troughToSurface);
    vec3 mixedColor2 = mix(mixedColor1, PeakColor.rgb, surfaceToPeak);

    return mixedColor2;
}


void main()
{
    vec3 vertexNormal = normalize(WorldNormal);
    vec3 viewDirection = normalize(WorldPosition - CameraPosition);
    vec3 reflectDirection = reflect(viewDirection, vertexNormal);
    reflectDirection.x *= -1.0;

    // If you have SampleEnvironment, uncomment this line and comment the fake color
    vec3 reflectionColor = SampleEnvironment(reflectDirection, 1.0);

    float fresnel = FresnelStrength * pow(1.0 - clamp(dot(viewDirection, vertexNormal), 0.0, 1.0), FresnelPower);

    vec3 color = CalculateWaterColor();

    color = mix(color, reflectionColor, fresnel);

    FragColor = vec4(color, Opacity);
}


