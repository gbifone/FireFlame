#pragma once
#include <unordered_map>
#include "Primitive\FLD3DPrimitive.h"
#include "Vertex\FLVertex.h"
#include "..\FLTypeDefs.h"

namespace FireFlame {
class Renderer;
class StopWatch;
class Scene {
public:
	Scene(std::shared_ptr<Renderer>& renderer);

    // some scene management
    int  GetReady();
	void Update(const StopWatch& gt);
	void Render(const StopWatch& gt);

    void AddShader(const stShaderDescription& shaderDesc);
	void AddPrimitive(const stRawMesh& mesh);
	void PrimitiveAddSubMesh(const std::string& name, const stRawMesh::stSubMesh& subMesh);

private:
    std::shared_ptr<Renderer> mRenderer;

	// todo : scene manage
	std::unordered_map<std::string, std::unique_ptr<D3DPrimitive>>     mPrimitives; 
    std::unordered_map<std::string, std::shared_ptr<D3DShaderWrapper>> mShaders;
};
}