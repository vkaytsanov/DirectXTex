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
        bool GetDXGIFactory(_Outptr_ IDXGIFactory1 * *pFactory)
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
        bool CreateDevice(int adapter, _Outptr_ ID3D11Device** pDevice)
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

    if (format != DXGI_FORMAT_UNKNOWN)
    {
        UINT support = 0;
        hr = pDevice->CheckFormatSupport(format, &support);
        if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_RENDER_TARGET))
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
            format = (GetFormatDataType(info.format) == FORMAT_TYPE_FLOAT)
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
    }

    if (images > 6)
    {
        wprintf(L"WARNING: Ignoring additional images, only using first 6 of %zu to form input cubemap\n", images);
    }

    // --- Convert input to cubemap ----------------------------------------------------
    if (images == 1)
    {
        // TODO - perform equirectangular projection to cubemap
    }
    else
    {
        // TODO - assemble cubemap
    }

    // --- Create result ---------------------------------------------------------------

    // TODO - sphere / dual parabolic projection

    // --- Write result ----------------------------------------------------------------
#if 0
    // Write texture
    wprintf(L"\nWriting %ls ", szOutputFile);
    PrintInfo(result.GetMetadata());
    wprintf(L"\n");
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

    hr = SaveToDDSFile(result.GetImages(), result.GetImageCount(), result.GetMetadata(),
        (dwOptions & (1 << OPT_USE_DX10)) ? (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE,
        szOutputFile);
    if (FAILED(hr))
    {
        wprintf(L"\nFAILED (%x)\n", static_cast<unsigned int>(hr));
        return 1;
    }
#endif

    return 0;
}
