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

	// ÿ֡��Դ����Ҫ���������������
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	// ÿ֡����Ҫ��������Դ������(�˰�����Ϊ2������������)
	std::unique_ptr<UploadBufferResource<ObjectConstants>> objCB = nullptr;
	std::unique_ptr<UploadBufferResource<PassConstants>> passCB = nullptr;
	// CPU�˵�Χ��ֵ
	UINT64 fenceCPU = 0;
};