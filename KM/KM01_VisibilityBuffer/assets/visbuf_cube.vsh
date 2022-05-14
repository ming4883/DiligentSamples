#include "structures.fxh"
#include "common_func.fxh"

StructuredBuffer<CachedMeshVertex> g_CachedVertexData;


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
    uint BufferOffset = VSIn.InstId * g_Constants.MeshDrawInfo.x + VSIn.VertId;

    CachedMeshVertex Cached = g_CachedVertexData[BufferOffset];

    // unpack SvPos
    UnpackFloat(Cached.PosDepthNormalUV.x, PSIn.Pos.x, PSIn.Pos.y);
    PSIn.Pos.z  = Cached.PosDepthNormalUV.y;
    PSIn.Pos.w  = 1.0;

    // unpack UV
    UnpackFloat(Cached.PosDepthNormalUV.w, PSIn.UV.x, PSIn.UV.y);

    uint TriId  = VSIn.VertId / 3;
    PSIn.Id     = ((VSIn.InstId & 0xffff) << 16) | (TriId & 0xffff);
}
