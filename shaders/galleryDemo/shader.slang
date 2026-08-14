struct PushConstants
{
    // view * proj, pushed as a single 64-byte matrix. The model sits at the
    // origin, so there's no separate model matrix.
    float4x4 viewProj;
};

[[vk::push_constant]]
PushConstants params;

struct VSInput {
  float3 pos;    // location 0
  float3 normal; // location 1
  float2 uv;     // location 2
};

struct VSOutput {
  float4 pos : SV_POSITION;
  float3 normal; // world space (model is unrotated, so model == world)
  float3 color;
};

[shader("vertex")]
VSOutput VSMain(VSInput input) {
  VSOutput output;
  // Engine convention: GLM matrices are uploaded directly.
  // Use vector-first multiplication to match the engine's layout.
  output.pos = mul(float4(input.pos, 1.0), params.viewProj);
  output.normal = input.normal;
  output.color = float3(0.55, 0.45, 0.8); // soft violet
  return output;
}

[shader("fragment")]
float4 FSMain(VSOutput input)
{
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));

    // Normal visualization
    float3 color = normal * 0.5 + 0.5;

    // Lambert lighting
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.2;

    color *= ambient + diffuse;

    return float4(color, 1.0);
}
