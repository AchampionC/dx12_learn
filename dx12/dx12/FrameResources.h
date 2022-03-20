#pragma once
#include "ToolFunc.h"
#include "UploadBufferResource.h"
using namespace DirectX::PackedVector;
using namespace DirectX;

struct FrameResources
{
public:
	FrameResources(ID3D12Device* device, UINT passCount, UINT objCount);
	FrameResources(const FrameResources& rhs) = delete;
	FrameResources& operator=(const FrameResources& rhs) = delete;
	~FrameResources();

	// 每帧资源都需要独立的命令分配器
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	// 每帧都需要独立的资源缓冲区(此案例仅为2个常量缓冲区)
	std::unique_ptr<UploadBufferResource<ObjectConstants>> objCB = nullptr;
	std::unique_ptr<UploadBufferResource<PassConstants>> passCB = nullptr;
	// CPU端的围栏值
	UINT64 fenceCPU = 0;
};