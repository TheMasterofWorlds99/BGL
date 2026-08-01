[[vk::push_constant]]
cbuffer PushConstants
{
    float fov;
    float aspect;
    float pitch;
    float viewHeight;
};

static const float Deg2Rad = 3.14159265f / 180.0f;

struct VSInput {
  float3 pos;
  float3 normal;
  float2 uv;
};

struct VSOutput {
  float4 pos : SV_POSITION;
};

// Run on every vertex in the mesh, allowing us to manipulate it's data before rendering
[shader("vertex")]
VSOutput VSMain(VSInput input) 
{
  VSOutput output;
  output.pos = float4(input.pos * 3, 1);
  return output;
}

// PCG!
uint NextRandomUInt(inout uint state)
{
  state = state * 747796405 + 2891336453;
  uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
  result = (result >> 22) ^ result;
  return result;
}

// Pseudo random value [0, 1] from 2D coord!
float GetRandomValue(float2 coord)
{
  uint rngState = dot(asuint(coord), uint2(19349669, 83492837));
  return NextRandomUInt(rngState) / 4294967295.0;
}

float SmoothNoise(float2 pos)
{
  // Get random values at each corner
  int2 cellCentre = floor(pos);
  float bl = GetRandomValue(cellCentre + int2(0,0));
  float tl = GetRandomValue(cellCentre + int2(0,1));
  float br = GetRandomValue(cellCentre + int2(1,0));
  float tr = GetRandomValue(cellCentre + int2(1,1));

  // Now we interpolate and smooth the noise
  float2 uv = frac(pos);
  uv = smoothstep(0, 1, uv);
  bl *= (1 - uv.x) * (1 - uv.y);
  tl *= (1 - uv.x) * uv.y;
  br *= uv.x * (1 - uv.y);
  tr *= uv.x * uv.y;
  return bl + tl + br + tr;
}

float FractalNoise(float2 pos, int layerCount)
{
    float sum = 0;
    float amplitude = 1;
    float frequency = 1;
    float totalAmplitude = 0;

    for(int i = 0; i < layerCount; i++)
    {
        sum += SmoothNoise(pos * frequency) * amplitude;
        
        totalAmplitude += amplitude;
        amplitude *= 0.5;
        frequency *= 2;
    }

    return sum / totalAmplitude;
}

float3 GetViewDir(float fov, float aspectRatio, float pitch, float2 uv)
{
  float sensorHeight = tan(fov * 0.5) * 2;
  float sensorWidth = sensorHeight * aspectRatio;
  float2 sensorPos = (uv - 0.5) * float2(sensorWidth, sensorHeight);

  float3 right = float3(1, 0, 0);
  float3 fwd = float3(0, sin(pitch), cos(pitch));
  float3 up = cross(fwd, right);

  float3 viewDir = right * sensorPos.x + up * sensorPos.y + fwd;
  return normalize(viewDir);
}

float3 Draw(float2 uv)
{
  float3 viewDir = GetViewDir(fov * Deg2Rad, aspect, pitch * Deg2Rad, uv);

  if (viewDir.y <= 0.0)
  {
    float skyFactor = saturate(-viewDir.y);
    skyFactor = pow(skyFactor, 0.7); // Smooth curve

    float3 horizonColor = float3(0.65, 0.82, 0.98); // Light atmosphere near horizon
    float3 zenithColor  = float3(0.18, 0.48, 0.92); // Deeper blue sky up top

    return lerp(horizonColor, zenithColor, skyFactor);
  }

  float3 viewPos = float3(0, max(0.1, viewHeight), 0);
    float dstToGround = -viewPos.y / viewDir.y;
    float3 groundPos = viewPos + viewDir * dstToGround;

  const float islandRadius = 150.0;
  const float noiseScale = 0.15;

  float noisey = FractalNoise(groundPos.xz * noiseScale, 5);
  float dist = length(groundPos.xz) / islandRadius;
  float elevation = noisey - dist;

  const float seaLevel = 0.31;
  const float beachHeight = 0.33;

  float3 deepWaterColor    = float3(0.01, 0.08, 0.35);
  float3 shallowWaterColor = float3(0.18, 0.60, 0.88);
  float3 sandColor         = float3(0.96, 0.82, 0.59);
  float3 grassColor        = float3(0.15, 0.55, 0.03);

  // Calculate Ocean Water color based on depth
  float waterDepth = saturate((seaLevel - elevation) / seaLevel);
  waterDepth = pow(waterDepth, 0.85); // Non-linear falloff for natural clarity
  float3 waterColor = lerp(shallowWaterColor, deepWaterColor, waterDepth);

  // Calculate Land color (Sand -> Grass)
  float grassWeight = smoothstep(seaLevel + 0.005, beachHeight + 0.02, elevation);
  float3 landColor  = lerp(sandColor, grassColor, grassWeight);

  // Smooth transition from Ocean to Beach
  float landWeight = smoothstep(seaLevel - 0.005, seaLevel + 0.01, elevation);

  return lerp(waterColor, landColor, landWeight);
}

// Run on every 'pixel' covered by the triangle
// to deterine its colour in the image
[shader("fragment")]
float4 FSMain(VSOutput input) 
{
  float2 pixelCoord = input.pos.xy;
  // Normalize to range [0, 1] Todo: don't hardcode resolution
  float2 screenUV = pixelCoord / float2(1280, 720);

  float3 col = Draw(screenUV);
  return float4(col, 1);
}
