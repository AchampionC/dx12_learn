#include "D3D12App.h"
HWND mhMainWnd = 0;	//某个窗口的句柄，ShowWindow和UpdateWindow函数均要调用此句柄
//窗口过程函数的声明,HWND是主窗口句柄
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lparam);

void D3D12App::OnResize()
{
	assert(d3dDevice);
	assert(swapChain);
	assert(cmdAllocator);
	FlushCmdQueue(); // 改变资源前先同步

	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));

	// 释放之前的资源, 为我们重新创建做好准备
	for (int i = 0; i < 2; i++)
	{
		swapChainBuffer[i].Reset();
	}

	depthStencilBuffer.Reset();

	// 重新调整后台缓冲区资源的大小
	ThrowIfFailed(swapChain->ResizeBuffers(2, 
	mClientWidth, 
	mClientHeight,
	DXGI_FORMAT_R8G8B8A8_UNORM,
	DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	//后台缓冲区索引置零
	mCurrentBackBuffer = 0;

	CreateRTV();
	CreateDSV();
	CreateViewPortAndScissorRect();
}

D3D12App* D3D12App::mApp = nullptr;

D3D12App::D3D12App(HINSTANCE hInstance)
:mhAppInst(hInstance)
{
	assert(mApp == nullptr);
	mApp = this;
}

D3D12App::~D3D12App()
{

}

int D3D12App::Run()
{
	//消息循环
	//定义消息结构体
	MSG msg = { 0 };
	//如果GetMessage函数不等于0，说明没有接受到WM_QUIT
	gt.Reset();

	while (msg.message != WM_QUIT)
	{
		//如果等于-1，说明GetMessage函数出错了，弹出错误框
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);	//键盘按键转换，将虚拟键消息转换为字符消息
			DispatchMessage(&msg);	//把消息分派给相应的窗口过程
		}
		//如果等于其他值，说明接收到了消息
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
	if (!InitDirect3D())
	{
		return false;
	}
	
	OnResize();

	return true;
}

bool D3D12App::InitWindow(HINSTANCE hInstance, int nShowCmd)
{
	//窗口初始化描述结构体(WNDCLASS)
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;	//当工作区宽高改变，则重新绘制窗口
	wc.lpfnWndProc = MainWndProc;	//指定窗口过程
	wc.cbClsExtra = 0;	//借助这两个字段来为当前应用分配额外的内存空间（这里不分配，所以置0）
	wc.cbWndExtra = 0;	//借助这两个字段来为当前应用分配额外的内存空间（这里不分配，所以置0）
	wc.hInstance = hInstance;	//应用程序实例句柄（由WinMain传入）
	wc.hIcon = LoadIcon(0, IDC_ARROW);	//使用默认的应用程序图标
	wc.hCursor = LoadCursor(0, IDC_ARROW);	//使用标准的鼠标指针样式
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	//指定了白色背景画刷句柄
	wc.lpszMenuName = 0;	//没有菜单栏
	wc.lpszClassName = L"MainWnd";	//窗口名
	//窗口类注册失败
	if (!RegisterClass(&wc))
	{
		//消息框函数，参数1：消息框所属窗口句柄，可为NULL。参数2：消息框显示的文本信息。参数3：标题文本。参数4：消息框样式
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return 0;
	}

	//窗口类注册成功
	RECT R;	//裁剪矩形
	R.left = 0;
	R.top = 0;
	R.right = 1280;
	R.bottom = 720;
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);	//根据窗口的客户区大小计算窗口的大小
	int width = R.right - R.left;
	int hight = R.bottom - R.top;

	//创建窗口,返回布尔值
	mhMainWnd = CreateWindow(L"MainWnd", L"DX12Initialize", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, hight, 0, 0, hInstance, 0);
	//窗口创建失败
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreatWindow Failed", 0, 0);
		return 0;
	}
	//窗口创建成功,则显示并更新窗口
	ShowWindow(mhMainWnd, nShowCmd);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3D12App::InitDirect3D()
{
	/*开启D3D12调试层*/
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
	OnResize();

	return true;
}

D3D12App* D3D12App::GetApp()
{
	return mApp;
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
	msaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//UNORM是归一化处理的无符号整数
	msaaQualityLevels.SampleCount = 1;
	msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msaaQualityLevels.NumQualityLevels = 0;
	//当前图形驱动对MSAA多重采样的支持（注意：第二个参数即是输入又是输出）
	ThrowIfFailed(d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQualityLevels, sizeof(msaaQualityLevels)));
	//NumQualityLevels在Check函数里会进行设置
	//如果支持MSAA，则Check函数返回的NumQualityLevels > 0
	//expression为假（即为0），则终止程序运行，并打印一条出错信息
	assert(msaaQualityLevels.NumQualityLevels > 0);
}

void D3D12App::CreateCommandObject()
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&cmdQueue)));

	ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator)));

	ThrowIfFailed(d3dDevice->CreateCommandList(0, // 掩码值为0， 单GPU
		D3D12_COMMAND_LIST_TYPE_DIRECT, // 命令列表类型
		cmdAllocator.Get(), // 命令分配器接口指针
		nullptr, // 流水线状态PSO, 这里不绘制, 所以会空指针
		IID_PPV_ARGS(&cmdList))); // 返回创建的命令列表

	cmdList->Close(); // 重置命令列表前必须将其关闭
}

void D3D12App::CreateSwapChain()
{
	swapChain.Reset();
	DXGI_SWAP_CHAIN_DESC swapChainDesc;	//交换链描述结构体
	swapChainDesc.BufferDesc.Width = 1280;	//缓冲区分辨率的宽度
	swapChainDesc.BufferDesc.Height = 720;	//缓冲区分辨率的高度
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//缓冲区的显示格式
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;	//刷新率的分子
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;	//刷新率的分母
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;	//逐行扫描VS隔行扫描(未指定的)
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	//图像相对屏幕的拉伸（未指定的）
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	//将数据渲染至后台缓冲区（即作为渲染目标）
	swapChainDesc.OutputWindow = mhMainWnd;	//渲染窗口句柄
	swapChainDesc.SampleDesc.Count = 1;	//多重采样数量
	swapChainDesc.SampleDesc.Quality = 0;	//多重采样质量
	swapChainDesc.Windowed = true;	//是否窗口化
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	//固定写法
	swapChainDesc.BufferCount = 2;	//后台缓冲区数量（双缓冲）
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	//自适应窗口模式（自动选择最适于当前窗口尺寸的显示模式）

	  //利用DXGI接口下的工厂类创建交换链
	ThrowIfFailed(dxgiFactory->CreateSwapChain(cmdQueue.Get(), &swapChainDesc, swapChain.GetAddressOf()));
}

void D3D12App::CreateDescriptorHeap()
{
	// 首先创建RTV堆
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvHeap)));

	//然后创建DSV堆
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
		//获得存于交换链中的后台缓冲区资源
		swapChain->GetBuffer(i, IID_PPV_ARGS(swapChainBuffer[i].GetAddressOf()));
		//创建RTV
		d3dDevice->CreateRenderTargetView(swapChainBuffer[i].Get(),
			nullptr,	//在交换链创建中已经定义了该资源的数据格式，所以这里指定为空指针
			rtvHeapHandle);	//描述符句柄结构体（这里是变体，继承自CD3DX12_CPU_DESCRIPTOR_HANDLE）
		//偏移到描述符堆中的下一个缓冲区
		rtvHeapHandle.Offset(1, rtvDescriptorSize);
	}
}

void D3D12App::CreateDSV()
{
	//assert(d3dDevice);
	//assert(swapChain);
	//assert(cmdAllocator);

	//// Flush before changing any resources.
	//FlushCmdQueue();

	//ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));

	//在CPU中创建好深度模板数据资源
	D3D12_RESOURCE_DESC dsvResourceDesc;
	dsvResourceDesc.Alignment = 0;	//指定对齐
	dsvResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	//指定资源维度（类型）为TEXTURE2D
	dsvResourceDesc.DepthOrArraySize = 1;	//纹理深度为1
	dsvResourceDesc.Width = mClientWidth;	//资源宽
	dsvResourceDesc.Height = mClientHeight;	//资源高
	dsvResourceDesc.MipLevels = 1;	//MIPMAP层级数量
	dsvResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//指定纹理布局（这里不指定）
	dsvResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	//深度模板资源的Flag
	dsvResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	//24位深度，8位模板,还有个无类型的格式DXGI_FORMAT_R24G8_TYPELESS也可以使用
	dsvResourceDesc.SampleDesc.Count = 1;	//多重采样数量
	dsvResourceDesc.SampleDesc.Quality = 0;	//多重采样质量
	CD3DX12_CLEAR_VALUE optClear;	//清除资源的优化值，提高清除操作的执行速度（CreateCommittedResource函数中传入）
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//24位深度，8位模板,还有个无类型的格式DXGI_FORMAT_R24G8_TYPELESS也可以使用
	optClear.DepthStencil.Depth = 1;	//初始深度值为1
	optClear.DepthStencil.Stencil = 0;	//初始模板值为0
	//创建一个资源和一个堆，并将资源提交至堆中（将深度模板数据提交至GPU显存中）

	ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),	//堆类型为默认堆（不能写入）
		D3D12_HEAP_FLAG_NONE,	//Flag
		&dsvResourceDesc,	//上面定义的DSV资源指针
		D3D12_RESOURCE_STATE_COMMON,	//资源的状态为初始状态
		&optClear,	//上面定义的优化值指针
		IID_PPV_ARGS(&depthStencilBuffer)));	//返回深度模板资源
		//创建DSV(必须填充DSV属性结构体，和创建RTV不同，RTV是通过句柄)
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	d3dDevice->CreateDepthStencilView(depthStencilBuffer.Get(),
		nullptr,	//D3D12_DEPTH_STENCIL_VIEW_DESC类型指针，可填&dsvDesc（见上注释代码），
							//由于在创建深度模板资源时已经定义深度模板数据属性，所以这里可以指定为空指针
		dsvHeap->GetCPUDescriptorHandleForHeapStart());	//DSV句柄

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
	//视口设置
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.Width = static_cast<float>(mClientWidth);
	viewPort.Height = static_cast<float>(mClientHeight);
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;
	//裁剪矩形设置（矩形外的像素都将被剔除）
	//前两个为左上点坐标，后两个为右下点坐标
	scissorRect = {0, 0, mClientWidth, mClientHeight};

}

void D3D12App::FlushCmdQueue()
{
	mCurrentFence++;	//CPU传完命令并关闭后，将当前围栏值+1
	cmdQueue->Signal(fence.Get(), mCurrentFence);	//当GPU处理完CPU传入的命令后，将fence接口中的围栏值+1，即fence->GetCompletedValue()+1
	if (fence->GetCompletedValue() < mCurrentFence)	//如果小于，说明GPU没有处理完所有命令
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");	//创建事件
		fence->SetEventOnCompletion(mCurrentFence, eventHandle);//当围栏达到mCurrentFence值（即执行到Signal（）指令修改了围栏值）时触发的eventHandle事件
		WaitForSingleObject(eventHandle, INFINITE);//等待GPU命中围栏，激发事件（阻塞当前线程直到事件触发，注意此Enent需先设置再等待，
							   //如果没有Set就Wait，就死锁了，Set永远不会调用，所以也就没线程可以唤醒这个线程）
		CloseHandle(eventHandle);
	}
}

void D3D12App::CalculateFrameState()
{
	static int frameCnt = 0;	//总帧数
	static float timeElapsed = 0.0f;	//流逝的时间
	frameCnt++;	//每帧++，经过一秒后其即为FPS值
	//调试模块
	/*std::wstring text = std::to_wstring(gt.TotalTime());
	std::wstring windowText = text;
	SetWindowText(mhMainWnd, windowText.c_str());*/
	//判断模块
	if ((gt.TotalTime() - timeElapsed) >= 1.0f)	//一旦>=0，说明刚好过一秒
	{
		float fps = (float)frameCnt;//每秒多少帧
		float mspf = 1000.0f / fps;	//每帧多少毫秒

		std::wstring fpsStr = std::to_wstring(fps);//转为宽字符
		std::wstring mspfStr = std::to_wstring(mspf);
		//将帧数据显示在窗口上
		std::wstring windowText = L"D3D12Init    fps:" + fpsStr + L"    " + L"mspf" + mspfStr;
		SetWindowText(mhMainWnd, windowText.c_str());

		//为计算下一组帧数值而重置
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

LRESULT D3D12App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// 消息处理
	switch (msg)
	{
		// 鼠标按键按下时的触发(左中右)
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			// wParam 位输入的虚拟键代码, lParam为系统反馈的光标信息
			OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		// 鼠标按键抬起时候的触发(左中右)
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		// 鼠标移动的触发
		case WM_MOUSEMOVE:
			OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;

		case WM_SIZE:
			mClientWidth = LOWORD(lParam);
			mClientHeight = HIWORD(lParam);
			if (d3dDevice)
			{
				//如果最小化,则暂停游戏，调整最小化和最大化状态
				if (wParam == SIZE_MINIMIZED)
				{
					mAppPaused = true;
					mMinimized = true;
					mMaximized = false;
				}
				else if (wParam == SIZE_MAXIMIZED)
				{
					mAppPaused = false;
					mMinimized = false;
					mMaximized = true;
					OnResize();
				}
				else if (wParam == SIZE_RESTORED)
				{
					if (mMinimized)
					{
						mAppPaused = false;
						mMinimized = false;
						OnResize();
					}

					else if (mMaximized)
					{
						mAppPaused = false;
						mMaximized = false;
						OnResize();
					}

					else if (mResizing)
					{

					}
					else
					{
						OnResize();
					}
				}
			}

			return 0;
		// 当窗口被销毁时, 终止循环
		case WM_DESTROY:
			PostQuitMessage(0); // 终止消息循环, 并发出WM_QUIT消息
			return 0;
		default:
			break;
	}
	// 将上面没有处理的消息转发给默认的窗口过程
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

//窗口过程函数
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return D3D12App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}