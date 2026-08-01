struct InstanceData {
  float4 pos;
  float4 vel;
  float4 spinAxis;
  float radius;
  float mass;
  float spinSpeed;
  float padding; // Keeps struct exactly 64-bytes aligned!
};

struct PushConstants
{
    float2 resolution;
    float time;

    InstanceData* instancesPtr;
    float deltaTime;
    int count;
};

[[vk::push_constant]]
PushConstants params;

// The bunnies are scattered randomly in a long cloud along -z: x/y are random
// within a box around the flight path (with the camera path itself kept
// clear), and the whole swarm drifts gently toward the camera — a finite,
// one-time fly-through, no respawning. Keep these constants in sync with
// initBunnyInstances() in 3DDemo.cpp!
static const float LineLength = 350.0;     // how far the cloud extends in -z
static const float LineSpread = 150.0;     // random x/y scatter around the path
static const float MinRadius = 10.0;       // keep the camera path clear
static const float DriftSpeed = 20.0;      // units per second toward the camera
static const float TwoPi = 6.2831853;

struct VSInput {
  // Per-vertex
  float3 pos;
  float3 normal;
  float2 uv;

  // Per-instance - just a plain vertex buffer the CPU rewrites
  // every frame with the latest physics state. Order/offsets must match
  // InstanceData in 3DDemo.cpp; vel and mass aren't needed here so they're
  // skipped over.
  float4 instancePos;
  float4 instanceSpinAxis; // w unused
  float instanceRadius;
  float instanceSpinSpeed;
};

struct VSOutput {
  float4 pos : SV_POSITION;
  float3 normal;
  float3 color;
};

uint PCG(inout uint state)
{
    uint word = ((state >> ((state >> 28) + 4)) ^ state) * 277803737u;
    state = state * 747796405u + 2891336453u;
    return (word >> 22) ^ word;
}

float RandomUNorm(inout uint state)
{
    return PCG(state) * (1.0 / 4294967295.0);
}

float3 RandomCol(inout uint state)
{
    return float3(RandomUNorm(state), RandomUNorm(state), RandomUNorm(state));
}

float3 RotateAroundAxis(float3 v, float3 axis, float angle)
{
    float s = sin(angle);
    float c = cos(angle);

    return
        v * c +
        cross(axis, v) * s +
        axis * dot(axis, v) * (1.0 - c);
}

[shader("vertex")]
VSOutput VSMain(VSInput input, uint instanceIndex : SV_InstanceID) {
  // Per-instance spin axis/speed are fixed at spawn time (CPU side); the
  // actual rotation *angle* is derived from params.time right here in the
  // shader, so spin animates smoothly without the CPU touching it every frame
  float3 spinAxis = input.instanceSpinAxis.xyz;
  float spinSpeed = input.instanceSpinSpeed;

  // Colour stays procedural (seeded by the instance index)
  uint rngState = instanceIndex;

  // Create rotate axes
  float3 iHat = spinAxis;
  float3 jHat = normalize(cross(iHat, float3(0, 1, 0)));
  // Wrap the spin angle so precision doesn't degrade during long sessions
  float spinAngle = fmod(params.time * spinSpeed, TwoPi);
  jHat = RotateAroundAxis(jHat, iHat, spinAngle);
  float3 kHat = cross(iHat, jHat);

  float3 vertWorld = iHat * input.pos.x + jHat * input.pos.y + kHat * input.pos.z;
  vertWorld = vertWorld * input.instanceRadius + input.instancePos.xyz;

  // Note: using a very small 'near' value or very large 'far' value will cause
  // the depth precision to suffer, potentially resulting in visible artifacts
  const float depthNear = 0.01;
  const float depthFar = 350; // long enough to cover the whole line

  // View space is right-handed: the camera sits at the origin looking down -z,
  // so a bunny's front face (+z) naturally faces the camera. The line drifts
  // toward us (see CSMain) and recycles bunnies behind the camera.
  float3 vertView = vertWorld;

  // Distance from the camera is positive for anything in front of it
  float dist = -vertView.z;

  // Map distance linearly to a depth value in [0, 1] (near = 0, far = 1).
  // The pipeline uses VK_COMPARE_OP_LESS_OR_EQUAL, so the closest fragment
  // (smallest depth) wins — no z-flip required.
  float depth = (dist - depthNear) / (depthFar - depthNear);
  // Perspective projection with a linear depth term (camera at the origin)
  float4 vertClip = float4(vertView.xy, depth * dist, dist);
  // Correct for aspect ratio
  vertClip.x *= params.resolution.y / params.resolution.x;

  VSOutput output;
  output.pos = vertClip;

  float3 normalWorld =
  iHat * input.normal.x +
  jHat * input.normal.y +
  kHat * input.normal.z;

  output.normal = normalize(normalWorld);
  output.color = RandomCol(rngState);
  return output;
}

[shader("fragment")]
float4 FSMain(VSOutput input)
{
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));

    float diffuse = max(dot(input.normal, lightDir), 0.0);

    float ambient = 0.2;

    float3 color = input.color * (ambient + diffuse);

    return float4(color, 1.0);
}

// Compute


[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 threadIndex : SV_DispatchThreadID) {
  if (threadIndex.x >= params.count) return;

  // Read from the pointer exactly like you would in C++!
  InstanceData bunny = params.instancesPtr[threadIndex.x];

  // Bunnies fly gently toward the camera (sitting at the origin, looking down
  // -z) and drift past it. x/y are left untouched, so each bunny holds its
  // spot in the cloud and just spins in place (see VSMain).
  bunny.pos.z += DriftSpeed * params.deltaTime;

  // Write changes back to the pointer!
  params.instancesPtr[threadIndex.x] = bunny;
}
