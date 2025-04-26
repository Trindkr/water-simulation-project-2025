#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec2 TexCoord;
in float WaveHeight;

out vec4 FragColor;

uniform vec4 TroughColor;
uniform vec4 SurfaceColor;
uniform vec4 PeakColor;
uniform float Opacity;

uniform float TroughThreshold;
uniform float TroughTransition;
uniform float PeakThreshold;
uniform float PeakTransition;

void main()
{

    float troughToSurface = smoothstep(TroughThreshold-TroughTransition,TroughThreshold +TroughTransition, WaveHeight);
    float surfaceToPeak = smoothstep(PeakThreshold-PeakTransition, PeakThreshold + PeakTransition, WaveHeight);

    vec3 mixedColor1 = mix(TroughColor.rgb, SurfaceColor.rgb, troughToSurface);
    vec3 mixedColor2 = mix(mixedColor1, PeakColor.rgb, surfaceToPeak);
    

    

    FragColor = vec4(mixedColor2.rgb, Opacity);
}


