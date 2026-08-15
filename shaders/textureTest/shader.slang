Sampler2D tex;

// A unit quad centered at the origin, UVs in [0,1]
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
  o.pos = float4(input.pos * 1.8, 1.0);
  o.uv = input.uv;
  o.normal = input.normal;
  return o;
}

[shader("fragment")]
float4 FSMain(VSOutput input) : SV_Target {
  float4 c = tex.Sample(input.uv);
  // The quad's normals are all +z, so this tint is exactly 1.0 — it exists
  // only to keep the location-1 attribute consumed (validation requirement).
  c.rgb *= 0.9 + 0.1 * input.normal.z;
  return c;
}
