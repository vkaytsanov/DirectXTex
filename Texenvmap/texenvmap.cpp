//--------------------------------------------------------------------------------------
// File: Texenvmap.cpp
//
// DirectX Texture environment map tool
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//--------------------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4005)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOMCX
#define NOSERVICE
#define NOHELP
#pragma warning(pop)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <memory>
#include <list>
#include <vector>

#include <wrl/client.h>

#include <d3d11_1.h>
#include <dxgi.h>
#include <dxgiformat.h>

#include <wincodec.h>

#pragma warning(disable : 4619 4616 26812)

#include "DirectXTex.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    enum COMMANDS
    {
        CMD_CUBIC = 1,
        CMD_SPHERE,
        CMD_DUAL_PARABOLA,
        CMD_DUAL_HEMISPHERE,
        CMD_MAX
    };

    enum OPTIONS
    {
        OPT_RECURSIVE = 1,
        OPT_FILELIST,
        OPT_WIDTH,
        OPT_HEIGHT,
        OPT_FORMAT,
        OPT_FILTER,
        OPT_SRGBI,
        OPT_SRGBO,
        OPT_SRGB,
        OPT_OUTPUTFILE,
        OPT_TOLOWER,
        OPT_OVERWRITE,
        OPT_USE_DX10,
        OPT_NOLOGO,
        OPT_SEPALPHA,
        OPT_NO_WIC,
        OPT_DEMUL_ALPHA,
        OPT_TA_WRAP,
        OPT_TA_MIRROR,
        OPT_GPU,
        OPT_MAX
    };

    static_assert(OPT_MAX <= 32, "dwOptions is a DWORD bitfield");

    struct SConversion
    {
        wchar_t szSrc[MAX_PATH];
    };

    struct SValue
    {
        LPCWSTR pName;
        DWORD dwValue;
    };

    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    const SValue g_pCommands[] =
    {
        { L"cubic",         CMD_CUBIC },
        { L"sphere",        CMD_SPHERE },
        { L"parabola",      CMD_DUAL_PARABOLA },
        { L"hemisphere",    CMD_DUAL_HEMISPHERE },
        { nullptr,          0 }
    };

    const SValue g_pOptions[] =
    {
        { L"r",         OPT_RECURSIVE },
        { L"flist",     OPT_FILELIST },
        { L"w",         OPT_WIDTH },
        { L"h",         OPT_HEIGHT },
        { L"f",         OPT_FORMAT },
        { L"if",        OPT_FILTER },
        { L"srgbi",     OPT_SRGBI },
        { L"srgbo",     OPT_SRGBO },
        { L"srgb",      OPT_SRGB },
        { L"o",         OPT_OUTPUTFILE },
        { L"l",         OPT_TOLOWER },
        { L"y",         OPT_OVERWRITE },
        { L"dx10",      OPT_USE_DX10 },
        { L"nologo",    OPT_NOLOGO },
        { L"sepalpha",  OPT_SEPALPHA },
        { L"nowic",     OPT_NO_WIC },
        { L"alpha",     OPT_DEMUL_ALPHA },
        { L"wrap",      OPT_TA_WRAP },
        { L"mirror",    OPT_TA_MIRROR },
        { L"gpu",       OPT_GPU },
        { nullptr,      0 }
    };

#define DEFFMT(fmt) { L## #fmt, DXGI_FORMAT_ ## fmt }

    const SValue g_pFormats[] =
    {
        // List only includes render target supported formats
        DEFFMT(R32G32B32A32_FLOAT),
        DEFFMT(R16G16B16A16_FLOAT),
        DEFFMT(R16G16B16A16_UNORM),
        DEFFMT(R32G32_FLOAT),
        DEFFMT(R10G10B10A2_UNORM),
        DEFFMT(R11G11B10_FLOAT),
        DEFFMT(R8G8B8A8_UNORM),
        DEFFMT(R8G8B8A8_UNORM_SRGB),
        DEFFMT(R16G16_FLOAT),
        DEFFMT(R16G16_UNORM),
        DEFFMT(R32_FLOAT),
        DEFFMT(R8G8_UNORM),
        DEFFMT(R16_FLOAT),
        DEFFMT(R16_UNORM),
        DEFFMT(R8_UNORM),
        DEFFMT(R8_UINT),
        DEFFMT(A8_UNORM),
        DEFFMT(B5G6R5_UNORM),
        DEFFMT(B8G8R8A8_UNORM),
        DEFFMT(B8G8R8A8_UNORM_SRGB),
        { nullptr, DXGI_FORMAT_UNKNOWN }
    };

    const SValue g_pFormatAliases[] =
    {
        { L"RGBA", DXGI_FORMAT_R8G8B8A8_UNORM },
        { L"BGRA", DXGI_FORMAT_B8G8R8A8_UNORM },

        { L"FP16", DXGI_FORMAT_R16G16B16A16_FLOAT },
        { L"FP32", DXGI_FORMAT_R32G32B32A32_FLOAT },

        { nullptr, DXGI_FORMAT_UNKNOWN }
    };

    const SValue g_pFilters[] =
    {
        { L"POINT",                     TEX_FILTER_POINT },
        { L"LINEAR",                    TEX_FILTER_LINEAR },
        { L"CUBIC",                     TEX_FILTER_CUBIC },
        { L"FANT",                      TEX_FILTER_FANT },
        { L"BOX",                       TEX_FILTER_BOX },
        { L"TRIANGLE",                  TEX_FILTER_TRIANGLE },
        { L"POINT_DITHER",              TEX_FILTER_POINT | TEX_FILTER_DITHER },
        { L"LINEAR_DITHER",             TEX_FILTER_LINEAR | TEX_FILTER_DITHER },
        { L"CUBIC_DITHER",              TEX_FILTER_CUBIC | TEX_FILTER_DITHER },
        { L"FANT_DITHER",               TEX_FILTER_FANT | TEX_FILTER_DITHER },
        { L"BOX_DITHER",                TEX_FILTER_BOX | TEX_FILTER_DITHER },
        { L"TRIANGLE_DITHER",           TEX_FILTER_TRIANGLE | TEX_FILTER_DITHER },
        { L"POINT_DITHER_DIFFUSION",    TEX_FILTER_POINT | TEX_FILTER_DITHER_DIFFUSION },
        { L"LINEAR_DITHER_DIFFUSION",   TEX_FILTER_LINEAR | TEX_FILTER_DITHER_DIFFUSION },
        { L"CUBIC_DITHER_DIFFUSION",    TEX_FILTER_CUBIC | TEX_FILTER_DITHER_DIFFUSION },
        { L"FANT_DITHER_DIFFUSION",     TEX_FILTER_FANT | TEX_FILTER_DITHER_DIFFUSION },
        { L"BOX_DITHER_DIFFUSION",      TEX_FILTER_BOX | TEX_FILTER_DITHER_DIFFUSION },
        { L"TRIANGLE_DITHER_DIFFUSION", TEX_FILTER_TRIANGLE | TEX_FILTER_DITHER_DIFFUSION },
        { nullptr,                      TEX_FILTER_DEFAULT                              }
    };

#define CODEC_DDS 0xFFFF0001
#define CODEC_TGA 0xFFFF0002
#define CODEC_HDR 0xFFFF0005

    const SValue g_pExtFileTypes[] =
    {
        { L".BMP",  WIC_CODEC_BMP },
        { L".JPG",  WIC_CODEC_JPEG },
        { L".JPEG", WIC_CODEC_JPEG },
        { L".PNG",  WIC_CODEC_PNG },
        { L".DDS",  CODEC_DDS },
        { L".TGA",  CODEC_TGA },
        { L".HDR",  CODEC_HDR },
        { L".TIF",  WIC_CODEC_TIFF },
        { L".TIFF", WIC_CODEC_TIFF },
        { L".WDP",  WIC_CODEC_WMP },
        { L".HDP",  WIC_CODEC_WMP },
        { L".JXR",  WIC_CODEC_WMP },
        { nullptr,  CODEC_DDS }
    };
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

namespace
{
#include "Shaders/Compiled/Texenvmap_VSBasic.inc"
#include "Shaders/Compiled/Texenvmap_PSBasic.inc"
#include "Shaders/Compiled/Texenvmap_PSEquiRect.inc"
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

namespace
{
    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    struct find_closer { void operator()(HANDLE h) noexcept { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

    using ScopedFindHandle = std::unique_ptr<void, find_closer>;

#ifdef _PREFAST_
#pragma prefast(disable : 26018, "Only used with static internal arrays")
#endif

    DWORD LookupByName(const wchar_t* pName, const SValue* pArray)
    {
        while (pArray->pName)
        {
            if (!_wcsicmp(pName, pArray->pName))
                return pArray->dwValue;

            pArray++;
        }

        return 0;
    }


    void SearchForFiles(const wchar_t* path, std::list<SConversion>& files, bool recursive)
    {
        // Process files
        WIN32_FIND_DATAW findData = {};
        ScopedFindHandle hFile(safe_handle(FindFirstFileExW(path,
            FindExInfoBasic, &findData,
            FindExSearchNameMatch, nullptr,
            FIND_FIRST_EX_LARGE_FETCH)));
        if (hFile)
        {
            for (;;)
            {
                if (!(findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY)))
                {
                    wchar_t drive[_MAX_DRIVE] = {};
                    wchar_t dir[_MAX_DIR] = {};
                    _wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);

                    SConversion conv;
                    _wmakepath_s(conv.szSrc, drive, dir, findData.cFileName, nullptr);
                    files.push_back(conv);
                }

                if (!FindNextFileW(hFile.get(), &findData))
                    break;
            }
        }

        // Process directories
        if (recursive)
        {
            wchar_t searchDir[MAX_PATH] = {};
            {
                wchar_t drive[_MAX_DRIVE] = {};
                wchar_t dir[_MAX_DIR] = {};
                _wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
                _wmakepath_s(searchDir, drive, dir, L"*", nullptr);
            }

            hFile.reset(safe_handle(FindFirstFileExW(searchDir,
                FindExInfoBasic, &findData,
                FindExSearchLimitToDirectories, nullptr,
                FIND_FIRST_EX_LARGE_FETCH)));
            if (!hFile)
                return;

            for (;;)
            {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (findData.cFileName[0] != L'.')
                    {
                        wchar_t subdir[MAX_PATH] = {};

                        {
                            wchar_t drive[_MAX_DRIVE] = {};
                            wchar_t dir[_MAX_DIR] = {};
                            wchar_t fname[_MAX_FNAME] = {};
                            wchar_t ext[_MAX_FNAME] = {};
                            _wsplitpath_s(path, drive, dir, fname, ext);
                            wcscat_s(dir, findData.cFileName);
                            _wmakepath_s(subdir, drive, dir, fname, ext);
                        }

                        SearchForFiles(subdir, files, recursive);
                    }
                }

                if (!FindNextFileW(hFile.get(), &findData))
                    break;
            }
        }
    }


    void PrintFormat(DXGI_FORMAT Format)
    {
        for (const SValue* pFormat = g_pFormats; pFormat->pName; pFormat++)
        {
            if (static_cast<DXGI_FORMAT>(pFormat->dwValue) == Format)
            {
                wprintf(L"%ls", pFormat->pName);
                break;
            }
        }
    }


    void PrintInfo(const TexMetadata& info)
    {
        wprintf(L" (%zux%zu", info.width, info.height);

        if (TEX_DIMENSION_TEXTURE3D == info.dimension)
            wprintf(L"x%zu", info.depth);

        if (info.mipLevels > 1)
            wprintf(L",%zu", info.mipLevels);

        if (info.arraySize > 1)
            wprintf(L",%zu", info.arraySize);

        wprintf(L" ");
        PrintFormat(info.format);

        switch (info.dimension)
        {
        case TEX_DIMENSION_TEXTURE1D:
            wprintf(L"%ls", (info.arraySize > 1) ? L" 1DArray" : L" 1D");
            break;

        case TEX_DIMENSION_TEXTURE2D:
            if (info.IsCubemap())
            {
                wprintf(L"%ls", (info.arraySize > 6) ? L" CubeArray" : L" Cube");
            }
            else
            {
                wprintf(L"%ls", (info.arraySize > 1) ? L" 2DArray" : L" 2D");
            }
            break;

        case TEX_DIMENSION_TEXTURE3D:
            wprintf(L" 3D");
            break;
        }

        switch (info.GetAlphaMode())
        {
        case TEX_ALPHA_MODE_OPAQUE:
            wprintf(L" \x0e0:Opaque");
            break;
        case TEX_ALPHA_MODE_PREMULTIPLIED:
            wprintf(L" \x0e0:PM");
            break;
        case TEX_ALPHA_MODE_STRAIGHT:
            wprintf(L" \x0e0:NonPM");
            break;
        case TEX_ALPHA_MODE_CUSTOM:
            wprintf(L" \x0e0:Custom");
            break;
        case TEX_ALPHA_MODE_UNKNOWN:
            break;
        }

        wprintf(L")");
    }


    void PrintList(size_t cch, const SValue* pValue)
    {
        while (pValue->pName)
        {
            size_t cchName = wcslen(pValue->pName);

            if (cch + cchName + 2 >= 80)
            {
                wprintf(L"\n      ");
                cch = 6;
            }

            wprintf(L"%ls ", pValue->pName);
            cch += cchName + 2;
            pValue++;
        }

        wprintf(L"\n");
    }


    void PrintLogo()
    {
        wprintf(L"Microsoft (R) DirectX Environment Map Tool (DirectXTex version)\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }


    _Success_(return != false)
        bool GetDXGIFactory(_Outptr_ IDXGIFactory1 * *pFactory) noexcept
    {
        if (!pFactory)
            return false;

        *pFactory = nullptr;

        typedef HRESULT(WINAPI* pfn_CreateDXGIFactory1)(REFIID riid, _Out_ void** ppFactory);

        static pfn_CreateDXGIFactory1 s_CreateDXGIFactory1 = nullptr;

        if (!s_CreateDXGIFactory1)
        {
            HMODULE hModDXGI = LoadLibraryW(L"dxgi.dll");
            if (!hModDXGI)
                return false;

            s_CreateDXGIFactory1 = reinterpret_cast<pfn_CreateDXGIFactory1>(reinterpret_cast<void*>(GetProcAddress(hModDXGI, "CreateDXGIFactory1")));
            if (!s_CreateDXGIFactory1)
                return false;
        }

        return SUCCEEDED(s_CreateDXGIFactory1(IID_PPV_ARGS(pFactory)));
    }


    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: texenvmap <command> <options> <files>\n\n");
        wprintf(L"   cubic               create cubic environment map\n");
        wprintf(L"   sphere              create sphere environment map\n");
        wprintf(L"   parabola            create dual parabolic environment map\n");
        wprintf(L"   hemisphere          create dual hemisphere environment map\n\n");

        wprintf(L"   -r                  wildcard filename search is recursive\n");
        wprintf(L"   -flist <filename>   use text file with a list of input files (one per line)\n");
        wprintf(L"   -w <n>              width\n");
        wprintf(L"   -h <n>              height\n");
        wprintf(L"   -f <format>         format\n");
        wprintf(L"   -if <filter>        image filtering\n");
        wprintf(L"   -srgb{i|o}          sRGB {input, output}\n");
        wprintf(L"   -o <filename>       output filename\n");
        wprintf(L"   -l                  force output filename to lower case\n");
        wprintf(L"   -y                  overwrite existing output file (if any)\n");
        wprintf(L"   -sepalpha           resize alpha channel separately from color channels\n");
        wprintf(L"   -nowic              Force non-WIC filtering\n");
        wprintf(L"   -wrap, -mirror      texture addressing mode (wrap, mirror, or clamp)\n");
        wprintf(L"   -alpha              convert premultiplied alpha to straight alpha\n");
        wprintf(L"   -dx10               Force use of 'DX10' extended header\n");
        wprintf(L"   -nologo             suppress copyright message\n");
        wprintf(L"   -gpu <adapter>      Select GPU for DirectCompute-based codecs (0 is default)\n");

        wprintf(L"\n   <format>: ");
        PrintList(13, g_pFormats);
        wprintf(L"      ");
        PrintList(13, g_pFormatAliases);

        wprintf(L"\n   <filter>: ");
        PrintList(13, g_pFilters);

        ComPtr<IDXGIFactory1> dxgiFactory;
        if (GetDXGIFactory(dxgiFactory.GetAddressOf()))
        {
            wprintf(L"\n   <adapter>:\n");

            ComPtr<IDXGIAdapter> adapter;
            for (UINT adapterIndex = 0;
                SUCCEEDED(dxgiFactory->EnumAdapters(adapterIndex, adapter.ReleaseAndGetAddressOf()));
                ++adapterIndex)
            {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc)))
                {
                    wprintf(L"      %u: VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                }
            }
        }
    }


    _Success_(return != false)
        bool CreateDevice(int adapter, _Outptr_ ID3D11Device** pDevice) noexcept
    {
        if (!pDevice)
            return false;

        *pDevice = nullptr;

        static PFN_D3D11_CREATE_DEVICE s_DynamicD3D11CreateDevice = nullptr;

        if (!s_DynamicD3D11CreateDevice)
        {
            HMODULE hModD3D11 = LoadLibraryW(L"d3d11.dll");
            if (!hModD3D11)
                return false;

            s_DynamicD3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(reinterpret_cast<void*>(GetProcAddress(hModD3D11, "D3D11CreateDevice")));
            if (!s_DynamicD3D11CreateDevice)
                return false;
        }

        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        ComPtr<IDXGIAdapter> pAdapter;
        if (adapter >= 0)
        {
            ComPtr<IDXGIFactory1> dxgiFactory;
            if (GetDXGIFactory(dxgiFactory.GetAddressOf()))
            {
                if (FAILED(dxgiFactory->EnumAdapters(static_cast<UINT>(adapter), pAdapter.GetAddressOf())))
                {
                    wprintf(L"\nERROR: Invalid GPU adapter index (%d)!\n", adapter);
                    return false;
                }
            }
        }

        D3D_FEATURE_LEVEL fl;
        HRESULT hr = s_DynamicD3D11CreateDevice(pAdapter.Get(),
            (pAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
            nullptr, createDeviceFlags, featureLevels, _countof(featureLevels),
            D3D11_SDK_VERSION, pDevice, &fl, nullptr);
        if (FAILED(hr))
        {
            hr = s_DynamicD3D11CreateDevice(nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr, createDeviceFlags, featureLevels, _countof(featureLevels),
                D3D11_SDK_VERSION, pDevice, &fl, nullptr);
        }

        if (SUCCEEDED(hr))
        {
            ComPtr<IDXGIDevice> dxgiDevice;
            hr = (*pDevice)->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
            if (SUCCEEDED(hr))
            {
                hr = dxgiDevice->GetAdapter(pAdapter.ReleaseAndGetAddressOf());
                if (SUCCEEDED(hr))
                {
                    DXGI_ADAPTER_DESC desc;
                    hr = pAdapter->GetDesc(&desc);
                    if (SUCCEEDED(hr))
                    {
                        wprintf(L"[Using Direct3D on \"%ls\"]\n\n", desc.Description);
                    }
                }
            }

            return true;
        }
        else
            return false;
    }


    struct ConstantBuffer
    {
        XMFLOAT4X4 transform;
    };

    static_assert((sizeof(ConstantBuffer) % 16) == 0, "CB incorrect alignment");

    class Shaders
    {
    public:
        Shaders() = default;

        HRESULT Create(ID3D11Device* device)
        {
            if (!device)
                return E_INVALIDARG;

            m_vertexShader.clear();
            m_pixelShader.clear();

            for (size_t j = 0; j < _countof(s_vs); ++j)
            {
                ComPtr<ID3D11VertexShader> shader;
                HRESULT hr = device->CreateVertexShader(s_vs[j].code, s_vs[j].length, nullptr, shader.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                m_vertexShader.emplace_back(shader);
            }

            for (size_t j = 0; j < _countof(s_ps); ++j)
            {
                ComPtr<ID3D11PixelShader> shader;
                HRESULT hr = device->CreatePixelShader(s_ps[j].code, s_ps[j].length, nullptr, shader.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                m_pixelShader.emplace_back(shader);
            }

            CD3D11_BUFFER_DESC desc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
            HRESULT hr = device->CreateBuffer(&desc, nullptr, m_constantBuffer.ReleaseAndGetAddressOf());
            if (FAILED(hr))
                return hr;

            return S_OK;
        }

        enum VS_INDEX : unsigned int
        {
            VS_BASIC = 0,
        };

        enum PS_INDEX : unsigned int
        {
            PS_BASIC = 0,
            PS_EQUIRECT,
        };

        void Apply(
            unsigned int vsindex,
            unsigned int psindex,
            _In_ ID3D11DeviceContext* deviceContext,
            _In_opt_ ConstantBuffer* cbuffer)
        {
            if ((vsindex >= _countof(s_vs))
                || (psindex >= _countof(s_ps))
                || !deviceContext)
                return;

            deviceContext->VSSetShader(m_vertexShader[vsindex].Get(), nullptr, 0);
            deviceContext->PSSetShader(m_pixelShader[psindex].Get(), nullptr, 0);

            if (cbuffer)
            {
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (SUCCEEDED(deviceContext->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    memcpy(mapped.pData, cbuffer, sizeof(ConstantBuffer));
                    deviceContext->Unmap(m_constantBuffer.Get(), 0);
                }
                auto cb = m_constantBuffer.Get();
                deviceContext->VSSetConstantBuffers(0, 1, &cb);
            }
        }

        void GetVertexShaderBytecode(
            unsigned int vsindex,
            _Out_ void const** pShaderByteCode,
            _Out_ size_t* pByteCodeLength)
        {
            if (pShaderByteCode)
            {
                *pShaderByteCode = nullptr;
            }

            if (pByteCodeLength)
            {
                *pByteCodeLength = 0;
            }

            if (!pShaderByteCode
                || !pByteCodeLength
                || (vsindex >= _countof(s_vs)))
                return;

            *pShaderByteCode = s_vs[vsindex].code;
            *pByteCodeLength = s_vs[vsindex].length;
        }

    private:
        ComPtr<ID3D11Buffer> m_constantBuffer;
        std::vector<ComPtr<ID3D11VertexShader>> m_vertexShader;
        std::vector<ComPtr<ID3D11PixelShader>> m_pixelShader;

        struct ShaderBytecode
        {
            void const* code;
            size_t length;
        };

        const ShaderBytecode s_vs[1] =
        {
            { Texenvmap_VSBasic, sizeof(Texenvmap_VSBasic) },
        };

        const ShaderBytecode s_ps[2] =
        {
            { Texenvmap_PSBasic, sizeof(Texenvmap_PSBasic) },
            { Texenvmap_PSEquiRect, sizeof(Texenvmap_PSEquiRect) },
        };
    };


    class StateObjects
    {
    public:
        StateObjects() = default;

        HRESULT Create(ID3D11Device* device)
        {
            if (!device)
                return E_INVALIDARG;

            {
                D3D11_BLEND_DESC desc = {};
                desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

                desc.RenderTarget[0].BlendEnable = FALSE;
                desc.RenderTarget[0].SrcBlend = desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                desc.RenderTarget[0].DestBlend = desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
                desc.RenderTarget[0].BlendOp = desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

                HRESULT hr = device->CreateBlendState(&desc, m_opaque.ReleaseAndGetAddressOf());
                if (FAILED(hr))
                    return hr;
            }

            {
                D3D11_DEPTH_STENCIL_DESC desc = {};
                desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
                desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
                desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

                desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
                desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

                desc.BackFace = desc.FrontFace;
                HRESULT hr = device->CreateDepthStencilState(&desc, m_depthNone.ReleaseAndGetAddressOf());
                if (FAILED(hr))
                    return hr;
            }

            {
                D3D11_RASTERIZER_DESC desc = {};
                desc.CullMode = D3D11_CULL_NONE;
                desc.FillMode = D3D11_FILL_SOLID;
                desc.DepthClipEnable = TRUE;
                desc.MultisampleEnable = TRUE;

                HRESULT hr = device->CreateRasterizerState(&desc, m_cullNone.ReleaseAndGetAddressOf());
                if (FAILED(hr))
                    return hr;
            }

            {
                D3D11_SAMPLER_DESC desc = {};
                desc.MaxAnisotropy = D3D11_MAX_MAXANISOTROPY;
                desc.MaxLOD = FLT_MAX;
                desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

                desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                HRESULT hr = device->CreateSamplerState(&desc, m_linearClamp.ReleaseAndGetAddressOf());
                if (FAILED(hr))
                    return hr;
            }

            return S_OK;
        }

        ID3D11BlendState* Opaque() const noexcept { return m_opaque.Get(); }
        ID3D11DepthStencilState* DepthNone() const noexcept { return m_depthNone.Get(); }
        ID3D11RasterizerState* CullNone() const noexcept { return m_cullNone.Get(); }
        ID3D11SamplerState* LinearClamp() const noexcept { return m_linearClamp.Get(); }

    private:
        ComPtr<ID3D11BlendState> m_opaque;
        ComPtr<ID3D11DepthStencilState> m_depthNone;
        ComPtr<ID3D11RasterizerState> m_cullNone;
        ComPtr<ID3D11SamplerState> m_linearClamp;
    };


    class RenderTarget
    {
    public:
        RenderTarget() = default;

        HRESULT Create(
            ID3D11Device* device,
            size_t width,
            size_t height,
            DXGI_FORMAT format) noexcept
        {
            texture.Reset();
            srv.Reset();
            rtv.Reset();

            if (!device || !width || !height)
                return E_INVALIDARG;

            if ((width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
                || (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

            HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                hr = device->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf());
                if (FAILED(hr))
                    return hr;
            }

            viewPort.TopLeftX = viewPort.TopLeftY = 0.f;
            viewPort.Width = static_cast<float>(width);
            viewPort.Height = static_cast<float>(height);
            viewPort.MinDepth = D3D11_MIN_DEPTH;
            viewPort.MaxDepth = D3D11_MAX_DEPTH;

            return hr;
        }

        void Begin(ID3D11DeviceContext* context, bool clear = false)
        {
            if (!context)
                return;

            if (clear)
            {
                float black[4] = { 0.f, 0.f, 0.f, 1.f };
                context->ClearRenderTargetView(rtv.Get(), black);
            }

            context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);

            context->RSSetViewports(1, &viewPort);
        }

        void End(ID3D11DeviceContext* context)
        {
            if (!context)
                return;

            ID3D11RenderTargetView* nullrtv = nullptr;
            context->OMSetRenderTargets(1, &nullrtv, nullptr);
        }

        ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }
        ID3D11Texture2D* GetTexture() const { return texture.Get(); }

    private:
        D3D11_VIEWPORT viewPort;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    };


    // Vertex types
    struct VertexPositionTexture
    {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;

        static constexpr unsigned int InputElementCount = 2;
        static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };

    const D3D11_INPUT_ELEMENT_DESC VertexPositionTexture::InputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };


    class UnitCube
    {
    public:
        UnitCube() = default;

        HRESULT Create(_In_ ID3D11Device* device) noexcept
        {
            if (!device)
                return E_INVALIDARG;

            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.ByteWidth = static_cast<UINT>(sizeof(VertexPositionTexture) * nVerts);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = c_cubeVertices;

            HRESULT hr = device->CreateBuffer(&desc, &initData, vertexBuffer.ReleaseAndGetAddressOf());
            if (FAILED(hr))
                return hr;

            desc.ByteWidth = static_cast<UINT>(sizeof(uint16_t) * nFaces * 3);
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            initData.pSysMem = c_cubeIndices;

            return device->CreateBuffer(&desc, &initData, indexBuffer.ReleaseAndGetAddressOf());
        }

        void Draw(_In_ ID3D11DeviceContext* context)
        {
            if (!context)
                return;

            auto vb = vertexBuffer.Get();
            UINT stride = sizeof(VertexPositionTexture);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

            context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            context->DrawIndexed(nFaces * 3, 0, 0);
        }

        HRESULT CreateInputLayout(_In_ ID3D11Device* device, Shaders& shaders, _COM_Outptr_ ID3D11InputLayout** layout)
        {
            if (layout)
            {
                *layout = nullptr;
            }

            if (!device || !layout)
                return E_INVALIDARG;

            const void* code = nullptr;;
            size_t length = 0;
            shaders.GetVertexShaderBytecode(Shaders::VS_BASIC, &code, &length);

            return device->CreateInputLayout(
                VertexPositionTexture::InputElements,
                VertexPositionTexture::InputElementCount,
                code,
                length,
                layout);
        }

    private:
        static constexpr UINT nVerts = 24;
        static const VertexPositionTexture c_cubeVertices[nVerts];

        static constexpr UINT nFaces = 12;
        static const uint16_t c_cubeIndices[nFaces * 3];

        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    };

    const VertexPositionTexture UnitCube::c_cubeVertices[] =
    {
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },

        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
    };

    const uint16_t UnitCube::c_cubeIndices[] =
    {
        3,1,0,
        2,1,3,

        6,4,5,
        7,4,6,

        11,9,8,
        10,9,11,

        14,12,13,
        15,12,14,

        19,17,16,
        18,17,19,

        22,20,21,
        23,20,22
    };


    size_t FitPowerOf2(size_t targetx, size_t maxsize)
    {
        size_t x;
        for (x = maxsize; x > 1; x >>= 1) { if (x <= targetx) break; }
        return x;
    }

    void FitPowerOf2(size_t& targetx, size_t& targety, size_t maxsize)
    {
        float origAR = float(targetx) / float(targety);

        if (targetx > targety)
        {
            size_t x;
            for (x = maxsize; x > 1; x >>= 1) { if (x <= targetx) break; }
            targetx = x;

            float bestScore = FLT_MAX;
            for (size_t y = maxsize; y > 0; y >>= 1)
            {
                float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targety = y;
                }
            }
        }
        else
        {
            size_t y;
            for (y = maxsize; y > 1; y >>= 1) { if (y <= targety) break; }
            targety = y;

            float bestScore = FLT_MAX;
            for (size_t x = maxsize; x > 0; x >>= 1)
            {
                float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targetx = x;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Entry-point
//--------------------------------------------------------------------------------------
#ifdef _PREFAST_
#pragma prefast(disable : 28198, "Command-line tool, frees all memory on exit")
#endif

int __cdecl wmain(_In_ int argc, _In_z_count_(argc) wchar_t* argv[])
{
    // Parameters and defaults
    size_t width = 0;
    size_t height = 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    TEX_FILTER_FLAGS dwFilter = TEX_FILTER_DEFAULT;
    TEX_FILTER_FLAGS dwSRGB = TEX_FILTER_DEFAULT;
    TEX_FILTER_FLAGS dwFilterOpts = TEX_FILTER_DEFAULT;
    DWORD fileType = WIC_CODEC_BMP;
    int adapter = -1;

    wchar_t szOutputFile[MAX_PATH] = {};

    // Initialize COM (needed for WIC)
    HRESULT hr = hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    // Process command line
    if (argc < 2)
    {
        PrintUsage();
        return 0;
    }

    DWORD dwCommand = LookupByName(argv[1], g_pCommands);
    switch (dwCommand)
    {
    case CMD_CUBIC:
    case CMD_SPHERE:
    case CMD_DUAL_PARABOLA:
    case CMD_DUAL_HEMISPHERE:
        break;

    default:
        wprintf(L"Must use one of: cubic, sphere, parabola, hemisphere\n\n");
        return 1;
    }

    DWORD dwOptions = 0;
    std::list<SConversion> conversion;

    for (int iArg = 2; iArg < argc; iArg++)
    {
        PWSTR pArg = argv[iArg];

        if (('-' == pArg[0]) || ('/' == pArg[0]))
        {
            pArg++;
            PWSTR pValue;

            for (pValue = pArg; *pValue && (':' != *pValue); pValue++);

            if (*pValue)
                *pValue++ = 0;

            DWORD dwOption = LookupByName(pArg, g_pOptions);

            if (!dwOption || (dwOptions & (1 << dwOption)))
            {
                PrintUsage();
                return 1;
            }

            dwOptions |= 1 << dwOption;

            // Handle options with additional value parameter
            switch (dwOption)
            {
            case OPT_FILELIST:
            case OPT_WIDTH:
            case OPT_HEIGHT:
            case OPT_FORMAT:
            case OPT_FILTER:
            case OPT_OUTPUTFILE:
            case OPT_GPU:
                if (!*pValue)
                {
                    if ((iArg + 1 >= argc))
                    {
                        PrintUsage();
                        return 1;
                    }

                    iArg++;
                    pValue = argv[iArg];
                }
                break;

            default:
                break;
            }

            switch (dwOption)
            {
            case OPT_WIDTH:
                if (swscanf_s(pValue, L"%zu", &width) != 1)
                {
                    wprintf(L"Invalid value specified with -w (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (swscanf_s(pValue, L"%zu", &height) != 1)
                {
                    wprintf(L"Invalid value specified with -h (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_FORMAT:
                format = static_cast<DXGI_FORMAT>(LookupByName(pValue, g_pFormats));
                if (!format)
                {
                    format = static_cast<DXGI_FORMAT>(LookupByName(pValue, g_pFormatAliases));
                    if (!format)
                    {
                        wprintf(L"Invalid value specified with -f (%ls)\n", pValue);
                        return 1;
                    }
                }
                break;

            case OPT_FILTER:
                dwFilter = static_cast<TEX_FILTER_FLAGS>(LookupByName(pValue, g_pFilters));
                if (!dwFilter)
                {
                    wprintf(L"Invalid value specified with -if (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_SRGBI:
                dwSRGB |= TEX_FILTER_SRGB_IN;
                break;

            case OPT_SRGBO:
                dwSRGB |= TEX_FILTER_SRGB_OUT;
                break;

            case OPT_SRGB:
                dwSRGB |= TEX_FILTER_SRGB;
                break;

            case OPT_SEPALPHA:
                dwFilterOpts |= TEX_FILTER_SEPARATE_ALPHA;
                break;

            case OPT_NO_WIC:
                dwFilterOpts |= TEX_FILTER_FORCE_NON_WIC;
                break;

            case OPT_OUTPUTFILE:
            {
                wcscpy_s(szOutputFile, MAX_PATH, pValue);

                wchar_t ext[_MAX_EXT];
                _wsplitpath_s(szOutputFile, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);

                fileType = LookupByName(ext, g_pExtFileTypes);
                break;
            }

            case OPT_TA_WRAP:
                if (dwFilterOpts & TEX_FILTER_MIRROR)
                {
                    wprintf(L"Can't use -wrap and -mirror at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                dwFilterOpts |= TEX_FILTER_WRAP;
                break;

            case OPT_TA_MIRROR:
                if (dwFilterOpts & TEX_FILTER_WRAP)
                {
                    wprintf(L"Can't use -wrap and -mirror at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                dwFilterOpts |= TEX_FILTER_MIRROR;
                break;

            case OPT_GPU:
                if (swscanf_s(pValue, L"%d", &adapter) != 1)
                {
                    wprintf(L"Invalid value specified with -gpu (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (adapter < 0)
                {
                    wprintf(L"Adapter index (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_FILELIST:
            {
                std::wifstream inFile(pValue);
                if (!inFile)
                {
                    wprintf(L"Error opening -flist file %ls\n", pValue);
                    return 1;
                }
                wchar_t fname[1024] = {};
                for (;;)
                {
                    inFile >> fname;
                    if (!inFile)
                        break;

                    if (*fname == L'#')
                    {
                        // Comment
                    }
                    else if (*fname == L'-')
                    {
                        wprintf(L"Command-line arguments not supported in -flist file\n");
                        return 1;
                    }
                    else if (wcspbrk(fname, L"?*") != nullptr)
                    {
                        wprintf(L"Wildcards not supported in -flist file\n");
                        return 1;
                    }
                    else
                    {
                        SConversion conv;
                        wcscpy_s(conv.szSrc, MAX_PATH, fname);
                        conversion.push_back(conv);
                    }

                    inFile.ignore(1000, '\n');
                }
                inFile.close();
            }
            break;

            default:
                break;
            }
        }
        else if (wcspbrk(pArg, L"?*") != nullptr)
        {
            size_t count = conversion.size();
            SearchForFiles(pArg, conversion, (dwOptions & (1 << OPT_RECURSIVE)) != 0);
            if (conversion.size() <= count)
            {
                wprintf(L"No matching files found for %ls\n", pArg);
                return 1;
            }
        }
        else
        {
            SConversion conv;
            wcscpy_s(conv.szSrc, MAX_PATH, pArg);

            conversion.push_back(conv);
        }
    }

    if (conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    if (~dwOptions & (1 << OPT_NOLOGO))
        PrintLogo();

    ComPtr<ID3D11Device> pDevice;
    if (!CreateDevice(adapter, pDevice.GetAddressOf()))
    {
        wprintf(L"\nERROR: Direct3D device not available\n");
        return 1;
    }

    ComPtr<ID3D11DeviceContext> pContext;
    pDevice->GetImmediateContext(pContext.GetAddressOf());

    StateObjects stateObjects;
    hr = stateObjects.Create(pDevice.Get());
    if (FAILED(hr))
    {
        wprintf(L" FAILED creating Direct3D state objects (%x)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    Shaders shaders;
    hr = shaders.Create(pDevice.Get());
    if (FAILED(hr))
    {
        wprintf(L" FAILED creating Direct3D shaders (%x)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    UnitCube unitCube;
    hr = unitCube.Create(pDevice.Get());
    if (FAILED(hr))
    {
        wprintf(L" FAILED creating Direct3D unit cube (%x)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    if (format != DXGI_FORMAT_UNKNOWN)
    {
        UINT support = 0;
        hr = pDevice->CheckFormatSupport(format, &support);
        UINT required = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET;
        if (FAILED(hr) || ((support & required) != required))
        {
            wprintf(L"\nERROR: Direct3D device does not support format as a render target (DXGI_FORMAT_");
            PrintFormat(format);
            wprintf(L")\n");
            return 1;
        }
    }

    if (conversion.size() != 1 && conversion.size() != 6)
    {
        wprintf(L"ERROR: cubic/sphere/parabola/hemisphere requires 1 or 6 input images\n");
        return 1;
    }

    // Load images
    size_t images = 0;

    std::vector<std::unique_ptr<ScratchImage>> loadedImages;

    size_t maxWidth = 0;
    size_t maxHeight = 0;

    for (auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv)
    {
        wchar_t ext[_MAX_EXT] = {};
        wchar_t fname[_MAX_FNAME] = {};
        _wsplitpath_s(pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);

        // Load source image
        if (pConv != conversion.begin())
            wprintf(L"\n");
        else if (!*szOutputFile)
        {
            if (_wcsicmp(ext, L".dds") == 0)
            {
                wprintf(L"ERROR: Need to specify output file via -o\n");
                return 1;
            }

            _wmakepath_s(szOutputFile, nullptr, nullptr, fname, L".dds");
        }

        wprintf(L"reading %ls", pConv->szSrc);
        fflush(stdout);

        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);
        if (!image)
        {
            wprintf(L"\nERROR: Memory allocation failed\n");
            return 1;
        }

        if (_wcsicmp(ext, L".dds") == 0)
        {
            hr = LoadFromDDSFile(pConv->szSrc, DDS_FLAGS_ALLOW_LARGE_FILES, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            if (info.IsVolumemap())
            {
                wprintf(L"\nERROR: Can't use volume textures as input\n");
                return 1;
            }

            if (info.arraySize > 1 && info.arraySize != 6)
            {
                wprintf(L"\nERROR: Can only use single cubemap or 6-entry array textures\n");
                return 1;
            }
        }
        else if (_wcsicmp(ext, L".tga") == 0)
        {
            hr = LoadFromTGAFile(pConv->szSrc, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }
        }
        else if (_wcsicmp(ext, L".hdr") == 0)
        {
            hr = LoadFromHDRFile(pConv->szSrc, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }
        }
        else
        {
            // WIC shares the same filter values for mode and dither
            static_assert(static_cast<int>(WIC_FLAGS_DITHER) == static_cast<int>(TEX_FILTER_DITHER), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_DITHER_DIFFUSION) == static_cast<int>(TEX_FILTER_DITHER_DIFFUSION), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_POINT) == static_cast<int>(TEX_FILTER_POINT), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_LINEAR) == static_cast<int>(TEX_FILTER_LINEAR), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_CUBIC) == static_cast<int>(TEX_FILTER_CUBIC), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_FANT) == static_cast<int>(TEX_FILTER_FANT), "WIC_FLAGS_* & TEX_FILTER_* should match");

            hr = LoadFromWICFile(pConv->szSrc, WIC_FLAGS_NONE | dwFilter, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }
        }

        PrintInfo(info);

        fflush(stdout);

        // --- Planar ------------------------------------------------------------------
        if (IsPlanar(info.format))
        {
            auto img = image->GetImage(0, 0, 0);
            assert(img);
            size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = ConvertToSinglePlane(img, nimg, info, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [converttosingleplane] (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }

            auto& tinfo = timage->GetMetadata();

            info.format = tinfo.format;

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
        }

        // --- Decompress --------------------------------------------------------------
        if (IsCompressed(info.format))
        {
            const Image* img = image->GetImage(0, 0, 0);
            assert(img);
            size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = Decompress(img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage.get());
            if (FAILED(hr))
            {
                wprintf(L" FAILED [decompress] (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }

            auto& tinfo = timage->GetMetadata();

            info.format = tinfo.format;

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
        }

        // --- Undo Premultiplied Alpha (if requested) ---------------------------------
        if ((dwOptions & (1 << OPT_DEMUL_ALPHA))
            && HasAlpha(info.format)
            && info.format != DXGI_FORMAT_A8_UNORM)
        {
            if (info.GetAlphaMode() == TEX_ALPHA_MODE_STRAIGHT)
            {
                printf("\nWARNING: Image is already using straight alpha\n");
            }
            else if (!info.IsPMAlpha())
            {
                printf("\nWARNING: Image is not using premultipled alpha\n");
            }
            else
            {
                auto img = image->GetImage(0, 0, 0);
                assert(img);
                size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = PremultiplyAlpha(img, nimg, info, TEX_PMALPHA_REVERSE | dwSRGB, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [demultiply alpha] (%x)\n", static_cast<unsigned int>(hr));
                    continue;
                }

                auto& tinfo = timage->GetMetadata();
                info.miscFlags2 = tinfo.miscFlags2;

                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
            }
        }

        if (format == DXGI_FORMAT_UNKNOWN)
        {
            format = (FormatDataType(info.format) == FORMAT_TYPE_FLOAT)
                ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        images += info.arraySize;

        if (info.arraySize > 1)
        {
            for(size_t j = 0; j < info.arraySize; ++j)
            {
                auto img = image->GetImage(0, j, 0);
                if (!img)
                {
                    wprintf(L"\nERROR: Splitting array failed\n");
                    return 1;
                }

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = timage->InitializeFromImage(*img);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [splitting array] (%x)\n", static_cast<unsigned int>(hr));
                    return 1;
                }

                loadedImages.emplace_back(std::move(timage));
            }
        }
        else
        {
            loadedImages.emplace_back(std::move(image));
        }

        if (info.width > maxWidth)
            maxWidth = info.width;
        if (info.height > maxHeight)
            maxHeight = info.height;
    }

    if (images > 6)
    {
        wprintf(L"WARNING: Ignoring additional images, only using first 6 of %zu to form input cubemap\n", images);
    }

    // --- Convert input to cubemap ----------------------------------------------------
    if (!width)
        width = height;

    if (!height)
        height = width;

    if (!width || !height)
    {
        // TODO - Make pow2?
        if (images == 1)
        {
            width = height = FitPowerOf2(maxHeight, 16384);
        }
        else
        {
            width = maxWidth;
            height = maxHeight;
            FitPowerOf2(width, height, 16384);
        }
    }

    size_t cubeWidth = (dwCommand == CMD_CUBIC) ? width : (images == 1) ? maxHeight : maxWidth;
    size_t cubeHeight = (dwCommand == CMD_CUBIC) ? height : maxHeight;

    RenderTarget cubemap[6];
    for (auto& it : cubemap)
    {
        hr = it.Create(pDevice.Get(), cubeWidth, cubeHeight, format);
        if (FAILED(hr))
        {
            wprintf(L" FAILED to initialize Direct3D cubemap (%08X)\n", static_cast<unsigned int>(hr));
            return 1;
        }
    }

    ComPtr<ID3D11InputLayout> inputLayout;
    hr = unitCube.CreateInputLayout(pDevice.Get(), shaders, inputLayout.GetAddressOf());
    if (FAILED(hr))
    {
        wprintf(L" FAILED to initialize Direct3D input layout(%08X)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    if (images == 1)
    {
        // TODO - perform equirectangular projection to cubemap
    }
    else
    {
        pContext->OMSetBlendState(stateObjects.Opaque(), nullptr, 0xFFFFFFFF);
        pContext->OMSetDepthStencilState(stateObjects.DepthNone(), 0);
        pContext->RSSetState(stateObjects.CullNone());
        auto linear = stateObjects.LinearClamp();

        for (size_t face = 0; face < 6; ++face) 
        {
            ComPtr<ID3D11ShaderResourceView> srv;
            auto& input = loadedImages[face];

            hr = CreateShaderResourceView(pDevice.Get(), input->GetImage(0, 0, 0), 1, input->GetMetadata(), srv.GetAddressOf());
            if (FAILED(hr))
            {
                wprintf(L" FAILED to initialize Direct3D texture from image #%zu (%08X)\n", face, static_cast<unsigned int>(hr));
                return 1;
            }

            cubemap[face].Begin(pContext.Get(), false);

            XMMATRIX mat = XMMatrixIdentity();

            ConstantBuffer cbuffer;
            XMStoreFloat4x4(&cbuffer.transform, mat);

            shaders.Apply(Shaders::VS_BASIC, Shaders::PS_BASIC, pContext.Get(), &cbuffer);
            pContext->IASetInputLayout(inputLayout.Get());
            pContext->PSSetShaderResources(0, 1, srv.GetAddressOf());
            pContext->PSSetSamplers(0, 1, &linear);

            unitCube.Draw(pContext.Get());

            cubemap[face].End(pContext.Get());
        }
    }

    // --- Create result ---------------------------------------------------------------

    // TODO - sphere / dual parabolic projection

    // --- Write result ----------------------------------------------------------------
    wprintf(L"\nWriting %ls ", szOutputFile);
    fflush(stdout);

    if (dwOptions & (1 << OPT_TOLOWER))
    {
        (void)_wcslwr_s(szOutputFile);
    }

    if (~dwOptions & (1 << OPT_OVERWRITE))
    {
        if (GetFileAttributesW(szOutputFile) != INVALID_FILE_ATTRIBUTES)
        {
            wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
            return 1;
        }
    }

    ScratchImage image[6];
    Image imageArray[6] = {};
    switch (dwCommand)
    {
    case CMD_CUBIC:
        for (size_t face = 0; face < 6; ++face)
        {
            hr = CaptureTexture(pDevice.Get(), pContext.Get(), cubemap[face].GetTexture(), image[face]);
            if (FAILED(hr))
            {
                wprintf(L" FAILED to capture Direct3D texture from image #%zu (%08X)\n", face, static_cast<unsigned int>(hr));
                return 1;
            }

            imageArray[face] = *image[face].GetImage(0, 0, 0);
        }

        {
            TexMetadata mdata = {};
            mdata.width = imageArray[0].width;
            mdata.height = imageArray[0].height;
            mdata.format = imageArray[0].format;
            mdata.arraySize = 6;
            mdata.depth = mdata.mipLevels = 1;
            mdata.miscFlags = TEX_MISC_TEXTURECUBE;
            mdata.dimension = TEX_DIMENSION_TEXTURE2D;

            hr = SaveToDDSFile(imageArray, 6, mdata,
                (dwOptions & (1 << OPT_USE_DX10)) ? (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE,
                szOutputFile);
            if (FAILED(hr))
            {
                wprintf(L"\nFAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }
        }
        break;

    default:
        printf("ERROR: E_NOTIMPL\n");
        return 1;
    }

    return 0;
}
