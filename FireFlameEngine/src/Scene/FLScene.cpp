#include "PCH.h"
#include "FLScene.h"
#include "..\Renderer\FLD3DRenderer.h"
#include "..\ShaderWrapper\FLD3DShaderWrapper.h"
#include "..\Timer\FLStopWatch.h"
#include "..\Engine\FLEngine.h"
#include "..\PSOManager\FLD3DPSOManager.h"
#include "Pass\FLPass.h"
#include "..\Material\FLMaterial.h"
#include "..\Material\FLTexture.h"
#ifdef USE_MS_DDS_LOADER
#include "..\3rd_utils\DDSTextureLoader12.h"
#else
#include "..\3rd_utils\DDSTextureLoaderLuna.h"
#endif

namespace FireFlame {
Scene::Scene(std::shared_ptr<D3DRenderer>& renderer) : mRenderer(renderer){}

void Scene::Update(const StopWatch& gt) {
    Engine::GetEngine()->GetRenderer()->WaitForGPUFrame();
    mUpdateFunc(gt.DeltaTime());
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
}
void Scene::UpdateObjectCBs(const StopWatch& gt){
    auto currObjectCB = Engine::GetEngine()->GetRenderer()->GetCurrFrameResource()->ObjectCB.get();
    for (auto& e : mRenderItems){
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        auto& item = e.second;
        if (item->NumFramesDirty > 0){
            auto shaderName = item->Shader;
            auto shader = mShaders[shaderName];
            //currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            shader->UpdateObjCBData(item->ObjCBIndex, item->DataLen, item->Data);
            //Matrix4X4* m = (Matrix4X4*)item->Data;
            // Next FrameResource need to be updated too.
            item->NumFramesDirty--;
        }
    }
}

void Scene::UpdateMaterialCBs(const StopWatch& gt)
{
    auto currMaterialCB = Engine::GetEngine()->GetRenderer()->GetCurrFrameResource()->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            currMaterialCB->CopyData(mat->MatCBIndex, mat->dataLen, mat->data);
            mat->NumFramesDirty--;
        }
    }
}

void Scene::Render(const StopWatch& gt) {
	mRenderer->Render(gt);
}
void Scene::PreRender() {
    mRenderer->SetCurrentPSO(nullptr);
}
void Scene::Draw(ID3D12GraphicsCommandList* cmdList) 
{
    for (auto& pass : mPasses)
    {
        DrawPass(cmdList, pass.second.get());
    }
}
void Scene::DrawPass(ID3D12GraphicsCommandList* cmdList, Pass* pass)
{
    for (auto& namedPrimitive : mPrimitives) 
    {
        auto& primitive = namedPrimitive.second;
        auto mesh = primitive->GetMesh();
        if (!mesh->ResidentOnGPU()) 
        {
            mesh->MakeResident2GPU(mRenderer->GetDevice(), mRenderer->GetCommandList());
        }
    }

    // todo : some render items share the same pso and shader etc...
    // need to handle outside for loop
    auto renderer = Engine::GetEngine()->GetRenderer();
    for (auto& itemsTopType : mMappedRItems) 
    {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)itemsTopType.first;
        for (auto& itemsShader : itemsTopType.second) 
        {
            D3DShaderWrapper* Shader = mShaders[itemsShader.first].get();

            auto CBVHeap = Shader->GetCBVHeap();
            ID3D12DescriptorHeap* descriptorHeaps[] = { CBVHeap };
            cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            cmdList->SetGraphicsRootSignature(Shader->GetRootSignature());

            if (Shader->GetPassParamIndex() != (UINT)-1)
            {
                int passCbvIndex = pass->CBIndex;
                passCbvIndex += renderer->GetCurrFrameResIndex() * Shader->GetPassCBVMaxCount();
                auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
                passCbvHandle.Offset(passCbvIndex, renderer->GetCbvSrvUavDescriptorSize());
                cmdList->SetGraphicsRootDescriptorTable(Shader->GetPassParamIndex(), passCbvHandle);
            }
            DrawRenderItems(cmdList, itemsShader.second, Shader, topType, true);
            DrawRenderItems(cmdList, itemsShader.second, Shader, topType, false);
        }
    }
}

void Scene::DrawRenderItems
(
    ID3D12GraphicsCommandList* cmdList,
    PSOMappedVecRItem& mappedRItems, 
    D3DShaderWrapper* Shader,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topType,
    bool opaque
)
{
    auto renderer = Engine::GetEngine()->GetRenderer();
    
    auto& vecRItems = mappedRItems[opaque];
    if (vecRItems.empty()) return;
    auto pso = Engine::GetEngine()->GetPSOManager()->GetPSO
    (
        Shader->GetName(),
        renderer->GetMSAAMode(),
        opaque,
        topType,
        renderer->GetCullMode(),
        renderer->GetFillMode()
    );
    cmdList->SetPipelineState(pso);
    for (auto& renderItem : vecRItems)
    {
        renderItem->Render(Shader);
    }
}

int Scene::GetReady() {
    mRenderer->RegisterPreRenderFunc(std::bind(&Scene::PreRender, this));
    mRenderer->RegisterDrawFunc(std::bind(&Scene::Draw, this, std::placeholders::_1));

    // if no pass, add a default pass
    if (mPasses.empty() && !mShaders.empty() && !mRenderItems.empty())
    {
        AddPass(mShaders.begin()->first, "DefaultPass");
    }

    mRenderer->ResetCommandList();
    // make mesh resident to GPU memory
    for (auto& namedPrimitive : mPrimitives){
        auto& primitive = namedPrimitive.second;
        primitive->GetMesh()->MakeResident2GPU(mRenderer->GetDevice(), mRenderer->GetCommandList());
    }
    mRenderer->ExecuteCommand();
    // Wait until initialization is complete.
    mRenderer->WaitForGPU();
    for (auto& namedPrimitive : mPrimitives) {
        auto& primitive = namedPrimitive.second;
        primitive->GetMesh()->DisposeUploaders();
    }
    return 0;
}

void Scene::AddShader(const stShaderDescription& shaderDesc) {
    std::shared_ptr<D3DShaderWrapper> shader = nullptr;
    auto it = mShaders.find(shaderDesc.name);
    if (it == mShaders.end()) {
        shader = std::make_shared<D3DShaderWrapper>(shaderDesc.name);
        mShaders[shaderDesc.name] = shader;
    }
    else {
        shader = mShaders[shaderDesc.name];
    }
    shader->SetParamIndex
    (
        shaderDesc.texParamIndex, shaderDesc.objParamIndex,
        shaderDesc.matParamIndex, shaderDesc.passParamIndex
    );
    //shader->BuildCBVDescriptorHeaps(mRenderer->GetDevice(), 1);
    //shader->BuildConstantBuffers(mRenderer->GetDevice(), shaderDesc.objCBSize);
#ifdef TEX_SRV_USE_CB_HEAP
    shader->BuildFrameCBResources
    (
        shaderDesc.objCBSize, 100,
        shaderDesc.passCBSize, 3,
        shaderDesc.materialCBSize, shaderDesc.materialCBSize ? 100 : 0,
        shaderDesc.texSRVDescriptorTableSize, shaderDesc.maxTexSRVDescriptor
    );
#else
    if (shaderDesc.maxTexSRVDescriptor)
    {
        shader->BuildTexSRVHeap(shaderDesc.maxTexSRVDescriptor);
    }
    shader->BuildFrameCBResources
    (
        shaderDesc.objCBSize, 100,
        shaderDesc.passCBSize, 3,
        shaderDesc.materialCBSize, shaderDesc.materialCBSize ? 100 : 0
    );
#endif
    shader->BuildRootSignature(mRenderer->GetDevice());
    shader->BuildShadersAndInputLayout(shaderDesc);
    shader->BuildPSO
    (
        mRenderer->GetDevice(),             
        mRenderer->GetBackBufferFormat(),
        mRenderer->GetDepthStencilFormat()
    );
}
void Scene::AddPrimitive(const stRawMesh& mesh) {
    mPrimitives.emplace(mesh.name, std::make_unique<D3DPrimitive>(mesh));
}
void Scene::AddPrimitive(const stRawMesh& mesh, const std::string& shaderName) {
	mPrimitives.emplace(mesh.name, std::make_unique<D3DPrimitive>(mesh));
    auto it = mShaders.find(shaderName);
    if (it == mShaders.end()) throw std::exception("cannot find the shader");
    auto shader = it->second;
    mPrimitives[mesh.name]->SetShader(shader);
}
void Scene::PrimitiveUseShader(const std::string& primitive, const std::string& shader) {
    auto itPrimitive = mPrimitives.find(primitive);
    if (itPrimitive == mPrimitives.end()) throw std::exception("cannot find primitive");
    auto itShader = mShaders.find(shader);
    if (itShader == mShaders.end()) throw std::exception("cannot find shader");
    itPrimitive->second->SetShader(itShader->second);
}
void Scene::RenderItemChangeShader(const std::string& renderItem, const std::string& shader) {
    auto itRItem = mRenderItems.find(renderItem);
    if (itRItem == mRenderItems.end()) 
        throw std::exception("cannot find render item in function(RenderItemChangeShader)");
    auto itShader = mShaders.find(shader);
    if (itShader == mShaders.end()) 
        throw std::exception("cannot find shader in function(RenderItemChangeShader)");
    std::string oldShader = itRItem->second->Shader;
    itRItem->second->SetShader(shader);

    // first remove the render itme
    for (auto& itemsTopType : mMappedRItems) {
        auto& itemsShader = itemsTopType.second;
        auto it = itemsShader.find(oldShader);
        if (it == itemsShader.end()) continue;
        for (auto& itemsOpaqueStatus : it->second) {
            auto& vecRItems = itemsOpaqueStatus.second;
            for (auto it = vecRItems.begin(); it != vecRItems.end(); ++it) {
                if ((*it)->Name == renderItem) {
                    vecRItems.erase(it);
                    break;
                }
            }
        }
    }
    // readd render item
    auto& shaderMappedRItem = mMappedRItems[(UINT)D3DPrimitiveType(itRItem->second->PrimitiveType)];
    auto& psoMappedRItem = shaderMappedRItem[shader];
    auto& vecItems = psoMappedRItem[true];
    vecItems.push_back(itRItem->second.get());
}

void Scene::PrimitiveAddSubMesh(const std::string& name, const stRawMesh::stSubMesh& subMesh){
	auto it = mPrimitives.find(name);
	if (it == mPrimitives.end()) return;
    D3DMesh* mesh = mPrimitives[name]->GetMesh();
    mesh->AddSubMesh(subMesh);
}
void Scene::AddRenderItem
(
    const std::string&      primitiveName,
    const std::string&      shaderName,
    const stRenderItemDesc& desc
)
{
    auto itPrimitive = mPrimitives.find(primitiveName);
    if (itPrimitive == mPrimitives.end()) throw std::exception("cannot find primitive");
    auto itShader = mShaders.find(shaderName);
    if (itShader == mShaders.end()) throw std::exception("cannot find shader");
   
    D3DMesh* mesh = itPrimitive->second->GetMesh();
    D3DShaderWrapper* shader = itShader->second.get();

    auto renderItem = std::make_shared<D3DRenderItem>();
    renderItem->Name = desc.name;
    renderItem->NumFramesDirty = Engine::NumFrameResources();
    renderItem->IndexCount = desc.subMesh.indexCount;
    renderItem->StartIndexLocation = desc.subMesh.startIndexLocation;
    renderItem->BaseVertexLocation = desc.subMesh.baseVertexLocation;
    renderItem->ObjCBIndex = shader->GetFreeObjCBV();
    renderItem->Mesh = mesh;
    renderItem->Shader = shaderName;
    renderItem->PrimitiveType = FLPrimitiveTop2D3DPrimitiveTop(desc.topology);
    if (desc.data && desc.dataLen)
    {
        renderItem->Data = new char[desc.dataLen];
        renderItem->DataLen = desc.dataLen;
        memcpy(renderItem->Data, desc.data, desc.dataLen);
    }
    if (!desc.mat.empty())
    {
        auto itMat = mMaterials.find(desc.mat);
        if (itMat == mMaterials.end())
            throw std::exception("cannot find material in AddRenderItem");
        renderItem->Mat = itMat->second.get();
    }

    // All the render items are opaque(true). for now...
    auto& shaderMappedRItem = mMappedRItems[(UINT)D3DPrimitiveType(renderItem->PrimitiveType)];
    auto& psoMappedRItem = shaderMappedRItem[shaderName];
    auto& vecItems = psoMappedRItem[desc.opaque];
    vecItems.push_back(renderItem.get());
    mRenderItems.emplace(desc.name, renderItem);
}

void Scene::AddTexture(const std::string& name, const std::wstring& filename)
{
    auto tex = std::make_shared<Texture>(name, filename);
    std::unique_ptr<std::uint8_t[]> ddsdata;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
#ifdef USE_MS_DDS_LOADER
    ThrowIfFailed
    (
        DirectX::LoadDDSTextureFromFile
        (
            Engine::GetEngine()->GetRenderer()->GetDevice(),
            filename.c_str(),
            tex->resource.GetAddressOf(),
            ddsdata, subresources
        )
    );
#else
    auto renderer = Engine::GetEngine()->GetRenderer();
    renderer->ResetCommandList();
    ThrowIfFailed
    (
        DirectX::CreateDDSTextureFromFile12
        (
            renderer->GetDevice(),
            renderer->GetCommandList(),
            filename.c_str(),
            tex->resource, tex->uploadHeap
        )
    );
    renderer->ExecuteCommand();
    renderer->WaitForGPU();
    tex->uploadHeap = nullptr;
#endif
    mTextures[tex->name] = std::move(tex);
}

void Scene::AddTexture2D
(
    const std::string& name,
    const std::uint8_t* data,
    unsigned long format,
    unsigned long width,
    unsigned long height
)
{
    auto tex = std::make_shared<Texture>(name);
    auto renderer = Engine::GetEngine()->GetRenderer();
    renderer->ResetCommandList();
    tex->resource = D3DUtils::CreateDefaultTexture2D
    (
        renderer->GetDevice(),
        renderer->GetCommandList(),
        data, FireFlame::FLVertexFormat2DXGIFormat(format), width, height,
        tex->uploadHeap
    );
    renderer->ExecuteCommand();
    renderer->WaitForGPU();
    tex->uploadHeap = nullptr;
    mTextures[tex->name] = std::move(tex);
}

void Scene::AddMaterial
(
    const std::string& name,
    const std::string& shaderName,     // different shader may have different material definition
    size_t dataLen, const void* data
)
{
    auto itShader = mShaders.find(shaderName);
    if (itShader == mShaders.end()) 
        throw std::exception("cannot find shader in AddMaterial");
    auto shader = itShader->second;

    auto mat = std::make_shared<Material>(name, Engine::NumFrameResources());
    mat->MatCBIndex = shader->GetFreeMatCBV();
    //mat->DiffuseSrvHeapIndex = shader->GetFreeSRV();
    if (dataLen && data)
    {
        mat->data = new char[dataLen];
        mat->dataLen = dataLen;
        memcpy(mat->data, data, dataLen);
    }
    mMaterials.emplace(name, mat);
}

void Scene::AddMaterial
(
    const std::string& name,
    const std::string& shaderName,     // different shader may have different material definition
    const std::string& texName,
    size_t dataLen, const void* data
)
{
    auto itShader = mShaders.find(shaderName);
    if (itShader == mShaders.end())
        throw std::exception("cannot find shader in AddMaterial");
    auto shader = itShader->second;

    auto mat = std::make_shared<Material>(name, Engine::NumFrameResources());
    mat->MatCBIndex = shader->GetFreeMatCBV();
    if (!texName.empty())
    {
        auto itTex = mTextures.find(texName);
        if (itTex != mTextures.end())
        {
            mat->DiffuseSrvHeapIndex = shader->CreateTexSRV(itTex->second->resource.Get());
        }
    }
    if (dataLen && data)
    {
        mat->data = new char[dataLen];
        mat->dataLen = dataLen;
        memcpy(mat->data, data, dataLen);
    }
    mMaterials.emplace(name, mat);
}

void Scene::AddMaterial(const stMaterialDesc& matDesc)
{
    auto itShader = mShaders.find(matDesc.shaderName);
    if (itShader == mShaders.end())
        throw std::exception("cannot find shader in AddMaterial");
    auto shader = itShader->second;

    auto mat = std::make_shared<Material>(matDesc.name, Engine::NumFrameResources());
    mat->MatCBIndex = shader->GetFreeMatCBV();
    assert(matDesc.texNames.size() <= shader->GetTexSRVDescriptorTableSize());
    std::vector<ID3D12Resource*> vecRes;
    for (const auto& texName : matDesc.texNames)
    {
        auto itTex = mTextures.find(texName);
        if (itTex != mTextures.end())
        {
            vecRes.push_back(itTex->second->resource.Get());
        }
    }
    mat->DiffuseSrvHeapIndex = shader->CreateTexSRV(vecRes);
    
    if (matDesc.dataLen && matDesc.data)
    {
        mat->data = new char[matDesc.dataLen];
        mat->dataLen = matDesc.dataLen;
        memcpy(mat->data, matDesc.data, matDesc.dataLen);
    }
    mMaterials.emplace(matDesc.name, mat);
}

void Scene::AddPass(const std::string& shaderName, const std::string& passName)
{
    auto itShader = mShaders.find(shaderName);
    if (itShader == mShaders.end()) throw std::exception("cannot find shader in function call(AddPass)");
    auto shader = itShader->second;
    mPasses.emplace(passName, std::make_shared<Pass>(passName, shaderName, shader->GetFreePassCBV()));
}
void Scene::UpdateRenderItemCBData(const std::string& name, size_t size, const void* data) {
    if (mRenderItems.find(name) == mRenderItems.end()) {
        throw std::exception("UpdateRenderItemCBData cannot find renderitem");
    }
    auto& item = mRenderItems[name];
    if (item->Data == nullptr) {
        item->Data = new char[size];
        item->DataLen = size;
    }
    assert(item->DataLen == size);
    memcpy(item->Data, data, size);
    item->NumFramesDirty = Engine::NumFrameResources();

    //mRenderItems[name]->Shader->UpdateShaderCBData(mRenderItems[name]->ObjCBIndex, size, data);
}

void Scene::UpdateMaterialCBData(const std::string& name, size_t size, const void* data)
{
    auto itMat = mMaterials.find(name);
    if (itMat == mMaterials.end())
        throw std::exception("cannot find material in UpdateMaterialCBData");
    auto mat = itMat->second;
    mat->NumFramesDirty = Engine::NumFrameResources();
    if (mat->data == nullptr) {
        mat->data = new char[size];
        mat->dataLen = size;
    }
    assert(mat->dataLen == size);
    memcpy(mat->data, data, size);
}

void Scene::UpdatePassCBData(const std::string& name, size_t size, const void* data) {
    auto itPass = mPasses.find(name);
    if (itPass == mPasses.end())
        throw std::exception("cannot find pass in UpdatePassCBData function");
    auto itShader = mShaders.find(itPass->second->shaderName);
    if (itShader == mShaders.end())
        throw std::exception("cannot find shader in UpdatePassCBData function");
    auto shader = itShader->second;

    auto passCbvIndex = itPass->second->CBIndex;
    //passCbvIndex += Engine::GetEngine()->GetRenderer()->GetCurrFrameResIndex() * shader->GetPassCBVMaxCount();
    shader->UpdatePassCBData(passCbvIndex, size, data);
}
void Scene::UpdateMeshCurrVBFrameRes(const std::string& name, int index, size_t size, const void* data)
{
    auto frameRes = Engine::GetEngine()->GetRenderer()->GetCurrFrameResource();
    auto itVBRes = frameRes->VBResources.find(name);
    if (itVBRes == frameRes->VBResources.end())
        throw std::exception("cannot find VB in UpdateMeshCurrVBFrameRes");
    auto VB = itVBRes->second.get();
    VB->CopyData(index, size, data);
}
} // end namespace