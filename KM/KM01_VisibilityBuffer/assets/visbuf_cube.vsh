#include "structures.fxh"
#include "common_func.fxh"

StructuredBuffer<PackedMeshVertex> g_VertexData;
StructuredBuffer<MeshIndex> g_IndexData;
StructuredBuffer<MeshInstance> g_InstanceData;

cbuffer Constants
{
    GlobalConstants g_Constants;
};

struct VSInput
{
    uint VertId : SV_VertexID;
    uint InstId : SV_InstanceID;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 UV  : TEX_COORD;
    nointerpolation uint Id : ID;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be identical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    // Fetch Vertex
    PackedMeshVertex Packed = g_VertexData[g_IndexData[VSIn.VertId].Index];
    MeshVertex VtxData;
    VtxData.Pos = Packed.Data0.xyz;
    VtxData.Normal = float3(Packed.Data0.w, Packed.Data1.xy);
    VtxData.UV = Packed.Data1.zw;

    // HLSL matrices are row-major while GLSL matrices are column-major. We will
    // use convenience function MatrixFromRows() appropriately defined by the engine
    uint InstOffset = VSIn.InstId * 4;
    float4x4 InstanceMatr = MatrixFromRows(g_InstanceData[InstOffset].RowData, g_InstanceData[InstOffset+1].RowData, g_InstanceData[InstOffset+2].RowData, g_InstanceData[InstOffset+3].RowData);

    // Apply rotation
    float4x4 ToWorldMtx = mul(g_Constants.Rotation, InstanceMatr);
    float4 TransformedPos = mul(float4(VtxData.Pos,1.0), ToWorldMtx);

    // Apply view-projection matrix
    PSIn.Pos    = mul(TransformedPos, g_Constants.ViewProj);

    PSIn.UV     = VtxData.UV;
    uint TriId  = VSIn.VertId / 3;
    PSIn.Id     = ((VSIn.InstId & 0xffff) << 16) | (TriId & 0xffff);
}
