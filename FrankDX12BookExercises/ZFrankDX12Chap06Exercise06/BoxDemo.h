#pragma once
#include "..\Common\Demo.h"
#include "BoxMesh.h"

struct BoxDemoObjectConsts : ObjectConsts {
    float TotalTime = 0;
};

class BoxDemo : public Demo {
public:
    BoxDemo(FireFlame::Engine& e);

    void Update(float time_elapsed) override;

private:
    BoxMesh mBox;
};