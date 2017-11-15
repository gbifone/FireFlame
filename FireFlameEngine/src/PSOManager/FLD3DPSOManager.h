#ifndef _FL_D3D_PSO_MANAGER_H_
#define _FL_D3D_PSO_MANAGER_H_

#include "FLPSOManager.h"
#include <vector>
#include <string>
#include <map>
#include <tuple>
#include <d3d12.h>
#include <wrl.h>

namespace FireFlame {
class D3DPSOManager : public PSOManager {
private:
    typedef std::tuple
    <
        std::string,
        UINT, 
        D3D12_PRIMITIVE_TOPOLOGY_TYPE, 
        D3D12_CULL_MODE,
        D3D12_FILL_MODE
    > PSO_TRAIT;
    typedef Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO_ComPtr;

public:
    ID3D12PipelineState*  GetPSO
    (
        const std::string& shaderName,
        UINT MSAAMode, 
        D3D12_PRIMITIVE_TOPOLOGY_TYPE ptype,
        D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
        D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID
    ) const
    {
        auto it = mPSOs.find({ shaderName, MSAAMode, ptype,cull,fill });
        if (it != mPSOs.end()) return it->second.Get();
        return nullptr;
    }
    bool AddPSO(const std::string shaderName, D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc);

private:
    std::map<PSO_TRAIT, PSO_ComPtr> mPSOs;
};
}


#endif // !_FL_D3D_PSO_MANAGER_H_
