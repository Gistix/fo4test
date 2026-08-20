#pragma once

#include "PCH.h"

#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include <winrt/base.h>

class WrappedResource
{
public:
	WrappedResource(D3D11_TEXTURE2D_DESC a_texDesc, ID3D11Device5* a_d3d11Device, ID3D12Device* a_d3d12Device);

	ID3D11Texture2D* resource11;
	ID3D11ShaderResourceView* srv;
	ID3D11UnorderedAccessView* uav;
	ID3D11RenderTargetView* rtv;
	winrt::com_ptr<ID3D12Resource> resource;
};