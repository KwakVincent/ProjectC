#include "BattleRobot.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

class FBattleRobotModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FString const DllDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/ThirdParty/Onnx/Lib"));
        FPlatformProcess::AddDllDirectory(*DllDir);

        FString const DllPath = FPaths::Combine(DllDir, TEXT("onnxruntime.dll"));
        mOnnxDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
        if (mOnnxDllHandle != nullptr)
        {
            UE_LOG(LogBattleRobot, Log, TEXT("서드파티 ONNX Runtime DLL 로드 성공: %s"), *DllPath);
        }
        else
        {
            UE_LOG(LogBattleRobot, Error, TEXT("서드파티 ONNX Runtime DLL 로드 실패: %s"), *DllPath);
        }
    }

    virtual void ShutdownModule() override
    {
        if (mOnnxDllHandle != nullptr)
        {
            FPlatformProcess::FreeDllHandle(mOnnxDllHandle);
            mOnnxDllHandle = nullptr;
        }
    }

protected:
    void* mOnnxDllHandle = nullptr;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FBattleRobotModule, BattleRobot, "BattleRobot");

DEFINE_LOG_CATEGORY(LogBattleRobot)