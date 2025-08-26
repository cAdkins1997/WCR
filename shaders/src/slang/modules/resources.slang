
module resources;

public static const float PI = 3.1415926538;

public struct Vertex {
    public float3 position;
    public float uv_x;
    public float3 normal;
    public float uv_y;
    public float4 color;
};


public struct Material {
    public float4 baseColorFactor;
    public float metalnessFactor;
    public float roughnessFactor;
    public float emissiveStrength;

    public uint baseColorTexture;
    public uint mrTexture;
    public uint normalTexture;
    public uint occlusionTexture;
    public uint emissiveTexture;
};


public struct Light {
    public float3 direction;
    public float3 colour;
    public float intensity;
    public float range;
    public float innerAngle;
    public float outerAngle;
};

public struct PushConstants {
    public float4x4 renderMatrix;
    public ConstBufferPointer<Vertex> vertices;
    public ConstBufferPointer<Material> materials;
    public ConstBufferPointer<Light> lights;
    public uint materialIndex;
    public uint numLights;
};

public [vk::push_constant] ConstantBuffer<PushConstants> pushConstants;

public struct SceneData {
    public float4x4 view;
    public float4x4 projection;
    public float3 cameraPosition;
};

[[vk::binding(0, 0)]]
public ConstantBuffer<SceneData> sceneData;

[[vk::binding(1, 0)]]
public Sampler2D textures[];

public struct VSOutput {
    public float4 color;
    public float3 normal;
    public float3 fragPosition;
    public float4 transformedPosition : SV_Position;
    public float2 uv;
};