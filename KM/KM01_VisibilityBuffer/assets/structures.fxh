
#ifdef __cplusplus

using half2 = Vector2<uint16_t>;
using half3 = Vector3<uint16_t>;
using half4 = Vector4<uint16_t>;

#endif

struct MeshVertex
{
    float3 Pos;
    float3 Normal;
    float2 UV;
};

struct PackedMeshVertex
{
    float4 Data0; // pos.xyz, normal.x
    float4 Data1; // normal.yz, uv
};

struct MeshIndex
{
    uint Index;
};

struct MeshInstance
{
    float4 RowData;
};


struct CachedMeshVertex
{
    float4 PosDepthNormalUV;
};

struct GlobalConstants
{
    float4x4    ViewProj;
    float4x4    ViewProjInv;
    float4x4    Rotation;
    float4      ScreenSizeInvSize;
    float4      ViewLocation;
    uint4       MeshDrawInfo; // vertex count, instance count, padding, padding
};