
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

StructuredBuffer<PackedMeshVertex> g_VertexData;
StructuredBuffer<MeshIndex> g_IndexData;
StructuredBuffer<MeshInstance> g_InstanceData;

cbuffer Constants
{
    float4x4 g_ViewProj;
    float4x4 g_Rotation;
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
    float4 TransformedPos = mul(float4(VtxData.Pos,1.0), g_Rotation);
    // Apply instance-specific transformation
    TransformedPos = mul(TransformedPos, InstanceMatr);
    // Apply view-projection matrix
    PSIn.Pos    = mul(TransformedPos, g_ViewProj);
    PSIn.UV.xy  = VtxData.UV;
    PSIn.Id     = ((VSIn.InstId & 0xffff) << 16) | (VSIn.VertId & 0xffff);
}
