#pragma once

#include "ToolFunc.h"
#include "GameTime.h"
#include "d3dx12.h"

using namespace Microsoft::WRL;

class D3D12App
{
protected:
	D3D12App(HINSTANCE hInstance);
	virtual ~D3D12App();

public:
	int Run();
	virtual bool Init(HINSTANCE hInstance, int nShowCmd);
	bool InitWindow(HINSTANCE hInstance, int nShowCmd);
	bool InitDirect3D();

	static D3D12App* GetApp();
	virtual void Draw() = 0;
	virtual void Update() = 0;


	void CreateDevice();
	void CreateFence();
	void GetDescriptorSize();
	void SetMSAA();
	void CreateCommandObject();
	void CreateSwapChain();
	void CreateDescriptorHeap();
	void CreateRTV();
	void CreateDSV();
	void CreateViewPortAndScissorRect();

	void FlushCmdQueue();
	void CalculateFrameState();

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:

	virtual void OnResize();
	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }
protected:

	static D3D12App* mApp;
	HINSTANCE mhAppInst = nullptr;

	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled

	HWND mhMainWnd = 0;

	//指针接口和变量声明
	ComPtr<ID3D12Device> d3dDevice;
	ComPtr<IDXGIFactory4> dxgiFactory;
	ComPtr<ID3D12Fence> fence;
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	ComPtr<ID3D12CommandQueue> cmdQueue;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
	ComPtr<ID3D12Resource> depthStencilBuffer;
	ComPtr<ID3D12Resource> swapChainBuffer[2];
	ComPtr<IDXGISwapChain> swapChain;
	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	                                                  
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;

	UINT rtvDescriptorSize = 0;
	UINT dsvDescriptorSize = 0;
	UINT cbv_srv_uavDescriptorSize = 0;

	//GameTime类实例声明
	GameTime gt;

	UINT mCurrentBackBuffer = 0;
	int mCurrentFence = 0;	//初始CPU上的围栏点为0

	int mClientWidth = 800;
	int mClientHeight = 600;
};