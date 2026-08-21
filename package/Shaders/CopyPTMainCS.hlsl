Texture2D<float4> ptMainTexture     : register(t0);
RWTexture2D<float4> gameMainTexture : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    uint width, height;
    gameMainTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
        return;

    float4 pt = ptMainTexture[id];
    float3 game = gameMainTexture[id].rgb;

    // Alpha channel: 0.0 = primary ray miss (sky), 1.0 = primary ray hit
    float3 blended = lerp(game, pt.rgb, pt.a);

    gameMainTexture[id] = float4(blended, 1.0f);
}
