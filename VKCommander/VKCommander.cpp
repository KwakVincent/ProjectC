#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "VoicePipeline.h"
#include <iostream>
#include <vector>
#include <string>

FVoicePipeline* gPipeline = nullptr;
std::vector<float> gAccumulatedInput;

void AudioDataCallback(ma_device* pDevice, void* pOutput, void const* pInput, ma_uint32 FrameCount)
{
    if (!pInput || !gPipeline)
    {
        return;
    }

    float const* FloatInput = static_cast<float const*>(pInput);
    gAccumulatedInput.insert(gAccumulatedInput.end(), FloatInput, FloatInput + FrameCount);

    while (gAccumulatedInput.size() >= FSileroVAD::VAD_CHUNK_SIZE)
    {
        gPipeline->ProcessAudioChunk(gAccumulatedInput.data());
        gAccumulatedInput.erase(gAccumulatedInput.begin(), gAccumulatedInput.begin() + FSileroVAD::VAD_CHUNK_SIZE);
    }
}

int main()
{
    gPipeline = new FVoicePipeline();

    ma_context Context;
    if (ma_context_init(nullptr, 0, nullptr, &Context) != MA_SUCCESS)
    {
        std::cerr << "오디오 컨텍스트 초기화 실패" << std::endl;
        delete gPipeline;
        return -1;
    }

    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 CaptureCount = 0;
    if (ma_context_get_devices(&Context, nullptr, nullptr, &pCaptureInfos, &CaptureCount) != MA_SUCCESS || CaptureCount == 0)
    {
        std::cerr << "연결된 마이크 장치를 찾을 수 없습니다." << std::endl;
        ma_context_uninit(&Context);
        delete gPipeline;
        return -1;
    }

    std::cout << "\n=== 사용 가능한 마이크 장치 목록 (" << CaptureCount << "개) ===" << std::endl;
    ma_uint32 SelectedIndex = 0;
    for (ma_uint32 i = 0; i < CaptureCount; ++i)
    {
        std::cout << " [" << i << "] " << pCaptureInfos[i].name;
        if (pCaptureInfos[i].isDefault)
        {
            std::cout << " (기본 장치)";
            SelectedIndex = i;
        }
        std::cout << std::endl;
    }

    if (CaptureCount > 1)
    {
        std::cout << "사용할 마이크 번호를 입력하세요 (기본 장치 사용: 엔터): ";
        std::string InputLine;
        std::getline(std::cin, InputLine);
        if (!InputLine.empty())
        {
            try
            {
                int const InputIndex = std::stoi(InputLine);
                if (InputIndex >= 0 && InputIndex < static_cast<int>(CaptureCount))
                {
                    SelectedIndex = static_cast<ma_uint32>(InputIndex);
                }
            }
            catch (...)
            {
            }
        }
    }

    std::cout << "선택된 장치: [" << SelectedIndex << "] " << pCaptureInfos[SelectedIndex].name << std::endl;

    ma_device_config DeviceConfig = ma_device_config_init(ma_device_type_capture);
    DeviceConfig.capture.pDeviceID = &pCaptureInfos[SelectedIndex].id;
    DeviceConfig.capture.format = ma_format_f32;
    DeviceConfig.capture.channels = 1;
    DeviceConfig.sampleRate = FSileroVAD::SAMPLE_RATE;
    DeviceConfig.dataCallback = AudioDataCallback;

    ma_device Device;
    if (ma_device_init(&Context, &DeviceConfig, &Device) != MA_SUCCESS)
    {
        std::cerr << "마이크 디바이스 초기화 실패" << std::endl;
        ma_context_uninit(&Context);
        delete gPipeline;
        return -1;
    }

    if (ma_device_start(&Device) != MA_SUCCESS)
    {
        std::cerr << "마이크 캡처 시작 실패" << std::endl;
        ma_device_uninit(&Device);
        ma_context_uninit(&Context);
        delete gPipeline;
        return -1;
    }

    std::cout << "[오디오 장치 정보] 샘플레이트: " << Device.sampleRate << "Hz, 채널: " << Device.capture.channels << std::endl;

    std::cout << "\n=== 마이크 음성 인식 준비 완료 ===" << std::endl;
    std::cout << "마이크에 대고 말을 시작하세요 (종료하려면 엔터 키 입력)..." << std::endl;

    std::cin.get();

    ma_device_uninit(&Device);
    ma_context_uninit(&Context);
    delete gPipeline;
    return 0;
}
