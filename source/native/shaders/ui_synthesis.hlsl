// UI layer synthesis (see hudless_capture.hpp). Compiled by
// tools/companion/compile_shaders.ps1 into ../ui_synthesis_cs.h; only bytecode ships.
Texture2D<float4> Final : register(t0);
Texture2D<float4> Hudless : register(t1);
RWTexture2D<float4> Ui : register(u0);
RWByteAddressBuffer Coverage : register(u1);
cbuffer Size : register(b0) { uint2 size; };
groupshared uint covered;
[numthreads(8, 8, 1)]
void main(uint2 p : SV_DispatchThreadID, uint index : SV_GroupIndex) {
  if (index == 0) covered = 0;
  GroupMemoryBarrierWithGroupSync();
  if (all(p < size)) {
    // UI edges are antialiased: take the largest difference around the pixel.
    float d = 0;
    [unroll] for (int y = -1; y <= 1; ++y)
      [unroll] for (int x = -1; x <= 1; ++x) {
        int2 q = clamp(int2(p) + int2(x, y), int2(0, 0), int2(size) - 1);
        float3 e = abs(Final[q].rgb - Hudless[q].rgb);
        d = max(d, max(e.r, max(e.g, e.b)));
      }
    // Differences up to 3/255 are dithering; 12/255 and above are fully UI.
    float a = saturate((d - 3.0 / 255.0) / (9.0 / 255.0));
    float3 f = Final[p].rgb;
    Ui[p] = float4(max(f - (1.0 - a) * Hudless[p].rgb, 0.0), a);
    if (a > 0.5) InterlockedAdd(covered, 1);
  }
  GroupMemoryBarrierWithGroupSync();
  if (index == 0 && covered) Coverage.InterlockedAdd(0, covered);
}
