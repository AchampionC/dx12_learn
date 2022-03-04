#include "ToolFunc.h"

#include "d3dx12.h"
DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
	ErrorCode(hr),
	FunctionName(functionName),
	Filename(filename),
	LineNumber(lineNumber)
{
}

std::wstring DxException::ToString()const
{
	// Get the string description of the error code.
	_com_error err(ErrorCode);
	std::wstring msg = err.ErrorMessage();

	return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}

ComPtr<ID3DBlob> ToolFunc::CompileShader(const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entryPoint, const std::string& target)
{
	// �����ڵ���ģʽ, ��ʹ�õ��Ա�־
	UINT compileFlags = 0;
	#if defined (DEBUG) || defined(_DEBUG)
	// �õ���ģʽ��������ɫ�� | ָʾ�����������Ż��׶�
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#endif 
	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(fileName.c_str(), //hlslԴ�ļ���
		defines,	//�߼�ѡ�ָ��Ϊ��ָ��
		D3D_COMPILE_STANDARD_FILE_INCLUDE,	//�߼�ѡ�����ָ��Ϊ��ָ��
		entryPoint.c_str(),	//��ɫ������ڵ㺯����
		target.c_str(),		//ָ��������ɫ�����ͺͰ汾���ַ���
		compileFlags,	//ָʾ����ɫ���ϴ���Ӧ����α���ı�־
		0,	//�߼�ѡ��
		&byteCode,	//����õ��ֽ���
		&errors);	//������Ϣ

	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	return byteCode;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ToolFunc::CreateDefaultBuffer(ComPtr<ID3D12Device> d3dDevice, ComPtr<ID3D12GraphicsCommandList> cmdList, UINT64 byteSize, const void* initData, ComPtr<ID3D12Resource>& uploadBuffer)
{
	// �����ϴ���, ������д��CPU�ڴ�����, �������Ĭ�϶�
	ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // �����ϴ������͵Ķ�
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize), // ����Ĺ��캯��, ����byteSize, ������ΪĬ��ֵ, ����д
		D3D12_RESOURCE_STATE_GENERIC_READ, // �ϴ��������Դ��Ҫ���Ƹ�Ĭ�϶�, �����ǿɶ�״̬
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)));

	// ����Ĭ�϶�, ��Ϊ�ϴ��ѵ����ݴ������
	ComPtr<ID3D12Resource> defaultBuffer;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // ����Ĭ�϶����͵Ķ�
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON, //Ĭ�϶�Ϊ���մ洢���ݵĵط���������ʱ��ʼ��Ϊ��ͨ״̬
		nullptr,
		IID_PPV_ARGS(&defaultBuffer)));

	// ����Դ��Common״̬ת����Copy_Dest״̬(Ĭ�϶Ѵ�ʱ��Ϊ�������ݵ�Ŀ��)
	cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_COPY_DEST));

	//�����ݴ�CPU�ڴ濽����GPU����
	D3D12_SUBRESOURCE_DATA subResourceData;
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// ���ĺ���UpdateSubresources, �����ݴ�CPU�ڴ濽�����ϴ���, �ٴ��ϴ��ѿ�����Ĭ�϶�.1����������Դ���±꣨ģ���ж��壬��Ϊ��2������Դ��
	UpdateSubresources<1>(cmdList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

	// �ٽ���Դ��COPY_DEST״̬�л���GENERIC_READ״̬(����ֻ�ṩ����ɫ������)
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ));

	return defaultBuffer;

}
