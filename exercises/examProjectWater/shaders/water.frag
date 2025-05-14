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

uniform float WaveFrequency;
uniform float WaveAmplitude;
uniform float WaveSpeed;
uniform float Time;


// Simplex 2D noise
//
vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

float snoise(vec2 v){
  const vec4 C = vec4(0.211324865405187, 0.366025403784439,
           -0.577350269189626, 0.024390243902439);
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod(i, 289.0);
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
  + i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),
    dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

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
   
    // ndc 
    vec2 ndc = ((ClipSpace.xy / ClipSpace.w) * 0.5) + 0.5;
    vec2 reflectTextCoords = vec2(1.0 - ndc.x, ndc.y);

    //add distortion
    float baseDistortion = 0.05;
    float distortionStrength = baseDistortion * WaveAmplitude;

    float frequency = WaveFrequency * 5.0;
    float noiseX = snoise(WorldPosition.xz * frequency + vec2(Time * WaveSpeed));
    float noiseY = snoise((WorldPosition.xz + vec2(13.0, 7.0)) * frequency + vec2(Time * WaveSpeed));
    vec2 distortion = vec2(noiseX, noiseY) * distortionStrength;

    reflectTextCoords += distortion;

    // Clamp to avoid sampling outside texture
    reflectTextCoords = clamp(reflectTextCoords, vec2(0.001), vec2(0.999));

    vec4 reflectionColor = texture(ReflectionTexture, reflectTextCoords);

    float fresnel = FresnelStrength * pow(1.0 - clamp(dot(viewDirection, vertexNormal), 0.0, 1.0), FresnelPower);

    vec3 color = CalculateWaterColor(WaveHeight);

    color = mix(color, reflectionColor.xyz, fresnel);
    //color = mix(color, cubeReflectionColor.xyz, fresnel);

    //FragColor = vec4(reflectionColor.rgb, 1.0);

    FragColor = vec4(color, Opacity);
}


