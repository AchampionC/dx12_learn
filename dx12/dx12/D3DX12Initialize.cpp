
#include "ToolFunc.h"
#include "d3dx12.h"
#include "GameTime.h"
#include "D3D12App.h"
#include "ProceduralGeometry.h"
#include "DirectXPackedVector.h"
#include "UploadBufferResource.h"
#include "FrameResources.h"

using namespace Microsoft::WRL;
using namespace DirectX;

int frameResourcesCount = 3;

//定义顶点结构体
struct VPosData
{
	XMFLOAT3 Pos;
};

struct VColorData
{
	XMFLOAT4 Color;
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

std::unique_ptr<UploadBufferResource<ObjectConstants>> objCB = nullptr;
std::unique_ptr<UploadBufferResource<PassConstants>> passCB = nullptr;


struct RenderItem
{
	RenderItem() = default;

	// 该几何体的世界矩阵
	XMFLOAT4X4 world = MathHelper::Identity4x4();

	// 该几何体的常量数据再objConstantBuffer中的索引
	UINT objCBIndex = -1;

	// 该几何体的图元拓扑类型
	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 该几何体的绘制三参数
	UINT indexCount = 0;
	UINT startIndexLocation = 0;
	UINT baseVertexLocation = 0;

	UINT numFramesDirty = 3;

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
	void BuildRenderItem();
	void BuildFrameResources();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSO();
	void DrawRenderItem();
	void UpdateObjectCBs();
	void UpdatePassCBs();
	D3D12_VERTEX_BUFFER_VIEW GetVbv();
	D3D12_VERTEX_BUFFER_VIEW GetVColorBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIbv();

	std::vector<std::unique_ptr<FrameResources>> FrameResourcesArray;
	int currFrameResourcesIndex = 0; 
	UINT currentFence = 0;
	FrameResources* currFrameResources = nullptr;
	std::vector<std::unique_ptr<RenderItem>> allRitems;
	UINT objConstSize;
	UINT passConstSize;
	ComPtr<ID3D12DescriptorHeap> cbvHeap;
	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDesc;

	std::unique_ptr<UploadBufferResource<ObjectConstants>> objCB = nullptr;

	ComPtr<ID3DBlob> vsBytecode = nullptr;
	ComPtr<ID3DBlob> psBytecode = nullptr;

	//UINT vPosBufferByteSize;
	//UINT vColorBufferByteSize;
	//UINT ibByteSize;
	UINT vertexBufferByteSize;
	UINT IndexBufferByteSize;

	//ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
	//ComPtr<ID3DBlob> vPosBufferCPU = nullptr;
	//ComPtr<ID3DBlob> indexBufferCPU = nullptr;
	ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> indexBufferCPU = nullptr;


	//ComPtr<ID3D12Resource> vPosBufferGpu = nullptr;
	//ComPtr<ID3D12Resource> vColorBufferGpu = nullptr;
	//ComPtr<ID3D12Resource> indexBufferGpu = nullptr;
	ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

	//ComPtr<ID3D12Resource> vPosBufferUploader = nullptr;
	//ComPtr<ID3D12Resource> vColorBufferUploader = nullptr;
	//ComPtr<ID3D12Resource> indexBufferUploader = nullptr;
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

	// 绘制子物体的三个属性
	struct SubmeshGeometry
	{
		UINT indexCount;
		UINT startIndexLocation;
		UINT baseVertexLocation;
	};

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

};





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
		float dx = XMConvertToRadians(static_cast<float>(x - mLastMousePos.x) * 0.25f);
		float dy = XMConvertToRadians(static_cast<float>(y - mLastMousePos.y) * 0.25f);
		
		// 计算鼠标没有松开前的累计弧度
		mTheta += dx;
		mPhi += dy;

		// 限制角度phi的范围(0.1, Pi - 0.1)
		mPhi = MathHelper::Clamp(mPhi, 0.1f, 3.1416f - 0.1f);
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
	//ThrowIfFailed(cmdAllocator->Reset());//重复使用记录命令的相关内存
	//ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));//复用命令列表及其内存

	//ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), PSO.Get()));
	auto currCmdAllocator = currFrameResources->cmdAllocator;
	ThrowIfFailed(currCmdAllocator->Reset());//重复使用记录命令的相关内存
	ThrowIfFailed(cmdList->Reset(currCmdAllocator.Get(), PSO.Get()));//复用命令列表及其内存

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
	//cmdList->IASetVertexBuffers(1, 1, &GetVColorBufferView());
	cmdList->IASetIndexBuffer(&GetIbv());
	// 将图元拓扑类型传入流水线
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	// 设置根描述符表
	//int objCbvIndex = 0;
	//auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvHeap->GetGPUDescriptorHandleForHeapStart());
	//handle.Offset(objCbvIndex, cbv_srv_uavDescriptorSize);
	//cmdList->SetGraphicsRootDescriptorTable(0,// 根参数的起始索引
	//handle);

	int passCbvIndex = (int)allRitems.size() * frameResourcesCount + currFrameResourcesIndex;
	auto passcbvhandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvHeap->GetGPUDescriptorHandleForHeapStart());
	passcbvhandle.Offset(passCbvIndex, cbv_srv_uavDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(1,
		passcbvhandle);

	////绘制顶点
	//cmdList->DrawIndexedInstanced((UINT)indices.size(), // 每个实例要绘制的索引数
	//1, // 实例化个数
	//0, // 起始索引位置
	//0, // 子物体起始索引在全局索引中的位置
	//0); // 实例化高级技术, 暂时设置为0

	#if 0
	cmdList->DrawIndexedInstanced(DrawArgs["box"].indexCount, //每个实例要绘制的索引数
		1,	//实例化个数
		DrawArgs["box"].startIndexLocation,	//起始索引位置
		DrawArgs["box"].baseVertexLocation,	//子物体起始索引在全局索引中的位置
		0);	//实例化的高级技术，暂时设置为0
	cmdList->DrawIndexedInstanced(DrawArgs["grid"].indexCount, //每个实例要绘制的索引数
		1,	//实例化个数
		DrawArgs["grid"].startIndexLocation,	//起始索引位置
		DrawArgs["grid"].baseVertexLocation,	//子物体起始索引在全局索引中的位置
		0);	//实例化的高级技术，暂时设置为0
	cmdList->DrawIndexedInstanced(DrawArgs["sphere"].indexCount, //每个实例要绘制的索引数
		1,	//实例化个数
		DrawArgs["sphere"].startIndexLocation,	//起始索引位置
		DrawArgs["sphere"].baseVertexLocation,	//子物体起始索引在全局索引中的位置
		0);	//实例化的高级技术，暂时设置为0
	cmdList->DrawIndexedInstanced(DrawArgs["cylinder"].indexCount, //每个实例要绘制的索引数
		1,	//实例化个数
		DrawArgs["cylinder"].startIndexLocation,	//起始索引位置
		DrawArgs["cylinder"].baseVertexLocation,	//子物体起始索引在全局索引中的位置
		0);	//实例化的高级技术，暂时设置为0
	#endif 
	DrawRenderItem();
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(swapChainBuffer[ref_mCurrentBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));//从渲染目标到呈现
	//完成命令的记录关闭命令列表
	ThrowIfFailed(cmdList->Close());

	ID3D12CommandList* commandLists[] = { cmdList.Get() };//声明并定义命令列表数组
	cmdQueue->ExecuteCommandLists(_countof(commandLists), commandLists);//将命令从命令列表传至命令队列

	ThrowIfFailed(swapChain->Present(0, 0));
	ref_mCurrentBackBuffer = (ref_mCurrentBackBuffer + 1) % 2;


	currFrameResources->fenceCPU = ++currentFence;
	cmdQueue->Signal(fence.Get(), currentFence);
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
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildRenderItem();
	BuildFrameResources();
	BuildConstantBuffers();
	BuildPSO();

	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* cmdLists[] = { cmdList.Get() };
	cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCmdQueue();

	return true;
}

void D3D12InitApp::BuildDescriptorHeaps()
{
	objConstSize = ToolFunc::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	//创建CBV堆

	D3D12_DESCRIPTOR_HEAP_DESC cbHeapDesc;
	cbHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbHeapDesc.NumDescriptors = 2;
	cbHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&cbHeapDesc, IID_PPV_ARGS(&cbvHeap)));
}

void D3D12InitApp::BuildRenderItem()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&(boxRitem->world), XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->objCBIndex = 0;//BOX常量数据（world矩阵）在objConstantBuffer索引0上
	boxRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->indexCount = DrawArgs["box"].indexCount;
	boxRitem->baseVertexLocation = DrawArgs["box"].baseVertexLocation;
	boxRitem->startIndexLocation = DrawArgs["box"].startIndexLocation;
	allRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->world = MathHelper::Identity4x4();
	gridRitem->objCBIndex = 1;//BOX常量数据（world矩阵）在objConstantBuffer索引1上
	gridRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->indexCount = DrawArgs["grid"].indexCount;
	gridRitem->baseVertexLocation = DrawArgs["grid"].baseVertexLocation;
	gridRitem->startIndexLocation = DrawArgs["grid"].startIndexLocation;
	allRitems.push_back(std::move(gridRitem));

	UINT fllowObjCBIndex = 2;//接下去的几何体常量数据在CB中的索引从2开始
	//将圆柱和圆的实例模型存入渲染项中
	for (int i = 0; i < 5; i++)
	{
		auto leftCylinderRitem = std::make_unique<RenderItem>();
		auto rightCylinderRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);
		//左边5个圆柱
		XMStoreFloat4x4(&(leftCylinderRitem->world), leftCylWorld);
		//此处的索引随着循环不断加1（注意：这里是先赋值再++）
		leftCylinderRitem->objCBIndex = fllowObjCBIndex++;
		leftCylinderRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylinderRitem->indexCount = DrawArgs["cylinder"].indexCount;
		leftCylinderRitem->baseVertexLocation = DrawArgs["cylinder"].baseVertexLocation;
		leftCylinderRitem->startIndexLocation = DrawArgs["cylinder"].startIndexLocation;
		//右边5个圆柱
		XMStoreFloat4x4(&(rightCylinderRitem->world), rightCylWorld);
		rightCylinderRitem->objCBIndex = fllowObjCBIndex++;
		rightCylinderRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylinderRitem->indexCount = DrawArgs["cylinder"].indexCount;
		rightCylinderRitem->baseVertexLocation = DrawArgs["cylinder"].baseVertexLocation;
		rightCylinderRitem->startIndexLocation = DrawArgs["cylinder"].startIndexLocation;
		//左边5个球
		XMStoreFloat4x4(&(leftSphereRitem->world), leftSphereWorld);
		leftSphereRitem->objCBIndex = fllowObjCBIndex++;
		leftSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->indexCount = DrawArgs["sphere"].indexCount;
		leftSphereRitem->baseVertexLocation = DrawArgs["sphere"].baseVertexLocation;
		leftSphereRitem->startIndexLocation = DrawArgs["sphere"].startIndexLocation;
		//右边5个球
		XMStoreFloat4x4(&(rightSphereRitem->world), rightSphereWorld);
		rightSphereRitem->objCBIndex = fllowObjCBIndex++;
		rightSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->indexCount = DrawArgs["sphere"].indexCount;
		rightSphereRitem->baseVertexLocation = DrawArgs["sphere"].baseVertexLocation;
		rightSphereRitem->startIndexLocation = DrawArgs["sphere"].startIndexLocation;

		allRitems.push_back(std::move(leftCylinderRitem));
		allRitems.push_back(std::move(rightCylinderRitem));
		allRitems.push_back(std::move(leftSphereRitem));
		allRitems.push_back(std::move(rightSphereRitem));
	}
}

void D3D12InitApp::BuildFrameResources()
{
	for (int i = 0; i < frameResourcesCount; i ++ )
	{
		FrameResourcesArray.push_back(std::make_unique<FrameResources>(d3dDevice.Get(), 1, (UINT)allRitems.size()));
	}
}

void D3D12InitApp::BuildConstantBuffers()
{
#if 0
	// 定义并获得物体的常量缓冲区, 然后得到其首地址
	objConstSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	// elementcount 为1（1个子物体常量缓冲区）, isConstantBuffer为true(为常量缓冲区）
	objCB = std::make_unique<UploadBufferResource<ObjectConstants>>(d3dDevice.Get(), 1, true);
	// 获得常量缓冲区的首地址
	D3D12_GPU_VIRTUAL_ADDRESS obj_Address;
	obj_Address = objCB->Resource()->GetGPUVirtualAddress();
	int objCbElementIndex = 0;
	obj_Address += objCbElementIndex * objConstSize;
	int heapIndex = 0;
	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvHeap->GetCPUDescriptorHandleForHeapStart());
	handle.Offset(heapIndex, cbv_srv_uavDescriptorSize);
	//创建CBV描述符
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc0;
	cbvDesc0.BufferLocation = obj_Address;
	cbvDesc0.SizeInBytes = objConstSize;
	d3dDevice->CreateConstantBufferView(&cbvDesc0, handle);

	passConstSize = CalcConstantBufferByteSize(sizeof(PassConstants));
	passCB = std::make_unique<UploadBufferResource<PassConstants>>(d3dDevice.Get(), 1, true);
	D3D12_GPU_VIRTUAL_ADDRESS passcb_Address;
	passcb_Address = passCB->Resource()->GetGPUVirtualAddress();
	int	passCbElementIndex = 0;
	passcb_Address += passCbElementIndex * passConstSize;
	heapIndex = 1;
	handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvHeap->GetCPUDescriptorHandleForHeapStart());
	handle.Offset(heapIndex, cbv_srv_uavDescriptorSize);
	// 创建cbv描述符
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc1;
	cbvDesc1.BufferLocation = passcb_Address;
	cbvDesc1.SizeInBytes = passConstSize;
	d3dDevice->CreateConstantBufferView(&cbvDesc1, handle);
#endif
	UINT objectCount = (UINT)allRitems.size();//物体总个数（包括实例）
	UINT objConstSize = ToolFunc::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT passConstSize = ToolFunc::CalcConstantBufferByteSize(sizeof(PassConstants));
	//创建CBV堆
	D3D12_DESCRIPTOR_HEAP_DESC cbHeapDesc;
	cbHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbHeapDesc.NumDescriptors = (objectCount + 1) * frameResourcesCount;//此处一个堆中包含(几何体个数（包含实例）+1)个CBV
	cbHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&cbHeapDesc, IID_PPV_ARGS(&cbvHeap)));

	//elementCount为objectCount,22（22个子物体常量缓冲元素），isConstantBuffer为ture（是常量缓冲区）
	objCB = std::make_unique<UploadBufferResource<ObjectConstants>>(d3dDevice.Get(), objectCount, true);
	//获得常量缓冲区首地址
	for (int frameIndex = 0; frameIndex < frameResourcesCount; frameIndex ++)
	{
		for (int i = 0; i < objectCount; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS objCB_Address;
			objCB_Address = FrameResourcesArray[frameIndex]->objCB->Resource()->GetGPUVirtualAddress();
			objCB_Address += i * objConstSize;//子物体在常量缓冲区中的地址
			int heapIndex = objectCount * frameIndex + i;	//CBV堆中的CBV元素索引
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvHeap->GetCPUDescriptorHandleForHeapStart());//获得CBV堆首地址
			handle.Offset(heapIndex, cbv_srv_uavDescriptorSize);	//CBV句柄（CBV堆中的CBV元素地址）
			//创建CBV描述符
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = objCB_Address;
			cbvDesc.SizeInBytes = objConstSize;
			d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	//创建过程常量缓冲区描述符
	passCB = std::make_unique<UploadBufferResource<PassConstants>>(d3dDevice.Get(), 1, true);
	//获得常量缓冲区首地址
	for (int frameIndex = 0; frameIndex < frameResourcesCount; frameIndex++)
	{
		D3D12_GPU_VIRTUAL_ADDRESS passCB_Address;
		passCB_Address = passCB->Resource()->GetGPUVirtualAddress();
		int passCbElementIndex = 0;
		passCB_Address += passCbElementIndex * passConstSize;
		int heapIndex = objectCount * frameResourcesCount + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, cbv_srv_uavDescriptorSize);
		//创建CBV描述符
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = passCB_Address;
		cbvDesc.SizeInBytes = passConstSize;
		d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void D3D12InitApp::BuildRootSignature()
{
	// 根参数可以是描述符表、根描述符、根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	// 创建由单个CBV所组成的描述符表
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //描述符类型
	1, //描述符数量
	0); // 描述符所绑定的寄存器号

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		1);// 描述符所绑定的寄存器号

	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
	// 根签名由一组根参数构成
	CD3DX12_ROOT_SIGNATURE_DESC rootSig(2, //根参数的数量
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
#if 0
	//实例化顶点结构体并填充
	std::array<VPosData, 8> verticesPos =
	{
		VPosData({ XMFLOAT3(-1.0f, -1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, +1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, +1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, -1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, -1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, +1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, +1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, -1.0f, +1.0f) })
	};

	std::array<VColorData, 8> verticesColor =
	{
		VColorData({ XMFLOAT4(Colors::White) }),
		VColorData({ XMFLOAT4(Colors::Black) }),
		VColorData({ XMFLOAT4(Colors::Red) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Blue) }),
		VColorData({ XMFLOAT4(Colors::Yellow) }),
		VColorData({ XMFLOAT4(Colors::Cyan) }),
		VColorData({ XMFLOAT4(Colors::Magenta) })
	};

	//vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	vPosBufferByteSize = (UINT)verticesPos.size() * sizeof(VPosData);
	vColorBufferByteSize = (UINT)verticesColor.size() * sizeof(VColorData);
	ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(vPosBufferByteSize, &vertexBufferCPU)); // 创建顶点数据内存空间
	CopyMemory(vertexBufferCPU->GetBufferPointer(), verticesPos.data(), vPosBufferByteSize);

	ThrowIfFailed(D3DCreateBlob(vColorBufferByteSize, &vPosBufferCPU));
	CopyMemory(vPosBufferCPU->GetBufferPointer(), verticesColor.data(), vColorBufferByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &indexBufferCPU)); // 创建索引数据内存空间
	CopyMemory(indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	vPosBufferGpu = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), vPosBufferByteSize, verticesPos.data(), vPosBufferUploader);
	vColorBufferGpu = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), vColorBufferByteSize, verticesColor.data(), vColorBufferUploader);
	indexBufferGpu = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), ibByteSize, indices.data(), indexBufferUploader);
#endif 
	ProceduralGeometry proceGeo;
	ProceduralGeometry::MeshData box = proceGeo.CreateBox(1.5f, 0.5f, 1.5f, 3);
	ProceduralGeometry::MeshData grid = proceGeo.CreateGrid(20.0f, 30.0f, 60, 40);
	ProceduralGeometry::MeshData sphere = proceGeo.CreateSphere(0.5f, 20, 20);
	ProceduralGeometry::MeshData cylinder = proceGeo.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//计算单个几何体顶点在总顶点数组中的偏移量,顺序为：box、grid、sphere、cylinder
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = (UINT)grid.Vertices.size() + gridVertexOffset;
	UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;

	//计算单个几何体索引在总索引数组中的偏移量,顺序为：box、grid、sphere、cylinder
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = (UINT)grid.Indices32.size() + gridIndexOffset;
	UINT cylinderIndexOffset = (UINT)sphere.Indices32.size() + sphereIndexOffset;


	SubmeshGeometry boxSubmesh;
	boxSubmesh.indexCount = (UINT)box.Indices32.size();
	boxSubmesh.baseVertexLocation = boxVertexOffset;
	boxSubmesh.startIndexLocation = boxIndexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.indexCount = (UINT)grid.Indices32.size();
	gridSubmesh.baseVertexLocation = gridVertexOffset;
	gridSubmesh.startIndexLocation = gridIndexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.indexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.baseVertexLocation = sphereVertexOffset;
	sphereSubmesh.startIndexLocation = sphereIndexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.indexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.baseVertexLocation = cylinderVertexOffset;
	cylinderSubmesh.startIndexLocation = cylinderIndexOffset;

	size_t totalVertexCount = box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() + cylinder.Vertices.size();
	std::vector<Vertex> vertices(totalVertexCount);
	int k = 0;
	for (int i = 0; i < box.Vertices.size(); i++, k ++ )
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow);
	}

	for (int i = 0; i < grid.Vertices.size(); i ++, k++)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Brown);
	}

	for (int i = 0; i < sphere.Vertices.size(); i++, k ++ )
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Green);
	}

	for (int i = 0; i < cylinder.Vertices.size(); i ++, k++)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
	}
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());

	// 计算vertices 和 indices 两个缓存的各自大小
	const UINT vbByteSize = (UINT) vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT) indices.size() * sizeof(std::uint16_t);

	vertexBufferByteSize = vbByteSize;
	IndexBufferByteSize = ibByteSize;
	
	// 复制内存中的顶点索引数据至GPU缓存
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &vertexBufferCPU));
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &indexBufferCPU));
	CopyMemory(vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	CopyMemory(indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	vertexBufferGPU = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), vbByteSize, vertices.data(), vertexBufferUploader);
	indexBufferGPU = ToolFunc::CreateDefaultBuffer(d3dDevice.Get(), cmdList.Get(), ibByteSize, indices.data(), indexBufferUploader);


	DrawArgs["box"] = boxSubmesh;
	DrawArgs["grid"] = gridSubmesh;
	DrawArgs["sphere"] = sphereSubmesh;
	DrawArgs["cylinder"] = cylinderSubmesh;

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

void D3D12InitApp::DrawRenderItem()
{
	//将智能指针数组转换成普通指针数组
	std::vector<RenderItem*> ritems;
	for (auto& e : allRitems)
		ritems.push_back(e.get());

	//遍历渲染项数组
	for (int i = 0; i < (int)ritems.size(); i++)
	{
		auto ritem = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &GetVbv());
		cmdList->IASetIndexBuffer(&GetIbv());
		cmdList->IASetPrimitiveTopology(ritem->primitiveType);

		//设置根描述符表
		UINT objCbvIndex = currFrameResourcesIndex * (UINT)allRitems.size() + ritem->objCBIndex;
		auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvHeap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(objCbvIndex, cbv_srv_uavDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, //根参数的起始索引
			handle);

		//绘制顶点（通过索引缓冲区绘制）
		cmdList->DrawIndexedInstanced(ritem->indexCount, //每个实例要绘制的索引数
			1,	//实例化个数
			ritem->startIndexLocation,	//起始索引位置
			ritem->baseVertexLocation,	//子物体起始索引在全局索引中的位置
			0);	//实例化的高级技术，暂时设置为0
	}
}

void D3D12InitApp::UpdateObjectCBs()
{
	ObjectConstants objConstants;
	PassConstants passConstants;
	for (auto& e : allRitems)
	{
		if (e->numFramesDirty > 0)
		{
			mWorld = e->world;
			XMMATRIX w = XMLoadFloat4x4(&mWorld);
			XMStoreFloat4x4(&objConstants.world, XMMatrixTranspose(w));
			// 将数据拷贝至GPU缓存
			currFrameResources->objCB->CopyData(e->objCBIndex, objConstants);

			e->numFramesDirty -- ;
		}
	}
}

void D3D12InitApp::UpdatePassCBs()
{

	ObjectConstants objConstants;
	PassConstants passConstants;

	float y = mRadius * cosf(mPhi);
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX v = XMMatrixLookAtLH(pos, target, up);

	XMMATRIX p = XMLoadFloat4x4(&mProj);

	// 矩阵计算
	XMMATRIX VP_Matrix = v * p;
	XMStoreFloat4x4(&passConstants.viewProj, XMMatrixTranspose(VP_Matrix));
	passCB->CopyData(0, passConstants);
}

void D3D12InitApp::Update()
{
#if 0
	ObjectConstants objConstants;
	PassConstants passConstants;

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

	//世界矩阵数据传递至GPU
	for (auto& e : allRitems)
	{
		mWorld = e->world;
		XMMATRIX w = XMLoadFloat4x4(&mWorld);
		//XMMATRIX赋值给XMFLOAT4X4
		XMStoreFloat4x4(&objConstants.world, XMMatrixTranspose(w));
		//将数据拷贝至GPU缓存
		objCB->CopyData(e->objCBIndex, objConstants);
	}


	// 构建投影矩阵
	//XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * 3.1416f, static_cast<float>(mClientWidth) / mClientHeight, 1.0f, 1000.0f);

	// 构建世界矩阵
	//XMMATRIX w = XMLoadFloat4x4(&mWorld);
	//w *= XMMatrixTranslation(2.0f, 0.0f, 0.0f);

	// 拿到投影矩阵
	XMMATRIX p = XMLoadFloat4x4(&mProj);

	// 矩阵计算
	XMMATRIX VP_Matrix = v * p;
	XMStoreFloat4x4(&passConstants.viewProj, XMMatrixTranspose(VP_Matrix));
	passCB->CopyData(0, passConstants);
	//
	//// XMMATRIX 赋值给XMFLOAT4x4
	//XMStoreFloat4x4(&objConstants.world, XMMatrixTranspose(w));

	//// 将数据拷贝至GPU缓存
	//objCB->CopyData(0, objConstants);
	
	#endif 


	// 每帧遍历一个帧资源(多帧的话就是环形遍历)
	currFrameResourcesIndex = (currFrameResourcesIndex + 1) % frameResourcesCount;
	currFrameResources = FrameResourcesArray[currFrameResourcesIndex].get();

	// 如果GPU端围栏值小于CPU端围栏值, 即CPU速度快于GPU, 则令CPU等待
	if (currFrameResources->fenceCPU != 0 && fence->GetCompletedValue() < currFrameResources->fenceCPU)
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");
		ThrowIfFailed(fence->SetEventOnCompletion(currFrameResources->fenceCPU, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs();
	UpdatePassCBs();
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
	vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
	vbv.SizeInBytes = vertexBufferByteSize;
	vbv.StrideInBytes = sizeof(Vertex);

	return vbv;
}

D3D12_VERTEX_BUFFER_VIEW D3D12InitApp::GetVColorBufferView()
{
	D3D12_VERTEX_BUFFER_VIEW vColorBufferView;
	//vColorBufferView.BufferLocation = vColorBufferGpu->GetGPUVirtualAddress();
	//vColorBufferView.SizeInBytes = vColorBufferByteSize;
	//vColorBufferView.StrideInBytes = sizeof(VColorData);
	vColorBufferView.BufferLocation = 0;
	vColorBufferView.SizeInBytes = 0;
	vColorBufferView.StrideInBytes = 0;

	return vColorBufferView;
}

D3D12_INDEX_BUFFER_VIEW D3D12InitApp::GetIbv()
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R16_UINT;
	ibv.SizeInBytes = IndexBufferByteSize;

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

