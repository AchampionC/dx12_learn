
#include "ToolFunc.h"
#include "d3dx12.h"
#include "GameTime.h"
#include "D3D12App.h"

using namespace Microsoft::WRL;



class D3D12InitApp : public D3D12App
{
public:
	D3D12InitApp() {};
	~D3D12InitApp() {};
private:
	virtual void Draw() override;
};

void D3D12InitApp::Draw()
{
	ThrowIfFailed(cmdAllocator->Reset());//�ظ�ʹ�ü�¼���������ڴ�
	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));//���������б����ڴ�
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



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		D3D12InitApp theApp;
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

