#include "MPSApp.h"
#include <fstream>

MPSApp::MPSApp(FireFlame::Engine& e) : FLEngineApp2(e)
{
    float mTheta = 1.24f*FireFlame::MathHelper::FL_PI;
    float mPhi = 0.42f*FireFlame::MathHelper::FL_PI;
    float mRadius = 12.0f;
}

void MPSApp::Initialize()
{
    mFireKeeperLoader.load("D:\\DSIII_CHR\\c1400\\c1400.flver");

    BuildNoiseData();

    AddShaders();
    AddPSOs();
    AddMeshs();
    AddTextures();
    AddMaterials();
    AddRenderItems();
    AddPasses();
}

void MPSApp::BuildNoiseData()
{
    unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    FireFlame::Noise::Permutate(seed);

    const float step = (float)32.0f / mNoiseTexWidth;
    mNoiseData = std::make_unique<float[]>(mNoiseTexWidth*mNoiseTexWidth);

    for (size_t i = 0; i < mNoiseTexWidth; ++i)
    {
        for (size_t j = 0; j < mNoiseTexWidth; ++j)
        {
            //mNoiseData[j*pixel_width + i] = std::abs(FireFlame::Noise::Evaluate(i*step, j*step, 0.5f));
            mNoiseData[j*mNoiseTexWidth + i] = FireFlame::Noise::Evaluate(i*step, j*step, 0.5f);
            //mNoiseData[j*mNoiseTexWidth + i] += 1.0f;
            //mNoiseData[j*mNoiseTexWidth + i] /= 2.0f;
        }
    }
}

void MPSApp::Update(float time_elapsed)
{
    FLEngineApp2::Update(time_elapsed);

    MultiObjectCBData multiObjCBData;
    multiObjCBData.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    multiObjCBData.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    multiObjCBData.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    multiObjCBData.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    multiObjCBData.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    multiObjCBData.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
    mEngine.GetScene()->UpdateMultiObjCBData("default", sizeof(MultiObjectCBData), &multiObjCBData);

    // Reflect the lighting.
    using namespace DirectX;
    XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
    XMMATRIX R = XMMatrixReflect(mirrorPlane);
    for (int i = 0; i < 3; ++i)
    {
        XMFLOAT3 lightDir3(multiObjCBData.Lights[i].Direction.x, multiObjCBData.Lights[i].Direction.y, multiObjCBData.Lights[i].Direction.z);
        XMVECTOR lightDir = XMLoadFloat3(&lightDir3);
        XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
        XMStoreFloat3(&lightDir3, reflectedLightDir);
        multiObjCBData.Lights[i].Direction = { lightDir3.x,lightDir3.y,lightDir3.z };
    }
    mEngine.GetScene()->UpdateMultiObjCBData("mirror", sizeof(MultiObjectCBData), &multiObjCBData);
}

void MPSApp::AddShaders()
{
    using namespace FireFlame;
    mShaderDesc.name = "MPSShader";
    mShaderDesc.objCBSize = sizeof(ObjectConsts);
    mShaderDesc.passCBSize = sizeof(PassConstants);
    mShaderDesc.materialCBSize = sizeof(MaterialConstants);
    mShaderDesc.multiObjCBSize = sizeof(MultiObjectCBData);
    mShaderDesc.maxTexSRVDescriptor = 256;
    mShaderDesc.texSRVDescriptorTableSize = 4;
    mShaderDesc.ParamDefault2();
    mShaderDesc.AddVertexInput("POSITION", FireFlame::VERTEX_FORMAT_FLOAT3);
    mShaderDesc.AddVertexInput("NORMAL", FireFlame::VERTEX_FORMAT_FLOAT3);
    mShaderDesc.AddVertexInput("TEXCOORD", FireFlame::VERTEX_FORMAT_FLOAT2);
    auto& vs = mShaderDesc.AddShaderStage(L"Shaders\\MPSShader.hlsl", Shader_Type::VS, "VS", "vs_5_0");
    mShaderMacrosVS[""] = "";
    auto& ps = mShaderDesc.AddShaderStage(L"Shaders\\MPSShader.hlsl", Shader_Type::PS, "PS", "ps_5_0");
    mShaderMacrosPS[""] = "";

    std::vector<std::pair<std::string, std::string>> macros = { { "TERRAIN", "1" } };
    auto& psTerrain = mShaderDesc.AddShaderStage
    (
        L"Shaders\\MPSShader.hlsl",
        Shader_Type::PS, "PS", "ps_5_0",
        macros
    );
    mShaderMacrosPS["terrain"] = psTerrain.Macros2String();

    macros = { { "FOG", "1" } };
    auto& psFogged = mShaderDesc.AddShaderStage
    (
        L"Shaders\\MPSShader.hlsl",
        Shader_Type::PS, "PS", "ps_5_0",
        macros
    );
    mShaderMacrosPS["fogged"] = psFogged.Macros2String();

    macros.emplace_back("ALPHA_CLIP", "1");
    auto& psAlphaClip = mShaderDesc.AddShaderStage
    (
        L"Shaders\\MPSShader.hlsl",
        Shader_Type::PS, "PS", "ps_5_0",
        macros
    );
    mShaderMacrosPS["fog_and_alpha_clip"] = psAlphaClip.Macros2String();

    mEngine.GetScene()->AddShader(mShaderDesc);

    mEngine.GetScene()->AddMultiObjCB(mShaderDesc.name, "default");
    mEngine.GetScene()->AddMultiObjCB(mShaderDesc.name, "mirror");
}

void MPSApp::AddPSOs()
{
    using namespace FireFlame;
    auto scene = Engine::GetEngine()->GetScene();

    PSODesc desc(mShaderDesc.name, mShaderMacrosVS[""], mShaderMacrosPS[""]);
    scene->AddPSO("default", desc);

    PSODesc descTerrain(mShaderDesc.name, mShaderMacrosVS[""], mShaderMacrosPS["terrain"]);
    scene->AddPSO("terrain", descTerrain);

    //PSODesc* pDesc = new (&desc) PSODesc(mShaderDesc.name, mShaderMacrosVS[""], mShaderMacrosPS[""]);
    desc.default();
    desc.colorWriteEnable[0] = (std::uint8_t)0/*COLOR_WRITE_ENABLE::COLOR_WRITE_ENABLE_ALL*/;
    desc.depthWriteMask = 0;

    desc.stencilEnable = true;
    desc.stencilFailOp = STENCIL_OP::KEEP;
    desc.stencilDepthFailOp = STENCIL_OP::KEEP;
    desc.stencilPassOp = STENCIL_OP::REPLACE;
    desc.stencilFunc = COMPARISON_FUNC::ALWAYS;

    scene->AddPSO("markStencilMirrors", desc);

    desc.default();
    desc.stencilEnable = true;
    desc.stencilFailOp = STENCIL_OP::KEEP;
    desc.stencilDepthFailOp = STENCIL_OP::KEEP;
    desc.stencilPassOp = STENCIL_OP::KEEP;
    desc.stencilFunc = COMPARISON_FUNC::EQUAL;
    desc.frontCounterClockwise = true;
    scene->AddPSO("drawStencilReflections", desc);

    // transparent
    desc.default();
    desc.opaque = false;
    scene->AddPSO("transparent", desc);

    desc.stencilEnable = true;
    desc.stencilPassOp = STENCIL_OP::INCR;
    desc.stencilFunc = COMPARISON_FUNC::EQUAL;
    scene->AddPSO("shadow", desc);
}

void MPSApp::AddMeshs()
{
    AddRoomMesh();
    AddSkullMesh();
    AddFireKeeperMesh();
}

void MPSApp::AddRoomMesh()
{
    // Create and specify geometry.  For this sample we draw a floor
    // and a wall with a mirror on it.  We put the floor, wall, and
    // mirror geometry in one vertex buffer.
    //
    //   |-----------------|
    //   |                 |
    //   |----|-------|----|
    //   |Wall|Mirror |Wall|
    //   |    |       |    |
    //   /-----------------/
    //  /   Floor         /
    // /-----------------/
    using namespace FireFlame;
    std::array<FLVertexNormalTex, 20> vertices =
    {
        // Floor: Observe we tile texture coordinates.
        FLVertexNormalTex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f), // 0 
        FLVertexNormalTex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        FLVertexNormalTex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f),
        FLVertexNormalTex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f),

        // Wall: Observe we tile texture coordinates, and that we
        // leave a gap in the middle for the mirror.
        FLVertexNormalTex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
        FLVertexNormalTex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        FLVertexNormalTex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
        FLVertexNormalTex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

        FLVertexNormalTex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
        FLVertexNormalTex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        FLVertexNormalTex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
        FLVertexNormalTex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

        FLVertexNormalTex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
        FLVertexNormalTex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        FLVertexNormalTex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
        FLVertexNormalTex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

        // Mirror
        FLVertexNormalTex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
        FLVertexNormalTex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        FLVertexNormalTex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        FLVertexNormalTex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
    };

    std::array<std::int16_t, 30> indices =
    {
        // Floor
        0, 1, 2,
        0, 2, 3,

        // Walls
        4, 5, 6,
        4, 6, 7,

        8, 9, 10,
        8, 10, 11,

        12, 13, 14,
        12, 14, 15,

        // Mirror
        16, 17, 18,
        16, 18, 19
    };

    mMeshDesc["room"].name = "room";

    mMeshDesc["room"].indexCount = (unsigned int)indices.size();
    mMeshDesc["room"].indexFormat = Index_Format::UINT16;
    mMeshDesc["room"].indices = indices.data();

    mMeshDesc["room"].vertexDataCount.push_back((unsigned int)vertices.size());
    mMeshDesc["room"].vertexDataSize.push_back(sizeof(FLVertexNormalTex));
    mMeshDesc["room"].vertexData.push_back(vertices.data());

    // sub meshes
    mMeshDesc["room"].subMeshs.emplace_back("floor", 6);
    mMeshDesc["room"].subMeshs.emplace_back("wall", 18, 6);
    mMeshDesc["room"].subMeshs.emplace_back("mirror", 6, 24);
    mEngine.GetScene()->AddPrimitive(mMeshDesc["room"]);
}

void MPSApp::AddSkullMesh()
{
    using namespace FireFlame;

    std::ifstream fin("../../Resources/geometry/skull.txt");

    if (!fin)
    {
        MessageBox(0, L"../../Resources/geometry/skull.txt not found.", 0, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    std::vector<FLVertexNormalTex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        // Model does not have texture coordinates, so just zero them out.
        vertices[i].Tex = { 0.0f, 0.0f };
    }

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::uint32_t> indices(3 * tcount);
    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    auto& meshDesc = mMeshDesc["skull"];
    meshDesc.name = "skull";

    meshDesc.indexCount = (unsigned int)indices.size();
    meshDesc.indexFormat = Index_Format::UINT32;
    meshDesc.indices = indices.data();

    meshDesc.vertexDataCount.push_back((unsigned int)vertices.size());
    meshDesc.vertexDataSize.push_back(sizeof(FLVertexNormalTex));
    meshDesc.vertexData.push_back(vertices.data());

    // sub meshes
    meshDesc.subMeshs.emplace_back("all", (unsigned int)indices.size());
    mEngine.GetScene()->AddPrimitive(meshDesc);
}

void MPSApp::AddFireKeeperMesh()
{
    AddFireKeeperPart(0, true);
    AddFireKeeperPart(1, true);
    AddFireKeeperPart(2, true);
    AddFireKeeperPart(3, true);
    AddFireKeeperPart(4, true);
    AddFireKeeperPart(5, true);
    AddFireKeeperPart(6, true);
    AddFireKeeperPart(7, true);
    AddFireKeeperPart(8, true);
    AddFireKeeperPart(9, true);
    AddFireKeeperPart(10, true);
    AddFireKeeperPart(11, true);
    AddFireKeeperPart(12, true);
    AddFireKeeperPart(13, true);
    AddFireKeeperPart(14, true);
    AddFireKeeperPart(15, true);
    AddFireKeeperPart(16, true);
    AddFireKeeperPart(17, false);
}

void MPSApp::AddFireKeeperPart(size_t part, bool reverseNormal)
{
    using namespace FireFlame;

    // Mesh
    std::vector<FireFlame::FLVertexNormalTex> vertices;
    auto& rawUVs = mFireKeeperLoader.get_uvs();
    auto& rawVertices = mFireKeeperLoader.get_vertices();
    auto& rawIndices = mFireKeeperLoader.get_indices();

    std::vector<std::uint32_t> indices;
    for (const auto& index : rawIndices[part])
    {
        indices.push_back(index);
    }

    vertices.reserve(rawVertices[part].size());
    for (size_t i = 0; i < rawVertices[part].size(); i++)
    {
        vertices.emplace_back
        (
            rawVertices[part][i].x, rawVertices[part][i].y, rawVertices[part][i].z,
            0.f, 0.f, 0.f,
            rawUVs[part][i].u, -rawUVs[part][i].v
        );
    }

    // begin normals
    for (size_t i = 0; i < indices.size() / 3; i++)
    {
        auto& v0 = vertices[indices[i * 3 + 0]];
        auto& v1 = vertices[indices[i * 3 + 1]];
        auto& v2 = vertices[indices[i * 3 + 2]];
        auto e0 = v1.Pos - v0.Pos;
        auto e1 = v2.Pos - v0.Pos;
        auto normal = FireFlame::Vector3Cross(reverseNormal ? e1 : e0, reverseNormal ? e0 : e1);

        v0.Normal += normal;
        v1.Normal += normal;
        v2.Normal += normal;
    }
    for (auto& vertex : vertices)
    {
        vertex.Normal.Normalize();
    }
    // end normals

    std::string meshName = "fk_model_" + std::to_string(part);
    auto& mesh = mMeshDesc[meshName];
    mesh.name = meshName;
    mesh.primitiveTopology = Primitive_Topology::TriangleList;
    mesh.indexCount = (unsigned int)indices.size();
    mesh.indexFormat = Index_Format::UINT32;
    mesh.indices = indices.data();

    mesh.vertexDataCount.push_back((unsigned int)vertices.size());
    mesh.vertexDataSize.push_back(sizeof(FLVertexNormalTex));
    mesh.vertexData.push_back(vertices.data());

    // sub meshes
    mesh.subMeshs.emplace_back("All", (UINT)indices.size());
    mEngine.GetScene()->AddPrimitive(mesh);
}

void MPSApp::AddTextures()
{
    mEngine.GetScene()->AddTexture
    (
        "bricksTex",
        L"..\\..\\Resources\\Textures\\bricks3.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "checkboardTex",
        L"..\\..\\Resources\\Textures\\checkboard.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "iceTex",
        L"..\\..\\Resources\\Textures\\ice.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "white1x1Tex",
        L"..\\..\\Resources\\Textures\\white1x1.dds"
    );

    mEngine.GetScene()->AddTexture2D
    (
        "heightMap", (const std::uint8_t*)mNoiseData.get(),
        FireFlame::VERTEX_FORMAT_FLOAT1,
        mNoiseTexWidth, mNoiseTexWidth
    );
    mEngine.GetScene()->AddTexture
    (
        "darkdirtTex",
        L"..\\..\\Resources\\terrain\\darkdirt.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "lightdirtTex",
        L"..\\..\\Resources\\terrain\\lightdirt.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "snowTex",
        L"..\\..\\Resources\\terrain\\snow.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "grassTex",
        L"..\\..\\Resources\\terrain\\grass.dds"
    );

    // FireKeeper Textures
    AddFireKeeperTextures();
}

void MPSApp::AddFireKeeperTextures()
{

    mEngine.GetScene()->AddTexture
    (
        "arm",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_arm_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "arm_r",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_arm_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "body",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_body_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "body_r",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_body_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "hair",
        //L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_hair_a.dds"
        L"D:\\DSIII_CHR\\c1400\\c1400\\c1400_hair_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "hair_r",
        //L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_hair_r.dds"
        L"D:\\DSIII_CHR\\c1400\\c1400\\c1400_hair_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "head",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_head_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "head_r",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_head_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "leg",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_leg_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "leg_r",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_leg_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "manteau",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_manteau_a.dds"
        //L"D:\\DSIII_CHR\\c1400\\c1400\\c1400_manteau_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "manteau_r",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_manteau_r.dds"
        //L"D:\\DSIII_CHR\\c1400\\c1400\\c1400_manteau_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "skin",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_skin_a.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "skin_r",
        L"..\\..\\Resources\\Textures\\FireKeeper\\c1400_skin_r.dds"
    );
    mEngine.GetScene()->AddTexture
    (
        "unknown",
        L"D:\\DSIII_CHR\\c1400\\c1400\\c1400_v.dds"
    );
    mTexMapFK[0] = "hair";
    mTexMapFK[1] = "body";
    mTexMapFK[2] = "head"; // ͷ��
    mTexMapFK[3] = "skin";
    mTexMapFK[4] = "skin";
    mTexMapFK[5] = "body";
    mTexMapFK[6] = "body";
    mTexMapFK[7] = "body";
    mTexMapFK[8] = "body";
    mTexMapFK[9] = "body";
    mTexMapFK[10] = "skin";
    mTexMapFK[11] = "leg";
    mTexMapFK[12] = "body";
    mTexMapFK[13] = "manteau";
    mTexMapFK[14] = "manteau";
    mTexMapFK[15] = "body";
    mTexMapFK[16] = "arm";
    mTexMapFK[17] = "hair";
}

void MPSApp::AddMaterials()
{
    auto& bricks = mMaterials["bricks"];
    bricks.Name = "bricks";
    bricks.DiffuseAlbedo = FireFlame::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    bricks.FresnelR0 = FireFlame::Vector3f(0.05f, 0.05f, 0.05f);
    bricks.Roughness = 0.25f;
    mEngine.GetScene()->AddMaterial
    (
        bricks.Name,
        mShaderDesc.name, "bricksTex",
        sizeof(MaterialConstants), &bricks
    );

    auto& checkertile = mMaterials["checkertile"];
    checkertile.Name = "checkertile";
    checkertile.DiffuseAlbedo = FireFlame::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    checkertile.FresnelR0 = FireFlame::Vector3f(0.07f, 0.07f, 0.07f);
    checkertile.Roughness = 0.3f;
    mEngine.GetScene()->AddMaterial
    (
    {
        checkertile.Name,
        mShaderDesc.name, { "heightMap", "darkdirtTex", "lightdirtTex", "snowTex" },
        sizeof(MaterialConstants), &checkertile
    }
    );

    auto& icemirror = mMaterials["icemirror"];
    icemirror.Name = "icemirror";
    icemirror.DiffuseAlbedo = FireFlame::Vector4f(1.0f, 1.0f, 1.0f, 0.3f);
    icemirror.FresnelR0 = FireFlame::Vector3f(0.1f, 0.1f, 0.1f);
    icemirror.Roughness = 0.5f;
    mEngine.GetScene()->AddMaterial
    (
        icemirror.Name,
        mShaderDesc.name, "iceTex",
        sizeof(MaterialConstants), &icemirror
    );

    auto& skullMat = mMaterials["skullMat"];
    skullMat.Name = "skullMat";
    skullMat.DiffuseAlbedo = FireFlame::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    skullMat.FresnelR0 = FireFlame::Vector3f(0.05f, 0.05f, 0.05f);
    skullMat.Roughness = 0.3f;
    mEngine.GetScene()->AddMaterial
    (
        skullMat.Name,
        mShaderDesc.name, "white1x1Tex",
        sizeof(MaterialConstants), &skullMat
    );

    auto& shadowMat = mMaterials["shadowMat"];
    shadowMat.Name = "shadowMat";
    shadowMat.DiffuseAlbedo = FireFlame::Vector4f(0.0f, 0.0f, 0.0f, 0.5f);
    shadowMat.FresnelR0 = FireFlame::Vector3f(0.001f, 0.001f, 0.001f);
    shadowMat.Roughness = 0.0f;
    mEngine.GetScene()->AddMaterial
    (
        shadowMat.Name,
        mShaderDesc.name, "white1x1Tex",
        sizeof(MaterialConstants), &shadowMat
    );

    AddFireKeeperMaterials();
}

void MPSApp::AddFireKeeperMaterials()
{
    auto par_count = mFireKeeperLoader.get_part_count();
    for (size_t part = 0; part < par_count; part++)
    {
        std::string matName = "fk_mat_" + std::to_string(part);
        auto& material = mMaterials[matName];
        material.Name = matName;
        if (part == 2)
        {
            material.DiffuseAlbedo = { 4.f, 4.f, 4.f, 1.0f };
            material.FresnelR0 = { 1.0f,1.0f,1.0f };
            material.Roughness = 0.0f;
            material.UseSpecularMap = 0;
        }
        else if (part == 0 || part == 17)
        {
            material.DiffuseAlbedo = { 1.3f, 1.3f, 1.3f, 1.0f };
            material.FresnelR0 = { 0.6f,0.6f,0.6f };
            material.Roughness = 0.0f;
            material.UseSpecularMap = 1;
        }
        else
        {
            material.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
            material.FresnelR0 = { 0.05f,0.05f,0.05f };
            material.Roughness = 0.6f;
            material.UseSpecularMap = 1;
        }

        std::string specularTex;
        if (mTexMapFK[part] == "")
        {
            material.UseTexture = 2;
        }
        else
        {
            material.UseTexture = 1;
            specularTex = mTexMapFK[part] + "_r";
        }

        mEngine.GetScene()->AddMaterial
        (
        {
            material.Name,
            mShaderDesc.name,{ mTexMapFK[part],specularTex },
            sizeof(MaterialConstants), &material
        }
        );
    }
}

void MPSApp::AddRenderItems()
{
    AddRenderItemFloor();
    AddRenderItemWall();
    AddRenderItemSkull();
    AddRenderItemFireKeeper();
    AddRenderItemMirror();
}

void MPSApp::AddRenderItemFloor()
{
    using namespace DirectX;

    FireFlame::stRenderItemDesc RItem("floor", mMeshDesc["room"].subMeshs[0]);
    RItem.mat = "checkertile";
    XMFLOAT4X4 trans[2];
    XMStoreFloat4x4
    (
        &trans[0],
        //XMMatrixIdentity()
        XMMatrixTranspose(XMMatrixScaling(10.f, 10.f, 10.f) * XMMatrixTranslation(0.f, 0.f, 30.f))
    );
    XMStoreFloat4x4
    (
        &trans[1],
        //XMMatrixIdentity()
        XMMatrixTranspose(XMMatrixScaling(25.f, 25.f, 1.f))
    );
    RItem.dataLen = sizeof(XMFLOAT4X4)*_countof(trans);
    RItem.data = &trans[0];
    mRenderItems[RItem.name] = RItem;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["room"].name,
        mShaderDesc.name,
        "terrain",
        "default",
        0,
        RItem
    );
}

void MPSApp::AddRenderItemWall()
{
    using namespace DirectX;

    FireFlame::stRenderItemDesc RItem("wall", mMeshDesc["room"].subMeshs[1]);
    RItem.mat = "bricks";
    XMFLOAT4X4 trans[2];
    XMStoreFloat4x4
    (
        &trans[0],
        XMMatrixIdentity()
    );
    XMStoreFloat4x4
    (
        &trans[1],
        XMMatrixIdentity()
    );
    RItem.dataLen = sizeof(XMFLOAT4X4)*_countof(trans);
    RItem.data = &trans[0];
    mRenderItems[RItem.name] = RItem;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["room"].name,
        mShaderDesc.name,
        "default",
        "default",
        0,
        RItem
    );
}

void MPSApp::AddRenderItemSkull()
{
    using namespace DirectX;

    FireFlame::stRenderItemDesc RItem("skull", mMeshDesc["skull"].subMeshs[0]);
    RItem.mat = "skullMat";
    XMFLOAT4X4 trans[2];
    XMStoreFloat4x4
    (
        &trans[0],
        XMMatrixIdentity()
    );
    XMStoreFloat4x4
    (
        &trans[1],
        XMMatrixIdentity()
    );
    RItem.dataLen = sizeof(XMFLOAT4X4)*_countof(trans);
    RItem.data = &trans[0];
    mRenderItems[RItem.name] = RItem;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["skull"].name,
        mShaderDesc.name,
        "default",
        "default",
        0,
        RItem
    );

    RItem.name = "skullReflec";
    RItem.stencilRef = 1;
    mRenderItems[RItem.name] = RItem;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["skull"].name,
        mShaderDesc.name,
        "drawStencilReflections",
        "mirror",
        2,
        RItem
    );

    RItem.name = "skullShadow";
    RItem.stencilRef = 0;
    RItem.mat = "shadowMat";
    mRenderItems[RItem.name] = RItem;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["skull"].name,
        mShaderDesc.name,
        "shadow",
        "default",
        4,
        RItem
    );
}

void MPSApp::AddRenderItemFireKeeper()
{
    using namespace DirectX;
    auto parts = mFireKeeperLoader.get_part_count();
    for (size_t part = 0; part < parts; part++)
    {
        std::string matName = "fk_mat_" + std::to_string(part);
        std::string meshName = "fk_model_" + std::to_string(part);

        // add render item
        FireFlame::stRenderItemDesc RItem(meshName, mMeshDesc[meshName].subMeshs[0]);
        RItem.mat = matName;
        XMFLOAT4X4 trans[2];
        XMStoreFloat4x4
        (
            &trans[0],
            XMMatrixTranspose
            (
                XMMatrixTranslation(0.0f, 0.0f, 0.0f)*
                XMMatrixScaling(0.02f, 0.02f, 0.02f)
            )
        );
        XMStoreFloat4x4
        (
            &trans[1],
            XMMatrixIdentity()
        );
        RItem.dataLen = sizeof(XMFLOAT4X4)*_countof(trans);
        RItem.data = &trans[0];
        mRenderItems[RItem.name] = RItem;
        mEngine.GetScene()->AddRenderItem
        (
            mMeshDesc[meshName].name,
            mShaderDesc.name,
            "default",
            "default",
            0,
            RItem
        );
    }
}

void MPSApp::AddRenderItemMirror()
{
    using namespace DirectX;

    FireFlame::stRenderItemDesc RItem("mirror", mMeshDesc["room"].subMeshs[2]);
    RItem.mat = "icemirror";
    XMFLOAT4X4 trans[2];
    XMStoreFloat4x4
    (
        &trans[0],
        XMMatrixIdentity()
    );
    XMStoreFloat4x4
    (
        &trans[1],
        XMMatrixIdentity()
    );
    RItem.dataLen = sizeof(XMFLOAT4X4)*_countof(trans);
    RItem.data = &trans[0];
    RItem.stencilRef = 1;
    mRenderItems[RItem.name] = RItem;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["room"].name,
        mShaderDesc.name,
        "markStencilMirrors",
        "default",
        1,
        RItem
    );

    RItem.name = "mirror_blend";
    RItem.opaque = false;
    mEngine.GetScene()->AddRenderItem
    (
        mMeshDesc["room"].name,
        mShaderDesc.name,
        "transparent",
        //"default",
        "default",
        3,
        RItem
    );
}

void MPSApp::AddPasses()
{
    mPasses.push_back("DefaultPass");
    mEngine.GetScene()->AddPass(mShaderDesc.name, mPasses[0]);
}

void MPSApp::OnKeyboardInput(float time_elapsed)
{
    //
    // Allow user to move skull.
    //

    const float dt = mEngine.DeltaTime();

    if (GetAsyncKeyState('A') & 0x8000)
        mSkullTranslation.x -= 1.0f*dt;

    if (GetAsyncKeyState('D') & 0x8000)
        mSkullTranslation.x += 1.0f*dt;

    if (GetAsyncKeyState('W') & 0x8000)
        mSkullTranslation.y += 1.0f*dt;

    if (GetAsyncKeyState('S') & 0x8000)
        mSkullTranslation.y -= 1.0f*dt;

    MoveSkull();
}

void MPSApp::MoveSkull()
{
    // Don't let user move below ground plane.
    mSkullTranslation.y = (std::max)(mSkullTranslation.y, 0.0f);

    // Update the new world matrix.
    DirectX::XMMATRIX skullRotate = DirectX::XMMatrixRotationY(0.5f*FireFlame::MathHelper::FL_PI);
    DirectX::XMMATRIX skullScale = DirectX::XMMatrixScaling(0.45f, 0.45f, 0.45f);
    DirectX::XMMATRIX skullOffset = DirectX::XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
    DirectX::XMMATRIX skullWorld = skullRotate*skullScale*skullOffset;
    DirectX::XMFLOAT4X4 transform[2];
    DirectX::XMStoreFloat4x4(&transform[0], DirectX::XMMatrixTranspose(skullWorld));
    DirectX::XMStoreFloat4x4(&transform[1], DirectX::XMMatrixIdentity());
    mEngine.GetScene()->UpdateRenderItemCBData("skull", sizeof(transform), &transform[0]);

    // Update reflection world matrix.
    DirectX::XMVECTOR mirrorPlane = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
    DirectX::XMMATRIX R = DirectX::XMMatrixReflect(mirrorPlane);
    DirectX::XMStoreFloat4x4(&transform[0], DirectX::XMMatrixTranspose(skullWorld*R));
    DirectX::XMStoreFloat4x4(&transform[1], DirectX::XMMatrixIdentity());
    mEngine.GetScene()->UpdateRenderItemCBData("skullReflec", sizeof(transform), &transform[0]);

    // Update shadow world matrix.
    using namespace DirectX;
    DirectX::XMVECTOR shadowPlane = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
    DirectX::XMFLOAT3 lightDir = {};
    memcpy(&lightDir, &mMainPassCB.Lights[0].Direction, sizeof(DirectX::XMFLOAT3));
    DirectX::XMVECTOR toMainLight = -DirectX::XMLoadFloat3(&lightDir);
    DirectX::XMMATRIX S = DirectX::XMMatrixShadow(shadowPlane, toMainLight);
    DirectX::XMMATRIX shadowOffsetY = DirectX::XMMatrixTranslation(0.0f, 0.001f, 0.0f);
    //XMStoreFloat4x4(&mShadowedSkullRitem->World, skullWorld * S * shadowOffsetY);
    DirectX::XMStoreFloat4x4(&transform[0], DirectX::XMMatrixTranspose(skullWorld * S * shadowOffsetY));
    DirectX::XMStoreFloat4x4(&transform[1], DirectX::XMMatrixIdentity());
    mEngine.GetScene()->UpdateRenderItemCBData("skullShadow", sizeof(transform), &transform[0]);
}