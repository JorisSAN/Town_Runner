#pragma once
#include "Actor.h"
#include "Camera.h"
#include "GLTFObject.h"

namespace Diligent
{

class Plane : public GLTFObject
{
public:
    Plane(const SampleInitInfo& InitInfo, BackgroundMode backGround);

    Plane(const SampleInitInfo& InitInfo, BackgroundMode backGroundP, std::string name);

    void UpdateActor(double CurrTime, double ElapsedTime) override;

    /*
    void Initialize(const SampleInitInfo& InitInfo) override;

    void RenderActor(const Camera& CameraViewProj, bool IsShadowPass) override;

    private:
    void CreatePSO() override;
    */
    
};

} // namespace Diligent