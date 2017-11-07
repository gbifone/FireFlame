#include "Mesh.h"

void Mesh::GetMeshDesc(FireFlame::stRawMesh& meshDesc) {
    using namespace FireFlame;
    meshDesc.indexCount = (unsigned int)indices.size();
    meshDesc.indexFormat = FireFlame::Index_Format::UINT16;
    meshDesc.indices = indices.data();

    meshDesc.vertexCount = (unsigned int)vertices.size();
    meshDesc.vertexSize = sizeof(Mesh::Vertex);
    meshDesc.vertexFormat
        = VERTEX_FORMAT_FLOAT3
        | VERTEX_FORMAT_FLOAT3
        | VERTEX_FORMAT_FLOAT3
        | VERTEX_FORMAT_FLOAT2
        | VERTEX_FORMAT_FLOAT2
        | VERTEX_FORMAT_R8G8B8A8_UNORM;
    meshDesc.vertices = vertices.data();
    meshDesc.LocalToWorld = matrixLocal2World;
}

void Mesh::GetSubMeshDesc(FireFlame::stRawMesh::stSubMesh& subMesh) {
    subMesh.indexCount = (unsigned int)indices.size();
    subMesh.startIndexLocation = 0;
    subMesh.baseVertexLocation = 0;
}