
float PackFloats(float a, float b) {

    //Packing
    uint a16 = f32tof16(a);
    uint b16 = f32tof16(b);
    uint abPacked = (a16 << 16) | b16;
    return asfloat(abPacked);
}

void UnpackFloat(float input, out float a, out float b) {

    //Unpacking
    uint uintInput = asuint(input);
    a = f16tof32(uintInput >> 16);
    b = f16tof32(uintInput);
}

// Octahedron Normal Vectors
// [Cigolle 2014, "A Survey of Efficient Representations for Independent Unit Vectors"]
//						Mean	Max
// oct		8:8			0.33709 0.94424
// snorm	8:8:8		0.17015 0.38588
// oct		10:10		0.08380 0.23467
// snorm	10:10:10	0.04228 0.09598
// oct		12:12		0.02091 0.05874

float2 UnitVectorToOctahedron( float3 N )
{
	N.xy /= dot( 1, abs(N) );
	if( N.z <= 0 )
	{
		N.xy = ( 1 - abs(N.yx) ) * ( N.xy >= 0 ? float2(1,1) : float2(-1,-1) );
	}
	return N.xy;
}

float3 OctahedronToUnitVector( float2 Oct )
{
	float3 N = float3( Oct, 1 - dot( 1, abs(Oct) ) );
	if( N.z < 0 )
	{
		N.xy = ( 1 - abs(N.yx) ) * ( N.xy >= 0 ? float2(1,1) : float2(-1,-1) );
	}
	return normalize(N);
}