//
//  Copyright (C) 2017 Microsoft.  All rights reserved.
//  See LICENSE file in the project root for full license information.
// 
#include "AppxFactory.hpp"
#include "UnicodeConversion.hpp"
#include "Exceptions.hpp"
#include "ZipObject.hpp"
#include "AppxPackageObject.hpp"
#include "MSIXResource.hpp"
#include "VectorStream.hpp"

namespace MSIX {
    // IAppxFactory
    HRESULT STDMETHODCALLTYPE AppxFactory::CreatePackageWriter (
        IStream* outputStream,
        APPX_PACKAGE_SETTINGS* ,//settings, TODO: plumb this through
        IAppxPackageWriter** packageWriter) noexcept
    {
        return static_cast<HRESULT>(Error::NotImplemented);
    }

    HRESULT STDMETHODCALLTYPE AppxFactory::CreatePackageReader (
        IStream* inputStream,
        IAppxPackageReader** packageReader) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (packageReader == nullptr || *packageReader != nullptr), "Invalid parameter");
        ComPtr<IMsixFactory> self;
        ThrowHrIfFailed(QueryInterface(UuidOfImpl<IMsixFactory>::iid, reinterpret_cast<void**>(&self)));
        ComPtr<IStream> input(inputStream);
        auto zip = ComPtr<IStorageObject>::Make<ZipObject>(self.Get(), input);
        auto result = ComPtr<IAppxPackageReader>::Make<AppxPackageObject>(self.Get(), m_validationOptions, m_applicabilityFlags, zip);
        *packageReader = result.Detach();
        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    HRESULT STDMETHODCALLTYPE AppxFactory::CreateManifestReader(
        IStream* inputStream,
        IAppxManifestReader** manifestReader) noexcept
    {
        return static_cast<HRESULT>(Error::NotImplemented);
    }

    HRESULT STDMETHODCALLTYPE AppxFactory::CreateBlockMapReader (
        IStream* inputStream,
        IAppxBlockMapReader** blockMapReader) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (
            inputStream == nullptr || 
            blockMapReader == nullptr || 
            *blockMapReader != nullptr
        ),"bad pointer.");

        ComPtr<IMsixFactory> self;
        ThrowHrIfFailed(QueryInterface(UuidOfImpl<IMsixFactory>::iid, reinterpret_cast<void**>(&self)));
        ComPtr<IStream> stream(inputStream);
        *blockMapReader = ComPtr<IAppxBlockMapReader>::Make<AppxBlockMapObject>(self.Get(), stream).Detach();
        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    HRESULT STDMETHODCALLTYPE AppxFactory::CreateValidatedBlockMapReader (
        IStream* inputStream,
        LPCWSTR signatureFileName,
        IAppxBlockMapReader** blockMapReader) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (
            inputStream == nullptr || 
            signatureFileName == nullptr ||
            *signatureFileName == '\0' ||
            blockMapReader == nullptr || 
            *blockMapReader != nullptr
        ),"bad pointer.");

        ComPtr<IMsixFactory> self;
        ThrowHrIfFailed(QueryInterface(UuidOfImpl<IMsixFactory>::iid, reinterpret_cast<void**>(&self)));
        auto stream = ComPtr<IStream>::Make<FileStream>(utf16_to_utf8(signatureFileName), FileStream::Mode::READ);
        auto signature = ComPtr<IVerifierObject>::Make<AppxSignatureObject>(self.Get(), self->GetValidationOptions(), stream);
        ComPtr<IStream> input(inputStream);
        auto validatedStream = signature->GetValidationStream("AppxBlockMap.xml", input);
        *blockMapReader = ComPtr<IAppxBlockMapReader>::Make<AppxBlockMapObject>(self.Get(), validatedStream).Detach();
        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    // IAppxBundleFactory
    HRESULT STDMETHODCALLTYPE AppxFactory::CreateBundleWriter(IStream *outputStream, UINT64 bundleVersion, IAppxBundleWriter **bundleWriter) noexcept
    {
        #ifdef BUNDLE_SUPPORT
            return static_cast<HRESULT>(Error::NotImplemented);
        #else
            return static_cast<HRESULT>(MSIX::Error::NotSupported);
        #endif
    }

    HRESULT STDMETHODCALLTYPE AppxFactory::CreateBundleReader(IStream *inputStream, IAppxBundleReader **bundleReader) noexcept try
    {
        #ifdef BUNDLE_SUPPORT
            ComPtr<IAppxPackageReader> reader;
            ThrowHrIfFailed(CreatePackageReader(inputStream, &reader));
            auto result = reader.As<IAppxBundleReader>();
            *bundleReader = result.Detach();
            return static_cast<HRESULT>(Error::OK);
        #else
            return static_cast<HRESULT>(MSIX::Error::NotSupported);
        #endif
    } CATCH_RETURN();

    HRESULT STDMETHODCALLTYPE AppxFactory::CreateBundleManifestReader(IStream *inputStream, IAppxBundleManifestReader **manifestReader) noexcept
    {
        #ifdef BUNDLE_SUPPORT
            return static_cast<HRESULT>(Error::NotImplemented);
        #else
            return static_cast<HRESULT>(MSIX::Error::NotSupported);
        #endif
    }

    // IMsixFactory
    HRESULT AppxFactory::MarshalOutString(std::string& internal, LPWSTR *result) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (result == nullptr || *result != nullptr), "bad pointer" );
        *result = nullptr;
        if (!internal.empty())
        {
            auto intermediate = utf8_to_wstring(internal);
            std::size_t countBytes = sizeof(wchar_t)*(internal.size()+1);
            *result = reinterpret_cast<LPWSTR>(m_memalloc(countBytes));
            ThrowErrorIfNot(Error::OutOfMemory, (*result), "Allocation failed!");
            std::memset(reinterpret_cast<void*>(*result), 0, countBytes);
            std::memcpy(reinterpret_cast<void*>(*result),
                        reinterpret_cast<void*>(const_cast<wchar_t*>(intermediate.c_str())),
                        countBytes - sizeof(wchar_t));
        }
        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    HRESULT AppxFactory::MarshalOutBytes(std::vector<std::uint8_t>& data, UINT32* size, BYTE** buffer) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (size==nullptr || buffer == nullptr || *buffer != nullptr), "Bad pointer");
        *size = static_cast<UINT32>(data.size());
        *buffer = reinterpret_cast<BYTE*>(m_memalloc(data.size()));
        ThrowErrorIfNot(Error::OutOfMemory, (*buffer), "Allocation failed");
        std::memcpy(reinterpret_cast<void*>(*buffer),
                    reinterpret_cast<void*>(data.data()),
                    data.size());
        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    ComPtr<IStream> AppxFactory::GetResource(const std::string& resource)
    {
        if(!m_resourcezip) // Initialize it when first needed.
        {
            ComPtr<IMsixFactory> self;
            ThrowHrIfFailed(QueryInterface(UuidOfImpl<IMsixFactory>::iid, reinterpret_cast<void**>(&self)));
            // Get stream of the resource zip file generated at CMake processing.
            m_resourcesVector = std::vector<std::uint8_t>(Resource::resourceByte, Resource::resourceByte + Resource::resourceLength);
            auto resourceStream = ComPtr<IStream>::Make<VectorStream>(&m_resourcesVector);
            m_resourcezip = ComPtr<IStorageObject>::Make<ZipObject>(self.Get(), resourceStream.Get());
        }
        auto file = m_resourcezip->GetFile(resource);
        ThrowErrorIfNot(Error::FileNotFound, file, resource.c_str());
        return file;
    }

    HRESULT AppxFactory::MarshalOutWstring(std::wstring& internal, LPWSTR *result) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (result == nullptr || *result != nullptr), "bad pointer" );
        *result = nullptr;
        if (!internal.empty())
        {
            std::size_t countBytes = sizeof(wchar_t)*(internal.size()+1);
            *result = reinterpret_cast<LPWSTR>(m_memalloc(countBytes));
            ThrowErrorIfNot(Error::OutOfMemory, (*result), "Allocation failed!");
            std::memset(reinterpret_cast<void*>(*result), 0, countBytes);
            std::memcpy(reinterpret_cast<void*>(*result),
                        reinterpret_cast<void*>(const_cast<wchar_t*>(internal.c_str())),
                        countBytes - sizeof(wchar_t));
        }
        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    // IMsixFactoryOverrides
    HRESULT STDMETHODCALLTYPE AppxFactory::SpecifyExtension(MSIX_FACTORY_EXTENSION name, IUnknown* extension) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (extension == nullptr), "Invalid parameter");

        if (name == MSIX_FACTORY_EXTENSION_STREAM_FACTORY)
        {
            ThrowHrIfFailed(extension->QueryInterface(UuidOfImpl<IMsixStreamFactory>::iid, reinterpret_cast<void**>(&m_streamFactory)));
        }
        else
        {
            return static_cast<HRESULT>(Error::InvalidParameter);
        }

        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

    HRESULT STDMETHODCALLTYPE AppxFactory::GetCurrentSpecifiedExtension(MSIX_FACTORY_EXTENSION name, IUnknown** extension) noexcept try
    {
        ThrowErrorIf(Error::InvalidParameter, (extension == nullptr || *extension != nullptr), "Invalid parameter");

        if (name == MSIX_FACTORY_EXTENSION_STREAM_FACTORY)
        {
            if (m_streamFactory.Get() != nullptr)
            {
                *extension = m_streamFactory.As<IUnknown>().Detach();
            }
        }
        else
        {
            return static_cast<HRESULT>(Error::InvalidParameter);
        }

        return static_cast<HRESULT>(Error::OK);
    } CATCH_RETURN();

} // namespace MSIX 
