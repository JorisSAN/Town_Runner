"#ifndef _SHADER_UTILITIES_FXH_\n"
"#define _SHADER_UTILITIES_FXH_\n"
"\n"
"// Transforms camera-space Z to normalized device z coordinate\n"
"float CameraZToNormalizedDeviceZ(in float CameraZ, in float4x4 mProj)\n"
"{\n"
"    // In Direct3D and Vulkan, normalized device z range is [0, +1]\n"
"    // In OpengGL, normalized device z range is [-1, +1] (unless GL_ARB_clip_control extension is used to correct this nonsense).\n"
"    return MATRIX_ELEMENT(mProj,2,2) + MATRIX_ELEMENT(mProj,3,2) / CameraZ;\n"
"}\n"
"\n"
"// Transforms the normal from tangent space to world space using the\n"
"// position and UV derivatives.\n"
"float3 TransformTangentSpaceNormalGrad(in float3 dPos_dx,     // Position dx derivative\n"
"                                       in float3 dPos_dy,     // Position dy derivative\n"
"                                       in float2 dUV_dx,      // Normal map UV coordinates dx derivative\n"
"                                       in float2 dUV_dy,      // Normal map UV coordinates dy derivative\n"
"                                       in float3 MacroNormal, // Macro normal, must be normalized\n"
"                                       in float3 TSNormal     // Tangent-space normal\n"
"                                       )\n"
"\n"
"{\n"
"	float3 n = MacroNormal;\n"
"\n"
"    float3 t = (dUV_dy.y * dPos_dx - dUV_dx.y * dPos_dy) / (dUV_dx.x * dUV_dy.y - dUV_dy.x * dUV_dx.y);\n"
"    t = normalize(t - n * dot(n, t));\n"
"\n"
"    float3 b = normalize(cross(t, n));\n"
"\n"
"    float3x3 tbn = MatrixFromRows(t, b, n);\n"
"\n"
"    return normalize(mul(TSNormal, tbn));\n"
"}\n"
"\n"
"// Transforms the normal from tangent space to world space, without using the\n"
"// explicit tangent frame.\n"
"float3 TransformTangentSpaceNormal(in float3 Position,    // Vertex position in world space\n"
"                                   in float3 MacroNormal, // Macro normal, must be normalized\n"
"                                   in float3 TSNormal,    // Tangent-space normal\n"
"                                   in float2 NormalMapUV  // Normal map uv coordinates\n"
"                                   )\n"
"{\n"
"    float3 dPos_dx = ddx(Position);\n"
"    float3 dPos_dy = ddy(Position);\n"
"\n"
"    float2 dUV_dx = ddx(NormalMapUV);\n"
"    float2 dUV_dy = ddy(NormalMapUV);\n"
"\n"
"    return TransformTangentSpaceNormalGrad(dPos_dx, dPos_dy, dUV_dx, dUV_dy, MacroNormal, TSNormal);\n"
"}\n"
"\n"
"#endif //_SHADER_UTILITIES_FXH_\n"