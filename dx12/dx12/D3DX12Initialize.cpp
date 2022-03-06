
#include "ToolFunc.h"
#include "d3dx12.h"
#include "GameTime.h"
#include "D3D12App.h"

using namespace Microsoft::WRL;
using namespace DirectX;

//定义顶点结构体
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

//单个物体的常量数据
struct ObjectConstants
{
	//初始化物体空间变换到裁剪空间矩阵，Identity4x4()是单位矩阵，需要包含MathHelper头文件
	XMFLOAT4X4 worldViewProj = MathHelper::Identity4x4();
};

template<typename T>
class UploadBufferResource
{
public:
	UploadBufferResource(ComPtr<ID3D12Device> d3dDevice, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);//如果不是常量缓冲区，则直接计算缓存大小

		if (isConstantBuffer)
			elementByteSize = CalcConstantBufferByteSize(sizeof(T));//如果是常量缓冲区，则以256的倍数计算缓存大小
		//创建上传堆和资源
		ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));
		//返回欲更新资源的指针
		ThrowIfFailed(uploadBuffer->Map(0,//子资源索引，对于缓冲区来说，他的子资源就是自己
			nullptr,//对整个资源进行映射
			reinterpret_cast<void**>(&mappedData)));//返回待映射资源数据的目标内存块	
	}

	~UploadBufferResource()
	{
		if (uploadBuffer != nullptr)
			uploadBuffer->Unmap(0, nullptr);//取消映射

		mappedData = nullptr;//释放映射内存
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
		//前
		0, 1, 2,
		0, 2, 3,

		//后
		4, 6, 5,
		4, 7, 6,

		//左
		4, 5, 1,
		4, 1, 0,

		//右
		3, 2, 6,
		3, 6, 7,

		//上
		1, 5, 6,
		1, 6, 2,

		//下
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
	mLastMousePos.x = x; //在属于当前线程的指定窗口里，设置鼠标捕获
	mLastMousePos.y = y; //在属于当前线程的指定窗口里，设置鼠标捕获

	SetCapture(mhMainWnd); //在属于当前线程的指定窗口里，设置鼠标捕获
}

void D3D12InitApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture(); // 按键抬起时释放鼠标捕获
}

void D3D12InitApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0) // 如果按键在按下状态
	{
		// 将鼠标的移动距离换算成弧度, 0.25为调节阈值
		float dx = XMConvertToRadians(static_cast<float>(mLastMousePos.x - x) * 0.25f);
		float dy = XMConvertToRadians(static_cast<float>(mLastMousePos.y - y) * 0.25f);
		
		// 计算鼠标没有松开前的累计弧度
		mTheta += dx;
		mPhi += dy;

		// 限制角度phi的范围(0.1, Pi - 0.1)
		mTheta = MathHelper::Clamp(mTheta, 0.1f, 3.1416f - 0.1f);
	}
	else if((btnState & MK_RBUTTON) != 0)
	{
		// 将鼠标的移动距离换算成缩放大小, 0.005为调节阈值
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);
		// 根据鼠标输入更新摄像机可视范围半径
		mRadius += dx - dy;
		// 限制可视范围半径
		mRadius = MathHelper::Clamp(mRadius, 0.1f, 20.0f);

	}

	//将当前鼠标坐标赋值给“上一次鼠标坐标”，为下一次鼠标操作提供先前值
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void D3D12InitApp::Draw()
{
	ThrowIfFailed(cmdAllocator->Reset());//重复使用记录命令的相关内存
	//ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));//复用命令列表及其内存

	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), PSO.Get()));
	UINT& ref_mCurrentBackBuffer = mCurrentBackBuffer;
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(swapChainBuffer[ref_mCurrentBackBuffer].Get(),//转换资源为后台缓冲区资源
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));//从呈现到渲染目标转换

	cmdList->RSSetViewports(1, &viewPort);
	cmdList->RSSetScissorRects(1, &scissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), ref_mCurrentBackBuffer, rtvDescriptorSize);
	cmdList->ClearRenderTargetView(rtvHandle, DirectX::Colors::LightBlue, 0, nullptr);//清除RT背景色为暗红，并且不设置裁剪矩形
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->ClearDepthStencilView(dsvHandle,	//DSV描述符句柄
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,	//FLAG
		1.0f,	//默认深度值
		0,	//默认模板值
		0,	//裁剪矩形数量
		nullptr);	//裁剪矩形指针

	cmdList->OMSetRenderTargets(1,//待绑定的RTV数量
		&rtvHandle,	//指向RTV数组的指针
		true,	//RTV对象在堆内存中是连续存放的
		&dsvHandle);	//指向DSV的指针

	// 设置CBV描述符堆
	ID3D12DescriptorHeap* descriHeaps[] = {cbvHeap.Get()};
	cmdList->SetDescriptorHeaps(_countof(descriHeaps), descriHeaps);
	// 设置根签名
	cmdList->SetGraphicsRootSignature(rootSignature.Get());
	// 设置顶点缓冲区
	cmdList->IASetVertexBuffers(0, 1, &GetVbv());
	cmdList->IASetIndexBuffer(&GetIbv());
	// 将图元拓扑类型传入流水线
	cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 设置根描述符表
	cmdList->SetGraphicsRootDescriptorTable(0,// 根参数的起始索引
	cbvHeap->GetGPUDescriptorHandleForHeapStart());

	//绘制顶点
	cmdList->DrawIndexedInstanced((UINT)indices.size(), // 每个实例要绘制的索引数
	1, // 实例化个数
	0, // 起始索引位置
	0, // 子物体起始索引在全局索引中的位置
	0); // 实例化高级技术, 暂时设置为0

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(swapChainBuffer[ref_mCurrentBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));//从渲染目标到呈现
	//完成命令的记录关闭命令列表
	ThrowIfFailed(cmdList->Close());

	ID3D12CommandList* commandLists[] = { cmdList.Get() };//声明并定义命令列表数组
	cmdQueue->ExecuteCommandLists(_countof(commandLists), commandLists);//将命令从命令列表传至命令队列

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
	//创建CBV堆

	D3D12_DESCRIPTOR_HEAP_DESC cbHeapDesc;
	cbHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbHeapDesc.NumDescriptors = 1;
	cbHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&cbHeapDesc, IID_PPV_ARGS(&cbvHeap)));
}

void D3D12InitApp::BuildConstantBuffers()
{
	// 定义并获得物体的常量缓冲区, 然后得到其首地址

	// elementcount 为1（1个子物体常量缓冲区）, isConstantBuffer为true(为常量缓冲区）
	objCB = std::make_unique<UploadBufferResource<ObjectConstants>>(d3dDevice.Get(), 1, true);
	objConstSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
	// 获得常量缓冲区的首地址
	D3D12_GPU_VIRTUAL_ADDRESS address;
	address = objCB->Resource()->GetGPUVirtualAddress();
	// 通过常量缓冲区元素偏移值计算最终的元素地址
	int cbElementIndex = 0; // 常量缓冲区元素下标
	address += cbElementIndex * objConstSize;
	//创建CBV描述符
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = address;
	cbvDesc.SizeInBytes = objConstSize;
	d3dDevice->CreateConstantBufferView(&cbvDesc, cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12InitApp::BuildRootSignature()
{
	// 根参数可以是描述符表、根描述符、根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	// 创建由单个CBV所组成的描述符表
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //描述符类型
	1, //描述符数量
	0);

	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	// 根签名由一组根参数构成
	CD3DX12_ROOT_SIGNATURE_DESC rootSig(1, //根参数的数量
	slotRootParameter, // 根参数指针
	0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 用单个寄存器槽来创建一个根签名, 该槽位指向一个仅含有单个常量缓冲区的描述符区域
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
	//实例化顶点结构体并填充
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

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &vertexBufferCPU)); // 创建顶点数据内存空间
	CopyMemory(vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &indexBufferCPU)); // 创建索引数据内存空间
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
	psoDesc.SampleMask = UINT_MAX;	//0xffffffff,全部采样，没有遮罩
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;	//归一化的无符号整型
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;	//不使用4XMSAA

	psoDesc.SampleDesc.Quality = 0;	////不使用4XMSAA

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)));
}

void D3D12InitApp::Update()
{
	ObjectConstants objConstants;
	// 构建观察矩阵
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

	// 构建投影矩阵
	XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * 3.1416f, static_cast<float>(mClientWidth) / mClientHeight, 1.0f, 1000.0f);

	// 构建世界矩阵
	XMMATRIX w = XMLoadFloat4x4(&mWorld);

	// 矩阵计算
	XMMATRIX WVP_Matrix = v * p;
	
	// XMMATRIX 赋值给XMFLOAT4x4
	XMStoreFloat4x4(&objConstants.worldViewProj, XMMatrixTranspose(WVP_Matrix));

	// 将数据拷贝至GPU缓存
	objCB->CopyData(0, objConstants);
}

void D3D12InitApp::OnResize()
{
	D3D12App::OnResize();

	//构建投影矩阵
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

