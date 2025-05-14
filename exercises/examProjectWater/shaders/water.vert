//#version 330 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;

out vec3 WorldPosition;
out vec3 WorldNormal;
out vec2 TexCoord;
out float WaveHeight;
out vec4 ClipSpace;

uniform mat4 WorldMatrix;
uniform mat4 ViewProjMatrix;

uniform float WaveAmplitude;
uniform float WaveFrequency;
uniform float WavePersistence;
uniform float WaveLacunarity;
uniform int WaveOctaves;
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

float calculateWaveHeight(float x, float y)
{
    vec2 position = vec2(x, y);

    float total = 0.0;
    float amplitude = 1.0;
    float frequency = WaveFrequency;

    for(int i = 0; i < WaveOctaves; i++)
    {
        float noise = snoise( frequency * position + WaveSpeed*Time);

        total += amplitude * noise;
        amplitude *= WavePersistence;
        frequency *= WaveLacunarity;
    }

    return WaveAmplitude * total;
}

vec3 calculateNormal(vec3 pos, float height)
{
    float eps = 0.001;
    vec3 tangent = normalize(vec3(eps, calculateWaveHeight(pos.x - eps, pos.z) - height, 0.0));
    vec3 bitangent = normalize(vec3(0.0, calculateWaveHeight(pos.x, pos.z - eps) - height, eps));
    vec3 objectNormal = normalize(cross(tangent, bitangent));

   return objectNormal;
}

void main()
{
	WorldPosition = (WorldMatrix * vec4(VertexPosition, 1.0)).xyz;

    float height = calculateWaveHeight(WorldPosition.x, WorldPosition.z);
    WorldPosition.y += height;
    WorldNormal = calculateNormal(WorldPosition.xyz, height);
	TexCoord = VertexTexCoord;
    WaveHeight = height;
    ClipSpace = ViewProjMatrix * vec4(WorldPosition, 1.0);

	gl_Position = ClipSpace;
}
