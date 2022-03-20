#pragma once
#include "d3dx12.h"
#include "DirectXPackedVector.h"
#include <DirectXMath.h>
#include "MathHelper.h"
#include "ToolFunc.h"
using namespace Microsoft::WRL;
using namespace DirectX;


template<typename T>
class UploadBufferResource
{
public:
	UploadBufferResource(ComPtr<ID3D12Device> d3dDevice, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);//������ǳ�������������ֱ�Ӽ��㻺���С

		if (isConstantBuffer)
			elementByteSize = ToolFunc::CalcConstantBufferByteSize(sizeof(T));//����ǳ���������������256�ı������㻺���С
		//�����ϴ��Ѻ���Դ
		ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));
		//������������Դ��ָ��
		ThrowIfFailed(uploadBuffer->Map(0,//����Դ���������ڻ�������˵����������Դ�����Լ�
			nullptr,//��������Դ����ӳ��
			reinterpret_cast<void**>(&mappedData)));//���ش�ӳ����Դ���ݵ�Ŀ���ڴ��	
	}

	~UploadBufferResource()
	{
		if (uploadBuffer != nullptr)
			uploadBuffer->Unmap(0, nullptr);//ȡ��ӳ��

		mappedData = nullptr;//�ͷ�ӳ���ڴ�
	}

	UploadBufferResource(const UploadBufferResource& rhs) = delete;
	UploadBufferResource& operator = (const UploadBufferResource& rhs) = delete;

	ID3D12Resource* Resource() const
	{
		return uploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mappedData[elementIndex * elementByteSize], &data, sizeof(T));
	}
private:
	UINT elementByteSize;
	ComPtr<ID3D12Resource> uploadBuffer;
	BYTE* mappedData = nullptr;
	bool mIsConstantBuffer = false;

};

//��������ĳ�������
struct ObjectConstants
{
	//��ʼ������ռ�任���ü��ռ����Identity4x4()�ǵ�λ������Ҫ����MathHelperͷ�ļ�
	XMFLOAT4X4 world = MathHelper::Identity4x4();
};

struct PassConstants
{
	XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
};
