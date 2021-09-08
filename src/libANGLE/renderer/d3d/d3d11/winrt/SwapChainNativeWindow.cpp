//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SwapChainNativeWindow.cpp: NativeWindow for managing IDXGISwapChain native window types.

#include "libANGLE/renderer/d3d/d3d11/winrt/SwapChainNativeWindow.h"

#include <math.h>
#include <algorithm>

using namespace ABI::Windows::Foundation::Collections;
using namespace Microsoft::WRL;

namespace
{
DXGI_FORMAT GetSwapChainFormat(const ComPtr<IDXGISwapChain> &swapChain)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    HRESULT result            = swapChain->GetDesc(&desc);
    if (SUCCEEDED(result))
    {
        return desc.BufferDesc.Format;
    }
    return DXGI_FORMAT_UNKNOWN;
}
}  // namespace

namespace rx
{
bool SwapChainNativeWindow::initialize(EGLNativeWindowType window, IPropertySet *propertySet)
{
    mSupportsSwapChainResize = false;

    ComPtr<IPropertySet> props = propertySet;
    ComPtr<IInspectable> win   = window;
    SIZE swapChainSize         = {};
    HRESULT result             = S_OK;

    // IPropertySet is an optional parameter and can be null.
    // If one is specified, cache as an IMap and read the properties
    // used for initial host initialization.
    if (propertySet)
    {
        result = props.As(&mPropertyMap);
        if (FAILED(result))
        {
            return false;
        }

        // The EGLRenderSurfaceSizeProperty is optional and may be missing. The IPropertySet
        // was prevalidated to contain the EGLNativeWindowType before being passed to
        // this host.
        result = GetOptionalSizePropertyValue(mPropertyMap, EGLRenderSurfaceSizeProperty,
                                              &swapChainSize, &mSwapChainSizeSpecified);
        if (FAILED(result))
        {
            return false;
        }

        // The EGLRenderResolutionScaleProperty is optional and may be missing. The IPropertySet
        // was prevalidated to contain the EGLNativeWindowType before being passed to
        // this host.
        result = GetOptionalSinglePropertyValue(mPropertyMap, EGLRenderResolutionScaleProperty,
                                                &mSwapChainScale, &mSwapChainScaleSpecified);
        if (FAILED(result))
        {
            return false;
        }

        if (!mSwapChainScaleSpecified)
        {
            // Default value for the scale is 1.0f
            mSwapChainScale = 1.0f;
        }

        // A EGLRenderSurfaceSizeProperty and a EGLRenderResolutionScaleProperty can't both be
        // specified
        if (mSwapChainScaleSpecified && mSwapChainSizeSpecified)
        {
            ERR() << "It is invalid to specify both an EGLRenderSurfaceSizeProperty and a "
                     "EGLRenderResolutionScaleProperty.";
            return false;
        }
    }

    if (SUCCEEDED(result))
    {
        result = win.As(&mSwapChain);
    }

    if (SUCCEEDED(result))
    {
        // If a swapchain size is specfied, then the automatic resize
        // behaviors implemented by the host should be disabled.  The swapchain
        // will be still be scaled when being rendered to fit the bounds
        // of the host.
        // Scaling of the swapchain output is handled by the EGL client.
        if (mSwapChainSizeSpecified)
        {
            mClientRect = {0, 0, swapChainSize.cx, swapChainSize.cy};
        }
        else
        {
            Size swapChainPanelSize;
            result = GetSwapChainSize(mSwapChain, &swapChainPanelSize);

            if (SUCCEEDED(result))
            {
                // Update the client rect to account for any swapchain scale factor
                mClientRect = clientRect(swapChainPanelSize);
            }
        }
    }

    if (SUCCEEDED(result))
    {
        mNewClientRect     = mClientRect;
        mClientRectChanged = false;
        return true;
    }

    return false;
}

HRESULT SwapChainNativeWindow::createSwapChain(ID3D11Device *device,
                                               IDXGIFactory2 *factory,
                                               DXGI_FORMAT format,
                                               unsigned int width,
                                               unsigned int height,
                                               bool containsAlpha,
                                               IDXGISwapChain1 **swapChain)
{
    if (swapChain == nullptr || width == 0 || height == 0)
    {
        return E_INVALIDARG;
    }
    ASSERT(format == GetSwapChainFormat(mSwapChain));

    HRESULT result = mSwapChain.CopyTo(swapChain);

    // If the host is responsible for scaling the output of the swapchain, then
    // scale it now before returning an instance to the caller.  This is done by
    // first reading the current size of the swapchain panel, then scaling
    if (SUCCEEDED(result))
    {
        if (mSwapChainSizeSpecified || mSwapChainScaleSpecified)
        {
            Size currentPanelSize = {};
            result                = GetSwapChainSize(mSwapChain, &currentPanelSize);

            // Scale the swapchain to fit inside the contents of the panel.
            if (SUCCEEDED(result))
            {
                ASSERT(width == (unsigned int)currentPanelSize.Width);
                ASSERT(height == (unsigned int)currentPanelSize.Height);
                result = scaleSwapChain(currentPanelSize, mClientRect);
            }
        }
    }

    return result;
}

HRESULT SwapChainNativeWindow::scaleSwapChain(const Size &windowSize, const RECT &clientRect)
{
    return S_OK;
}

HRESULT GetSwapChainSize(const ComPtr<IDXGISwapChain> &swapChain, Size *windowSize)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    HRESULT result            = swapChain->GetDesc(&desc);
    if (SUCCEEDED(result))
    {
        *windowSize = {(float)desc.BufferDesc.Width, (float)desc.BufferDesc.Height};
    }
    return result;
}
}  // namespace rx
