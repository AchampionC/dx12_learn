
#include "ToolFunc.h"
#include "d3dx12.h"
#include "GameTime.h"
#include "D3D12App.h"

using namespace Microsoft::WRL;
using namespace DirectX;

//���嶥��ṹ��
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

//��������ĳ�������
struct ObjectConstants
{
	//��ʼ������ռ�任���ü��ռ����Identity4x4()�ǵ�λ������Ҫ����MathHelperͷ�ļ�
	XMFLOAT4X4 worldViewProj = MathHelper::Identity4x4();
};

template<typename T>
class UploadBufferResource
{
public:
	UploadBufferResource(ComPtr<ID3D12Device> d3dDevice, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);//������ǳ�������������ֱ�Ӽ��㻺���С

		if (isConstantBuffer)
			elementByteSize = CalcConstantBufferByteSize(sizeof(T));//����ǳ���������������256�ı������㻺���С
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

class D3D12InitApp : public D3D12App
{
public:
	D3D12InitApp(HINSTANCE hInstance);
	~D3D12InitApp() {};

	virtual bool Init(HINSTANCE hInstance, int nShowCmd) override;

private:
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
	virtual void Draw() override;
	virtual void Update() override;
	virtual void OnResize() override;

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSO();
	D3D12_VERTEX_BUFFER_VIEW GetVbv();
	D3D12_INDEX_BUFFER_VIEW GetIbv();

	UINT objConstSize;
	ComPtr<ID3D12DescriptorHeap> cbvHeap;
	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDesc;

	std::unique_ptr<UploadBufferResource<ObjectConstants>> objCB = nullptr;

	ComPtr<ID3DBlob> vsBytecode = nullptr;
	ComPtr<ID3DBlob> psBytecode = nullptr;

	UINT vbByteSize;
	UINT ibByteSize;

	ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> indexBufferCPU = nullptr;

	ComPtr<ID3D12Resource> vertexBufferGpu = nullptr;
	ComPtr<ID3D12Resource> indexBufferGpu = nullptr;

	ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

	ComPtr<ID3D12PipelineState> PSO = nullptr;


	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	std::array<std::uint16_t, 36> indices =
	{
		//ǰ
		0, 1, 2,
		0, 2, 3,

		//��
		4, 6, 5,
		4, 7, 6,

		//��
		4, 5, 1,
		4, 1, 0,

		//��
		3, 2, 6,
		3, 6, 7,

		//��
		1, 5, 6,
		1, 6, 2,

		//��
		4, 0, 3,
		4, 3, 7
	};

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePos;

};

UINT CalcConstantBufferByteSize(UINT byteSize)
{
	return (byteSize + 255) & ~255;
}



void D3D12InitApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x; //�����ڵ�ǰ�̵߳�ָ�������������겶��
	mLastMousePos.y = y; //�����ڵ�ǰ�̵߳�ָ�������������겶��

	SetCapture(mhMainWnd); //�����ڵ�ǰ�̵߳�ָ�������������겶��
}

void D3D12InitApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture(); // ����̧��ʱ�ͷ���겶��
}

void D3D12InitApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0) // ��������ڰ���״̬
	{
		// �������ƶ����뻻��ɻ���, 0.25Ϊ������ֵ
		float dx = XMConvertToRadians(static_cast<float>(mLastMousePos.x - x) * 0.25f);
		float dy = XMConvertToRadians(static_cast<float>(mLastMousePos.y - y) * 0.25f);
		
		// �������û���ɿ�ǰ���ۼƻ���
		mTheta += dx;
		mPhi += dy;

		// ���ƽǶ�phi�ķ�Χ(0.1, Pi - 0.1)
		mTheta = MathHelper::Clamp(mTheta, 0.1f, 3.1416f - 0.1f);
	}
	else if((btnState & MK_RBUTTON) != 0)
	{
		// �������ƶ����뻻������Ŵ�С, 0.005Ϊ������ֵ
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);
		// ����������������������ӷ�Χ�뾶
		mRadius += dx - dy;
		// ���ƿ��ӷ�Χ�뾶
		mRadius = MathHelper::Clamp(mRadius, 0.1f, 20.0f);

	}

	//����ǰ������긳ֵ������һ��������ꡱ��Ϊ��һ���������ṩ��ǰֵ
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void D3D12InitApp::Draw()
{
	ThrowIfFailed(cmdAllocator->Reset());//�ظ�ʹ�ü�¼���������ڴ�
	//ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));//���������б����ڴ�

	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), PSO.Get()));
	UINT& ref_mCurrentBackBuffer = mCurrentBackBuffer;
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(swapChainBuffer[ref_mCurrentBackBuffer].Get(),//ת����ԴΪ��̨��������Դ
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));//�ӳ��ֵ���ȾĿ��ת��

	cmdList->RSSetViewports(1, &viewPort);
	cmdList->RSSetScissorRects(1, &scissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), ref_mCurrentBackBuffer, rtvDescriptorSize);
	cmdList->ClearRenderTargetView(rtvHandle, DirectX::Colors::LightBlue, 0, nullptr);//���RT����ɫΪ���죬���Ҳ����òü�����
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->ClearDepthStencilView(dsvHandle,	//DSV���������
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,	//FLAG
		1.0f,	//Ĭ�����ֵ
		0,	//Ĭ��ģ��ֵ
		0,	//�ü���������
		nullptr);	//�ü�����ָ��

	cmdList->OMSetRenderTargets(1,//���󶨵�RTV����
		&rtvHandle,	//ָ��RTV�����ָ��
		true,	//RTV�����ڶ��ڴ�����������ŵ�
		&dsvHandle);	//ָ��DSV��ָ��

	// ����CBV��������
	ID3D12DescriptorHeap* descriHeaps[] = {cbvHeap.Get()};
	cmdList->SetDescriptorHeaps(_countof(descriHeaps), descriHeaps);
	// ���ø�ǩ��
	cmdList->SetGraphicsRootSignature(rootSignature.Get());
	// ���ö��㻺����
	cmdList->IASetVertexBuffers(0, 1, &GetVbv());
	cmdList->IASetIndexBuffer(&GetIbv());
	// ��ͼԪ�������ʹ�����ˮ��
	cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// ���ø���������
	cmdList->SetGraphicsRootDescriptorTable(0,// ����������ʼ����
	cbvHeap->GetGPUDescriptorHandleForHeapStart());

	//���ƶ���
	cmdList->DrawIndexedInstanced((UINT)indices.size(), // ÿ��ʵ��Ҫ���Ƶ�������
	1, // ʵ��������
	0, // ��ʼ����λ��
	0, // ��������ʼ������ȫ�������е�λ��
	0); // ʵ�����߼�����, ��ʱ����Ϊ0

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(swapChainBuffer[ref_mCurrentBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));//����ȾĿ�굽����
	//�������ļ�¼�ر������б�
	ThrowIfFailed(cmdList->Close());

	ID3D12CommandList* commandLists[] = { cmdList.Get() };//���������������б�����
	cmdQueue->ExecuteCommandLists(_countof(commandLists), commandLists);//������������б����������

	ThrowIfFailed(swapChain->Present(0, 0));
	ref_mCurrentBackBuffer = (ref_mCurrentBackBuffer + 1) % 2;

	FlushCmdQueue();
}


D3D12InitApp::D3D12InitApp(HINSTANCE hInstance)
:D3D12App(hInstance)
{

}

bool D3D12InitApp::Init(HINSTANCE hInstance, int nShowCmd)
{
	if (!D3D12App::Init(hInstance, nShowCmd))
		return false;

	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildPSO();

	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* cmdLists[] = { cmdList.Get() };
	cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCmdQueue();

	return true;
}

void D3D12InitApp::BuildDescriptorHeaps()
{
	objConstSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
	//����CBV��

	D3D12_DESCRIPTOR_HEAP_DESC cbHeapDesc;
	cbHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbHeapDesc.NumDescriptors = 1;
	cbHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&cbHeapDesc, IID_PPV_ARGS(&cbvHeap)));
}

void D3D12InitApp::BuildConstantBuffers()
{
	// ���岢�������ĳ���������, Ȼ��õ����׵�ַ

	// elementcount Ϊ1��1�������峣����������, isConstantBufferΪtrue(Ϊ������������
	objCB = std::make_unique<UploadBufferResource<ObjectConstants>>(d3dDevice.Get(), 1, true);
	objConstSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
	// ��ó������������׵�ַ
	D3D12_GPU_VIRTUAL_ADDRESS address;
	address = objCB->Resource()->GetGPUVirtualAddress();
	// ͨ������������Ԫ��ƫ��ֵ�������յ�Ԫ�ص�ַ
	int cbElementIndex = 0; // ����������Ԫ���±�
	address += cbElementIndex * objConstSize;
	//����CBV������
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = address;
	cbvDesc.SizeInBytes = objConstSize;
	d3dDevice->CreateConstantBufferView(&cbvDesc, cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12InitApp::BuildRootSignature()
{
	// ������������������������������������
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	// �����ɵ���CBV����ɵ���������
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //����������
	1, //����������
	0);

	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	// ��ǩ����һ�����������
	CD3DX12_ROOT_SIGNATURE_DESC rootSig(1, //������������
	slotRootParameter, // ������ָ��
	0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// �õ����Ĵ�����������һ����ǩ��, �ò�λָ��һ�������е�������������������������
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSig, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);

	if (errorBlob != nullptr)
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	ThrowIfFailed(hr);

	ThrowIfFailed(d3dDevice->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));
}

void D3D12InitApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	vsBytecode = ToolFunc::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	psBytecode = ToolFunc::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	inputLayoutDesc = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}

	};
}

void D3D12InitApp::BuildGeometry()
{
	//ʵ��������ṹ�岢���
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};



	vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &vertexBufferCPU)); // �������������ڴ�ռ�
	CopyMemory(vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &indexBufferCPU)); // �������������ڴ�ռ�
	CopyMemory(indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	vertexBufferGpu = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), vbByteSize, vertices.data(), vertexBufferUploader);
	indexBufferGpu = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), ibByteSize, indices.data(), indexBufferUploader);

}

void D3D12InitApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = {inputLayoutDesc.data(), (UINT)inputLayoutDesc.size()};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = {reinterpret_cast<BYTE*>(vsBytecode->GetBufferPointer()), vsBytecode->GetBufferSize()};
	psoDesc.PS = {reinterpret_cast<BYTE*>(psBytecode->GetBufferPointer()), psBytecode->GetBufferSize()};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;	//0xffffffff,ȫ��������û������
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;	//��һ�����޷�������
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;	//��ʹ��4XMSAA

	psoDesc.SampleDesc.Quality = 0;	////��ʹ��4XMSAA

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)));
}

void D3D12InitApp::Update()
{
	ObjectConstants objConstants;
	// �����۲����
	//float x = 5.0f;
	//float y = 5.0f;
	//float z = 5.0f;
	//float r = 5.0f;

	//x *= sinf(gt.TotalTime());

	//z = sqrt(r * r - x * x);

	//mTheta = 1.5f * XM_PI;
	//mPhi = XM_PIDIV4;
	//mRadius = 10.0f;

	float y = mRadius * cosf(mPhi);
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX v = XMMatrixLookAtLH(pos, target, up);

	// ����ͶӰ����
	XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * 3.1416f, static_cast<float>(mClientWidth) / mClientHeight, 1.0f, 1000.0f);

	// �����������
	XMMATRIX w = XMLoadFloat4x4(&mWorld);

	// �������
	XMMATRIX WVP_Matrix = v * p;
	
	// XMMATRIX ��ֵ��XMFLOAT4x4
	XMStoreFloat4x4(&objConstants.worldViewProj, XMMatrixTranspose(WVP_Matrix));

	// �����ݿ�����GPU����
	objCB->CopyData(0, objConstants);
}

void D3D12InitApp::OnResize()
{
	D3D12App::OnResize();

	//����ͶӰ����
	XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * 3.1416f, static_cast<float>(mClientWidth) / mClientHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, p);
}

D3D12_VERTEX_BUFFER_VIEW D3D12InitApp::GetVbv()
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = vertexBufferGpu->GetGPUVirtualAddress();
	vbv.SizeInBytes = vbByteSize;
	vbv.StrideInBytes = sizeof(Vertex);

	return vbv;
}

D3D12_INDEX_BUFFER_VIEW D3D12InitApp::GetIbv()
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = indexBufferGpu->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R16_UINT;
	ibv.SizeInBytes = ibByteSize;

	return ibv;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		D3D12InitApp theApp(hInstance);
		if (!theApp.Init(hInstance, nShowCmd))
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}


}

