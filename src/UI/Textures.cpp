#include "UI/Textures.h"

#include <d3d11.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <unordered_map>

namespace Isekai::UI {

    namespace {
        // SRV pointers live for the whole session; the game owns the device and we
        // never destroy the views (the process teardown reclaims them).
        std::unordered_map<std::string, ImTextureID> g_cache;
    }

    ImTextureID GetTexture(const std::string& a_path) {
        if (const auto it = g_cache.find(a_path); it != g_cache.end()) {
            return it->second;
        }

        // Cache a miss up front: whatever fails below stays failed, and the log
        // gets exactly one line about it instead of one per frame.
        g_cache[a_path] = 0;

        auto* device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetDevice());
        if (!device) {
            logger::error("Textures: no D3D device for \"{}\"", a_path);
            return 0;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(a_path.c_str(), &width, &height, &channels, 4);
        if (!pixels) {
            logger::error("Textures: could not load \"{}\" ({})", a_path,
                          stbi_failure_reason() ? stbi_failure_reason() : "?");
            return 0;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = pixels;
        sub.SysMemPitch = static_cast<UINT>(width * 4);

        ID3D11Texture2D*          texture = nullptr;
        ID3D11ShaderResourceView* view = nullptr;
        const bool ok = SUCCEEDED(device->CreateTexture2D(&desc, &sub, &texture)) &&
                        SUCCEEDED(device->CreateShaderResourceView(texture, nullptr, &view));
        if (texture) {
            texture->Release();  // the view keeps its own reference
        }
        stbi_image_free(pixels);

        if (!ok || !view) {
            logger::error("Textures: D3D upload failed for \"{}\"", a_path);
            return 0;
        }

        const auto id = static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(view));
        g_cache[a_path] = id;
        logger::info("Textures: loaded \"{}\" ({}x{})", a_path, width, height);
        return id;
    }
}
