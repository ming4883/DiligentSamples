#include "structures.fxh"
#include "common_func.fxh"

StructuredBuffer<PackedMeshVertex> g_VertexData;
StructuredBuffer<MeshIndex> g_IndexData;
StructuredBuffer<MeshInstance> g_InstanceData;

RWStructuredBuffer<CachedMeshVertex> g_CachedVertexData;

cbuffer Constants
{
    GlobalConstants g_Constants;
};

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint DispatchId = DTid.x;
    uint InstanceId = DispatchId / g_Constants.MeshDrawInfo.x;
    uint VertexId = DispatchId - InstanceId * g_Constants.MeshDrawInfo.x;

    if (VertexId >= g_Constants.MeshDrawInfo.x)
        return;

    if (InstanceId >= g_Constants.MeshDrawInfo.y)
        return;

    // Fetch Vertex
    PackedMeshVertex Packed = g_VertexData[g_IndexData[VertexId].Index];
    MeshVertex VtxData;
    VtxData.Pos = Packed.Data0.xyz;
    VtxData.Normal = float3(Packed.Data0.w, Packed.Data1.xy);
    VtxData.UV = Packed.Data1.zw;

    // HLSL matrices are row-major while GLSL matrices are column-major. We will
    // use convenience function MatrixFromRows() appropriately defined by the engine
    uint InstOffset = InstanceId * 4;
    float4x4 InstanceMatr = MatrixFromRows(g_InstanceData[InstOffset].RowData, g_InstanceData[InstOffset+1].RowData, g_InstanceData[InstOffset+2].RowData, g_InstanceData[InstOffset+3].RowData);
    // Apply rotation
    float4 TransformedPos = mul(float4(VtxData.Pos,1.0), g_Constants.Rotation);
    float4 TransformedNrm = mul(float4(VtxData.Normal,0.0), g_Constants.Rotation);
    // Apply instance-specific transformation
    TransformedPos = mul(TransformedPos, InstanceMatr);
    TransformedNrm = mul(float4(TransformedNrm.xyz, 0), InstanceMatr);
    // Apply view-projection matrix
    float4 SvPos = mul(TransformedPos, g_Constants.ViewProj);
    float InvW = 1.0 / SvPos.w;
    SvPos.xy *= InvW;
    float2 OctNrm = UnitVectorToOctahedron(normalize(TransformedNrm.xyz));
    CachedMeshVertex Cached;
    Cached.PosDepthNormalUV.x = PackFloats(SvPos.x, SvPos.y);
    Cached.PosDepthNormalUV.y = InvW;
    Cached.PosDepthNormalUV.z = PackFloats(OctNrm.x, OctNrm.y);
    Cached.PosDepthNormalUV.w = PackFloats(VtxData.UV.x, VtxData.UV.y);

    g_CachedVertexData[DispatchId] = Cached;
}
