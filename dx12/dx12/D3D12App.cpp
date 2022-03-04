#include "D3D12App.h"
HWND mhMainWnd = 0;	//ĳ�����ڵľ����ShowWindow��UpdateWindow������Ҫ���ô˾��
//���ڹ��̺���������,HWND�������ھ��
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lparam);

D3D12App::D3D12App()
{

}

D3D12App::~D3D12App()
{

}

int D3D12App::Run()
{
	//��Ϣѭ��
	//������Ϣ�ṹ��
	MSG msg = { 0 };
	//���GetMessage����������0��˵��û�н��ܵ�WM_QUIT
	gt.Reset();

	while (msg.message != WM_QUIT)
	{
		//�������-1��˵��GetMessage���������ˣ����������
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);	//���̰���ת�������������Ϣת��Ϊ�ַ���Ϣ
			DispatchMessage(&msg);	//����Ϣ���ɸ���Ӧ�Ĵ��ڹ���
		}
		//�����������ֵ��˵�����յ�����Ϣ
		else
		{
			gt.Tick();
			if (!gt.IsStoped())
			{
				CalculateFrameState();
				Update();
				Draw();
			}
			else
			{
				Sleep(100);
			}
		}
	}
	return (int)msg.wParam;
}

bool D3D12App::Init(HINSTANCE hInstance, int nShowCmd)
{
	if (!InitWindow(hInstance, nShowCmd))
	{
		return false;
	}
	else if (!InitDirect3D())
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool D3D12App::InitWindow(HINSTANCE hInstance, int nShowCmd)
{
	//���ڳ�ʼ�������ṹ��(WNDCLASS)
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;	//����������߸ı䣬�����»��ƴ���
	wc.lpfnWndProc = MainWndProc;	//ָ�����ڹ���
	wc.cbClsExtra = 0;	//�����������ֶ���Ϊ��ǰӦ�÷��������ڴ�ռ䣨���ﲻ���䣬������0��
	wc.cbWndExtra = 0;	//�����������ֶ���Ϊ��ǰӦ�÷��������ڴ�ռ䣨���ﲻ���䣬������0��
	wc.hInstance = hInstance;	//Ӧ�ó���ʵ���������WinMain���룩
	wc.hIcon = LoadIcon(0, IDC_ARROW);	//ʹ��Ĭ�ϵ�Ӧ�ó���ͼ��
	wc.hCursor = LoadCursor(0, IDC_ARROW);	//ʹ�ñ�׼�����ָ����ʽ
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	//ָ���˰�ɫ������ˢ���
	wc.lpszMenuName = 0;	//û�в˵���
	wc.lpszClassName = L"MainWnd";	//������
	//������ע��ʧ��
	if (!RegisterClass(&wc))
	{
		//��Ϣ����������1����Ϣ���������ھ������ΪNULL������2����Ϣ����ʾ���ı���Ϣ������3�������ı�������4����Ϣ����ʽ
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return 0;
	}

	//������ע��ɹ�
	RECT R;	//�ü�����
	R.left = 0;
	R.top = 0;
	R.right = 1280;
	R.bottom = 720;
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);	//���ݴ��ڵĿͻ�����С���㴰�ڵĴ�С
	int width = R.right - R.left;
	int hight = R.bottom - R.top;

	//��������,���ز���ֵ
	mhMainWnd = CreateWindow(L"MainWnd", L"DX12Initialize", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, hight, 0, 0, hInstance, 0);
	//���ڴ���ʧ��
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreatWindow Failed", 0, 0);
		return 0;
	}
	//���ڴ����ɹ�,����ʾ�����´���
	ShowWindow(mhMainWnd, nShowCmd);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3D12App::InitDirect3D()
{
	/*����D3D12���Բ�*/
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	CreateDevice();
	CreateFence();
	GetDescriptorSize();
	SetMSAA();
	CreateCommandObject();
	CreateSwapChain();
	CreateDescriptorHeap();
	CreateRTV();
	CreateDSV();
	CreateViewPortAndScissorRect();

	return true;
}

void D3D12App::CreateDevice()
{
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

	ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3dDevice)));
}

void D3D12App::CreateFence()
{
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
}

void D3D12App::GetDescriptorSize()
{
	rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	cbv_srv_uavDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3D12App::SetMSAA()
{
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
	msaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//UNORM�ǹ�һ��������޷�������
	msaaQualityLevels.SampleCount = 1;
	msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msaaQualityLevels.NumQualityLevels = 0;
	//��ǰͼ��������MSAA���ز�����֧�֣�ע�⣺�ڶ������������������������
	ThrowIfFailed(d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQualityLevels, sizeof(msaaQualityLevels)));
	//NumQualityLevels��Check��������������
	//���֧��MSAA����Check�������ص�NumQualityLevels > 0
	//expressionΪ�٣���Ϊ0��������ֹ�������У�����ӡһ��������Ϣ
	assert(msaaQualityLevels.NumQualityLevels > 0);
}

void D3D12App::CreateCommandObject()
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&cmdQueue)));

	ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator)));

	ThrowIfFailed(d3dDevice->CreateCommandList(0, // ����ֵΪ0�� ��GPU
		D3D12_COMMAND_LIST_TYPE_DIRECT, // �����б�����
		cmdAllocator.Get(), // ����������ӿ�ָ��
		nullptr, // ��ˮ��״̬PSO, ���ﲻ����, ���Ի��ָ��
		IID_PPV_ARGS(&cmdList))); // ���ش����������б�

	cmdList->Close(); // ���������б�ǰ���뽫��ر�
}

void D3D12App::CreateSwapChain()
{
	swapChain.Reset();
	DXGI_SWAP_CHAIN_DESC swapChainDesc;	//�����������ṹ��
	swapChainDesc.BufferDesc.Width = 1280;	//�������ֱ��ʵĿ��
	swapChainDesc.BufferDesc.Height = 720;	//�������ֱ��ʵĸ߶�
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//����������ʾ��ʽ
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;	//ˢ���ʵķ���
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;	//ˢ���ʵķ�ĸ
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;	//����ɨ��VS����ɨ��(δָ����)
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	//ͼ�������Ļ�����죨δָ���ģ�
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	//��������Ⱦ����̨������������Ϊ��ȾĿ�꣩
	swapChainDesc.OutputWindow = mhMainWnd;	//��Ⱦ���ھ��
	swapChainDesc.SampleDesc.Count = 1;	//���ز�������
	swapChainDesc.SampleDesc.Quality = 0;	//���ز�������
	swapChainDesc.Windowed = true;	//�Ƿ񴰿ڻ�
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	//�̶�д��
	swapChainDesc.BufferCount = 2;	//��̨������������˫���壩
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	//����Ӧ����ģʽ���Զ�ѡ�������ڵ�ǰ���ڳߴ����ʾģʽ��

	  //����DXGI�ӿ��µĹ����ഴ��������
	ThrowIfFailed(dxgiFactory->CreateSwapChain(cmdQueue.Get(), &swapChainDesc, swapChain.GetAddressOf()));
}

void D3D12App::CreateDescriptorHeap()
{
	// ���ȴ���RTV��
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvHeap)));

	//Ȼ�󴴽�DSV��
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	dsvDescriptorHeapDesc.NumDescriptors = 1;
	dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&dsvHeap)));
}

void D3D12App::CreateRTV()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < 2; i++)
	{
		//��ô��ڽ������еĺ�̨��������Դ
		swapChain->GetBuffer(i, IID_PPV_ARGS(swapChainBuffer[i].GetAddressOf()));
		//����RTV
		d3dDevice->CreateRenderTargetView(swapChainBuffer[i].Get(),
			nullptr,	//�ڽ������������Ѿ������˸���Դ�����ݸ�ʽ����������ָ��Ϊ��ָ��
			rtvHeapHandle);	//����������ṹ�壨�����Ǳ��壬�̳���CD3DX12_CPU_DESCRIPTOR_HANDLE��
		//ƫ�Ƶ����������е���һ��������
		rtvHeapHandle.Offset(1, rtvDescriptorSize);
	}
}

void D3D12App::CreateDSV()
{
	assert(d3dDevice);
	assert(swapChain);
	assert(cmdAllocator);

	// Flush before changing any resources.
	FlushCmdQueue();

	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));

	//��CPU�д��������ģ��������Դ
	D3D12_RESOURCE_DESC dsvResourceDesc;
	dsvResourceDesc.Alignment = 0;	//ָ������
	dsvResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	//ָ����Դά�ȣ����ͣ�ΪTEXTURE2D
	dsvResourceDesc.DepthOrArraySize = 1;	//�������Ϊ1
	dsvResourceDesc.Width = 1280;	//��Դ��
	dsvResourceDesc.Height = 720;	//��Դ��
	dsvResourceDesc.MipLevels = 1;	//MIPMAP�㼶����
	dsvResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//ָ�������֣����ﲻָ����
	dsvResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	//���ģ����Դ��Flag
	dsvResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	//24λ��ȣ�8λģ��,���и������͵ĸ�ʽDXGI_FORMAT_R24G8_TYPELESSҲ����ʹ��
	dsvResourceDesc.SampleDesc.Count = 1;	//���ز�������
	dsvResourceDesc.SampleDesc.Quality = 0;	//���ز�������
	CD3DX12_CLEAR_VALUE optClear;	//�����Դ���Ż�ֵ��������������ִ���ٶȣ�CreateCommittedResource�����д��룩
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//24λ��ȣ�8λģ��,���и������͵ĸ�ʽDXGI_FORMAT_R24G8_TYPELESSҲ����ʹ��
	optClear.DepthStencil.Depth = 1;	//��ʼ���ֵΪ1
	optClear.DepthStencil.Stencil = 0;	//��ʼģ��ֵΪ0
	//����һ����Դ��һ���ѣ�������Դ�ύ�����У������ģ�������ύ��GPU�Դ��У�

	ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),	//������ΪĬ�϶ѣ�����д�룩
		D3D12_HEAP_FLAG_NONE,	//Flag
		&dsvResourceDesc,	//���涨���DSV��Դָ��
		D3D12_RESOURCE_STATE_COMMON,	//��Դ��״̬Ϊ��ʼ״̬
		&optClear,	//���涨����Ż�ֵָ��
		IID_PPV_ARGS(&depthStencilBuffer)));	//�������ģ����Դ
		//����DSV(�������DSV���Խṹ�壬�ʹ���RTV��ͬ��RTV��ͨ�����)
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	d3dDevice->CreateDepthStencilView(depthStencilBuffer.Get(),
		nullptr,	//D3D12_DEPTH_STENCIL_VIEW_DESC����ָ�룬����&dsvDesc������ע�ʹ��룩��
							//�����ڴ������ģ����Դʱ�Ѿ��������ģ���������ԣ������������ָ��Ϊ��ָ��
		dsvHeap->GetCPUDescriptorHandleForHeapStart());	//DSV���

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList.Get() };
	cmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCmdQueue();
}

void D3D12App::CreateViewPortAndScissorRect()
{
	//�ӿ�����
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.Width = 1280;
	viewPort.Height = 720;
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;
	//�ü��������ã�����������ض������޳���
	//ǰ����Ϊ���ϵ����꣬������Ϊ���µ�����
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = 1280;
	scissorRect.bottom = 720;
}

void D3D12App::FlushCmdQueue()
{
	mCurrentFence++;	//CPU��������رպ󣬽���ǰΧ��ֵ+1
	cmdQueue->Signal(fence.Get(), mCurrentFence);	//��GPU������CPU���������󣬽�fence�ӿ��е�Χ��ֵ+1����fence->GetCompletedValue()+1
	if (fence->GetCompletedValue() < mCurrentFence)	//���С�ڣ�˵��GPUû�д�������������
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");	//�����¼�
		fence->SetEventOnCompletion(mCurrentFence, eventHandle);//��Χ���ﵽmCurrentFenceֵ����ִ�е�Signal����ָ���޸���Χ��ֵ��ʱ������eventHandle�¼�
		WaitForSingleObject(eventHandle, INFINITE);//�ȴ�GPU����Χ���������¼���������ǰ�߳�ֱ���¼�������ע���Enent���������ٵȴ���
							   //���û��Set��Wait���������ˣ�Set��Զ������ã�����Ҳ��û�߳̿��Ի�������̣߳�
		CloseHandle(eventHandle);
	}
}

void D3D12App::CalculateFrameState()
{
	static int frameCnt = 0;	//��֡��
	static float timeElapsed = 0.0f;	//���ŵ�ʱ��
	frameCnt++;	//ÿ֡++������һ����伴ΪFPSֵ
	//����ģ��
	/*std::wstring text = std::to_wstring(gt.TotalTime());
	std::wstring windowText = text;
	SetWindowText(mhMainWnd, windowText.c_str());*/
	//�ж�ģ��
	if ((gt.TotalTime() - timeElapsed) >= 1.0f)	//һ��>=0��˵���պù�һ��
	{
		float fps = (float)frameCnt;//ÿ�����֡
		float mspf = 1000.0f / fps;	//ÿ֡���ٺ���

		std::wstring fpsStr = std::to_wstring(fps);//תΪ���ַ�
		std::wstring mspfStr = std::to_wstring(mspf);
		//��֡������ʾ�ڴ�����
		std::wstring windowText = L"D3D12Init    fps:" + fpsStr + L"    " + L"mspf" + mspfStr;
		SetWindowText(mhMainWnd, windowText.c_str());

		//Ϊ������һ��֡��ֵ������
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

//���ڹ��̺���
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//��Ϣ����
	switch (msg)
	{
		//�����ڱ�����ʱ����ֹ��Ϣѭ��
	case WM_DESTROY:
		PostQuitMessage(0);	//��ֹ��Ϣѭ����������WM_QUIT��Ϣ
		return 0;
	default:
		break;
	}
	//������û�д������Ϣת����Ĭ�ϵĴ��ڹ���
	return DefWindowProc(hwnd, msg, wParam, lParam);
}