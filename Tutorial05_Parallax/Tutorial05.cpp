﻿//--------------------------------------------------------------------------------------
// File: Tutorial05.cpp
//
// This application demonstrates animation using matrix transformations
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include "resource.h"
#include <dinput.h>
#include <vector>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#pragma comment (lib, "dinput8.lib")

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;

	XMFLOAT3 Normal;
	XMFLOAT2 UV;
	XMFLOAT3 Tangent;
};

struct Light
{
	Light()
	{
		ZeroMemory(this, sizeof(Light));
	}

	XMFLOAT3 dir;
	float zz; // 内存对齐
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
};

Light g_Light;


struct ConstantBuffer
{
	XMMATRIX  mWVP;
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMFLOAT4 mCamPos;
	float  BaseTextureRepeat;
	float  HeightMapScale;
	float f1;
	float f2;
	Light  light;
};



//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE               g_hInst = NULL;
HWND                    g_hWnd = NULL;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = NULL;
ID3D11DeviceContext*    g_pImmediateContext = NULL;

IDXGISwapChain*         g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11Texture2D*        g_pDepthStencil = NULL;
ID3D11DepthStencilView* g_pDepthStencilView = NULL;
ID3D11VertexShader*     g_pNormalMapVertexShader = NULL;
ID3D11PixelShader*      g_pNormalMapPixelShader = NULL;
ID3D11InputLayout*      g_pVertexLayout = NULL;
ID3D11Buffer*           g_pVertexBuffer = NULL;
ID3D11Buffer*			g_pTangentsBuffer = NULL;
ID3D11Buffer*           g_pIndexBuffer = NULL;
ID3D11Buffer*           g_pConstantBuffer = NULL;
XMMATRIX                g_CubeWorld1;
XMMATRIX                g_CubeWorld2;
XMMATRIX                g_View;
XMMATRIX                g_Projection;
XMMATRIX				g_WVP;
XMMATRIX				g_SkyBoxWorld;
ID3D11SamplerState* g_CubesTexSamplerState;
ID3D11RasterizerState* g_CWcullMode;
ID3D11DepthStencilState* g_DSLessEqual;
ID3D11RasterizerState* g_RSCullNone;
ID3D11ShaderResourceView* g_pTextureRV = NULL;
ID3D11ShaderResourceView* g_pNormalMapRV = NULL;

LPDIRECTINPUT8 g_DirectInput;
IDirectInputDevice8* g_DIMouse;
DIMOUSESTATE g_MouseLastState;
IDirectInputDevice8* g_DIKeyboard;

XMVECTOR g_CamPosition;
XMVECTOR g_CamTarget;
XMVECTOR g_CamUp;
XMVECTOR g_DefaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR g_DefaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR g_CamForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR g_CamRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
XMMATRIX g_CamRotationMatrix;
XMMATRIX g_GroundWorld;

float g_MoveLeftRight = 0.0f;
float g_MoveBackForward = 0.0f;
float g_MoveUpDown = 0.0f;

float g_CamYaw = 0.0f;
float g_CamPitch = 0.0f;


double g_FrameTime = 0;
double g_CurrentTime = 0;
double g_LastTime = 0;
DWORD g_StartTick = 0;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();
void LoadSkyMapAndCreateState();

HRESULT InitDirectInput(HINSTANCE hInstance);

void DetectInput(double time);
void UpdateTime();
void UpdateCamera();
HRESULT CompileAndCreateVertexShader(LPCSTR entry, ID3DBlob*& pVSBlob, ID3D11VertexShader*& vs);
HRESULT CompileAndCreatePixelShader(LPCSTR entry, ID3DBlob*& pVSBlob, ID3D11PixelShader*& ps);
bool LoadMesh(std::string& pFile, SimpleVertex** vertexes, int* verticesNum, WORD** indices, int* indicesNum, int* traingalNum);

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	if (FAILED(InitDirectInput(hInstance)))
	{
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{

			UpdateTime();
			DetectInput(g_FrameTime);
			UpdateCamera();
			Render();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}




//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 5", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

int g_IndicesNum;

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// Compile the vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = CompileAndCreateVertexShader("NORMALMAP_VS", pVSBlob, g_pNormalMapVertexShader);

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",	 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT ,0, 32 , D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Compile the pixel shader
	ID3DBlob* pPSBlob = NULL;
	hr = CompileAndCreatePixelShader("NORMALMAP_PS", pPSBlob, g_pNormalMapPixelShader);

	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	std::string meshPath("Box.fbx");
	SimpleVertex* cubeVertices = NULL;
	int vertNum(24), triNum;
	WORD* indices = NULL;
	int indicesNum = 36;
	LoadMesh(meshPath, &cubeVertices, &vertNum, &indices, &indicesNum, &triNum);
	g_IndicesNum = indicesNum;
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * vertNum;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = cubeVertices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Create index buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * indicesNum;        // 36 vertices needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;

	// Set index buffer
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the constant buffer

	g_Light.dir = XMFLOAT3(1.0f, 0.0f, 0.0f);
	g_Light.zz = 0.0f;
	g_Light.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	g_Light.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pConstantBuffer);
	if (FAILED(hr))
		return hr;
	// Initialize the world matrix
	g_CubeWorld1 = XMMatrixIdentity();
	g_CubeWorld2 = XMMatrixIdentity();
	//Camera
	g_CamPosition = XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f);
	g_CamTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	g_CamUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	//Set the View matrix
	g_View = XMMatrixLookAtLH(g_CamPosition, g_CamTarget, g_CamUp);
	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f);


	LoadSkyMapAndCreateState();
	

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext)
		g_pImmediateContext->ClearState();

	if (g_pConstantBuffer)
		g_pConstantBuffer->Release();
	if (g_pVertexBuffer)
		g_pVertexBuffer->Release();
	if (g_pIndexBuffer)
		g_pIndexBuffer->Release();
	if (g_pVertexLayout)
		g_pVertexLayout->Release();
	if (g_pNormalMapVertexShader)
		g_pNormalMapVertexShader->Release();
	if (g_pNormalMapPixelShader)
		g_pNormalMapPixelShader->Release();
	if (g_pDepthStencil)
		g_pDepthStencil->Release();
	if (g_pDepthStencilView)
		g_pDepthStencilView->Release();
	if (g_pRenderTargetView)
		g_pRenderTargetView->Release();
	if (g_pSwapChain)
		g_pSwapChain->Release();
	if (g_pImmediateContext)
		g_pImmediateContext->Release();
	if (g_pd3dDevice)
		g_pd3dDevice->Release();
	if (g_RSCullNone)
		g_RSCullNone->Release();
	if (g_pTextureRV)
		g_pTextureRV->Release();
	if (g_pNormalMapRV)
		g_pNormalMapRV->Release();

}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	//Set the default blend state (no blending) for opaque objects
	g_pImmediateContext->OMSetBlendState(0, 0, 0xffffffff);


	//
	// Clear the back buffer
	//
	float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; //red, green, blue, alpha
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	//
	// Clear the depth buffer to 1.0 (max depth)
	//
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	XMMATRIX mScale = XMMatrixScaling(0.05f, 0.05f, 0.05f);
	mScale = XMMatrixIdentity();
	// 1st Cube: Rotate around the origin
	//g_CubeWorld1 = mScale*XMMatrixRotationZ(3.1415926f*0.5f);
	g_CubeWorld1 = mScale*XMMatrixRotationRollPitchYaw(0, 0, g_CurrentTime);

	//g_CubeWorld1 = XMMatrixRotationY(0);
	//
	// Update variables for the first cube
	//
	ConstantBuffer cbCube1;
	
	//g_CubeWorld1 = mScale;
	cbCube1.mWorld = XMMatrixTranspose(g_CubeWorld1);
	g_WVP = g_CubeWorld1 * g_View * g_Projection;
	cbCube1.mWVP = XMMatrixTranspose(g_WVP);
	//XMVECTOR aa = XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f);
	cbCube1.light = g_Light;
	cbCube1.light.ambient = g_Light.ambient;
	XMStoreFloat4(&cbCube1.mCamPos, g_CamPosition);
	cbCube1.mView = g_View;
	cbCube1.BaseTextureRepeat = 1.0f;
	cbCube1.HeightMapScale = 0.2f;

	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cbCube1, 0, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);

	//
	// Render the first cube
	//
	g_pImmediateContext->PSSetSamplers(0, 1, &g_CubesTexSamplerState);
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_pTextureRV);
	g_pImmediateContext->PSSetShaderResources(1, 1, &g_pNormalMapRV);
	g_pImmediateContext->RSSetState(g_CWcullMode);
	g_pImmediateContext->VSSetShader(g_pNormalMapVertexShader, NULL, 0);
	g_pImmediateContext->PSSetShader(g_pNormalMapPixelShader, NULL, 0);
	g_pImmediateContext->DrawIndexed(g_IndicesNum, 0, 0);


		g_pSwapChain->Present(0, 0);
}

void UpdateTime()
{
	DWORD curTick = GetTickCount();
	if (g_StartTick == 0)
		g_StartTick = curTick;
	g_CurrentTime = (curTick - g_StartTick) / 2000.0f;
	g_FrameTime = g_CurrentTime - g_LastTime;
	g_LastTime = g_CurrentTime;
}

void UpdateCamera()
{
	g_CamRotationMatrix = XMMatrixRotationRollPitchYaw(g_CamPitch, g_CamYaw, 0);
	g_CamTarget = XMVector3TransformCoord(g_DefaultForward, g_CamRotationMatrix);
	g_CamTarget = XMVector3Normalize(g_CamTarget);

	XMMATRIX RotateYTempMatrix;
	RotateYTempMatrix = XMMatrixRotationY(g_CamYaw);

	g_CamRight = XMVector3TransformCoord(g_DefaultRight, RotateYTempMatrix);
	g_CamUp = XMVector3TransformCoord(g_CamUp, RotateYTempMatrix);
	g_CamForward = XMVector3TransformCoord(g_DefaultForward, g_CamRotationMatrix);

	g_CamPosition += g_MoveLeftRight*g_CamRight;
	g_CamPosition += g_MoveBackForward*g_CamForward;
	g_CamPosition += g_MoveUpDown * g_CamUp;

	g_MoveLeftRight = 0.0f;
	g_MoveBackForward = 0.0f;
	g_MoveUpDown = 0.0f;
	g_CamTarget = g_CamPosition + g_CamTarget;

	g_View = XMMatrixLookAtLH(g_CamPosition, g_CamTarget, g_CamUp);

}


HRESULT InitDirectInput(HINSTANCE hInstance)
{
	HRESULT hr = S_OK;

	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&g_DirectInput,
		NULL);

	hr = g_DirectInput->CreateDevice(GUID_SysKeyboard,
		&g_DIKeyboard,
		NULL);

	hr = g_DirectInput->CreateDevice(GUID_SysMouse,
		&g_DIMouse,
		NULL);

	hr = g_DIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = g_DIKeyboard->SetCooperativeLevel(g_hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	hr = g_DIMouse->SetDataFormat(&c_dfDIMouse);
	hr = g_DIMouse->SetCooperativeLevel(g_hWnd, DISCL_EXCLUSIVE | DISCL_NOWINKEY | DISCL_FOREGROUND);

	return hr;
}

void DetectInput(double time)
{
	DIMOUSESTATE mouseCurrState;

	BYTE keyboardState[256];

	g_DIKeyboard->Acquire();
	g_DIMouse->Acquire();

	g_DIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);

	g_DIKeyboard->GetDeviceState(sizeof(keyboardState), (LPVOID)&keyboardState);

	if (keyboardState[DIK_ESCAPE] & 0x80)
		PostMessage(g_hWnd, WM_DESTROY, 0, 0);

	float speed = 3.5f * time;

	if (keyboardState[DIK_A] & 0x80)
	{
		g_MoveLeftRight -= speed;
	}
	if (keyboardState[DIK_D] & 0x80)
	{
		g_MoveLeftRight += speed;
	}
	if (keyboardState[DIK_W] & 0x80)
	{
		g_MoveBackForward += speed;
	}
	if (keyboardState[DIK_S] & 0x80)
	{
		g_MoveBackForward -= speed;
	}
	if (keyboardState[DIK_Q] & 0x80)
	{
		g_MoveUpDown -= speed;
	}
	if (keyboardState[DIK_E] & 0x80)
	{
		g_MoveUpDown += speed;
	}
	if (keyboardState[DIK_R] & 0x80)
	{
		g_MoveUpDown = 0;
		g_MoveLeftRight = 0;
		g_MoveBackForward = 0;
		g_MoveBackForward = 0;
		g_CamYaw = 0;
		g_CamPitch = 0;
	}

	if ((mouseCurrState.lX != g_MouseLastState.lX) || (mouseCurrState.lY != g_MouseLastState.lY))
	{
		g_CamYaw += g_MouseLastState.lX * 0.001f;

		g_CamPitch += mouseCurrState.lY * 0.001f;

		g_MouseLastState = mouseCurrState;
	}

	UpdateCamera();

	return;
}

void LoadSkyMapAndCreateState()
{
	HRESULT hr = S_OK;


	D3D11_RENDER_TARGET_BLEND_DESC rtbd;
	ZeroMemory(&rtbd, sizeof(rtbd));

	rtbd.BlendEnable = true;
	rtbd.SrcBlend = D3D11_BLEND_SRC_COLOR;
	rtbd.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.BlendOp = D3D11_BLEND_OP_ADD;
	rtbd.SrcBlendAlpha = D3D11_BLEND_ONE;
	rtbd.DestBlendAlpha = D3D11_BLEND_ZERO;
	rtbd.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	rtbd.RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;


	D3DX11_IMAGE_LOAD_INFO loadSMInfo;
	loadSMInfo.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	//Load the 2d texture
	// Load the Texture
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"seafloor.dds", NULL, NULL, &g_pTextureRV, NULL);
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, L"four_NM_height.png", NULL, NULL, &g_pNormalMapRV, NULL);


	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV =  D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_CubesTexSamplerState);

	D3D11_RASTERIZER_DESC cmdesc;

	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_BACK;

	cmdesc.FrontCounterClockwise = false;

	hr = g_pd3dDevice->CreateRasterizerState(&cmdesc, &g_CWcullMode);


	cmdesc.CullMode = D3D11_CULL_NONE;
	hr = g_pd3dDevice->CreateRasterizerState(&cmdesc, &g_RSCullNone);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = true;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	g_pd3dDevice->CreateDepthStencilState(&dssDesc, &g_DSLessEqual);
}

HRESULT CompileAndCreateVertexShader(LPCSTR entry, ID3DBlob*& pVSBlob, ID3D11VertexShader*& vs)
{
	HRESULT hr = S_OK;

	hr = CompileShaderFromFile(L"Tutorial05.fx", entry, "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &vs);

	return hr;
}

HRESULT CompileAndCreatePixelShader(LPCSTR entry, ID3DBlob*& pPSBlob, ID3D11PixelShader*& ps)
{
	HRESULT hr = S_OK;

	hr = CompileShaderFromFile(L"Tutorial05.fx", entry, "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &ps);

	return hr;
}

bool LoadMesh(std::string& pFile,SimpleVertex** vertexes,int* verticesNum,WORD** indices,int* indicesNum,int* traingalNum)
{
	Assimp::Importer importer;
	// And have it read the given file with some example postprocessing
	// Usually - if speed is not the most important aspect for you - you'll 
	// propably to request more postprocessing than we do in this example.
	const aiScene* scene = importer.ReadFile(pFile,
		aiProcess_CalcTangentSpace |
		//aiProcess_MakeLeftHanded|
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType);
	if (scene->HasMeshes())
	{
		aiMesh* mesh = scene->mMeshes[0];
		*vertexes = new SimpleVertex[mesh->mNumVertices];
		*verticesNum = mesh->mNumVertices;
		for (unsigned i = 0; i < mesh->mNumVertices; ++i)
		{
			(*vertexes)[i].Pos = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			(*vertexes)[i].Normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			(*vertexes)[i].Tangent = XMFLOAT3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
			(*vertexes)[i].UV = XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		
		}

		int sumIdices = 0;
		for (unsigned i = 0;i<mesh->mNumFaces;++i)
		{
			for (unsigned j = 0;j<mesh->mFaces[i].mNumIndices;++j)
			{
				++sumIdices;
			}
		}
		*indicesNum = sumIdices;

		*indices = new WORD[sumIdices];
		int indicesIdx = 0;
		for (unsigned i = 0; i < mesh->mNumFaces; ++i)
		{
			for (unsigned j = 0; j < mesh->mFaces[i].mNumIndices; ++j)
			{
				(*indices)[indicesIdx] = mesh->mFaces[i].mIndices[j];
				++indicesIdx;
			}
		}

		*traingalNum = mesh->mNumFaces;

	}
	return true;
}


