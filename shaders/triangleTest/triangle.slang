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
VSOutput VSMain(VSInput input) {
  VSOutput output;
  output.pos = float4(input.pos.xyz, 1.0);
  return output;
}

// Run on every 'pixel' covered by the triangle
// to deterine its colour in the image
[shader("fragment")]
float4 FSMain(VSOutput input) {
  float3 col = float3(0, 1, 0);
  return float4(col, 1);
}
