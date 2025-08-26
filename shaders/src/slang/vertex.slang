import resources;

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID) {
    Vertex v = pushConstants.vertices[vertexID];
    VSOutput output;

    output.color = v.color;
    output.uv = float2(v.uv_x, v.uv_y);
    output.normal = v.normal;

    float4 position = float4(v.position, 1.0);
    output.fragPosition = mul(pushConstants.renderMatrix, position).xyz;

    output.transformedPosition = mul(sceneData.projection, mul(sceneData.view, float4(output.fragPosition, 1.0)));

    return output;
}