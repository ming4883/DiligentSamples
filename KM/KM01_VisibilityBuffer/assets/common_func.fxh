
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
