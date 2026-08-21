struct PushConstants
{
    float4x4 viewProj;
};

[[vk::push_constant]]
PushConstants params;

Sampler2D baseColor;

struct VSInput {
  float3 pos;
  float3 normal;
  float2 uv;
};

struct VSOutput {
  float4 pos : SV_POSITION;
  float2 uv;
  float3 normal;
};

[shader("vertex")]
VSOutput VSMain(VSInput input) {
  VSOutput o;
  o.pos = mul(float4(input.pos, 1.0), params.viewProj); // vector-first!
  o.uv = input.uv;
  o.normal = input.normal;
  return o;
}

[shader("fragment")]
float4 FSMain(VSOutput input) : SV_Target {
  float4 c = baseColor.Sample(input.uv);
  float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
  float diff = max(dot(normalize(input.normal), lightDir), 0.0);
  return float4(c.rgb * (0.3 + 0.7 * diff), 1.0);
}
