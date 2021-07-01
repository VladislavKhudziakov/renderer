
#include "tools.hpp"

#include <vk_utils/context.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <shaderc/shaderc.hpp>

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>
#include <map>

namespace
{
    struct level_index
    {
        uint64_t byte_offset;
        uint64_t byte_length;
        uint64_t uncompressed_byte_length;
    };

    struct ktx2_header
    {
        char identifier[12];
        uint32_t vk_format;
        uint32_t type_size;
        uint32_t pixel_width;
        uint32_t pixel_height;
        uint32_t pixel_depth;
        uint32_t layer_count;
        uint32_t face_count;
        uint32_t level_count;
        uint32_t supercompression_scheme;

        uint32_t dfd_byte_offset;
        uint32_t dfd_byte_length;
        uint32_t kvd_byte_offset;
        uint32_t kvd_byte_length;
        uint64_t sgd_byte_offset;
        uint64_t sgd_byte_length;
    };

    constexpr char ktx2_identifier[]{'«', 'K', 'T', 'X', ' ', '2', '0', '»', '\r', '\n', '\x1A', '\n'};

    template<typename T>
    T read_struct(const uint8_t** begin)
    {
        T res{};
        std::memcpy(&res, *begin, sizeof(res));
        *begin += sizeof(res);
        return res;
    }

    #define read_u32 read_struct<uint32_t>

    VkSamplerCreateInfo get_sampler_info(const vk_utils::sampler_info& sampler, uint32_t level_count)
    {
        return {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = sampler.fitering,
            .minFilter = sampler.fitering,
            .mipmapMode = sampler.fitering == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = sampler.tiled ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = sampler.tiled ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = sampler.tiled ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0,
            .anisotropyEnable = static_cast<VkBool32>(sampler.max_anisatropy != 0),
            .maxAnisotropy = sampler.max_anisatropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0,
            .maxLod = level_count - 1.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };
    }

    struct vk_format_info
    {
        uint32_t size;
        uint32_t channel_count;
    };

    std::map<VkFormat, vk_format_info> vk_format_table = {
        {VK_FORMAT_UNDEFINED, {0, 0}},
        {VK_FORMAT_R4G4_UNORM_PACK8, {1, 2}},
        {VK_FORMAT_R4G4B4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B4G4R4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R5G6B5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_B5G6R5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_R5G5B5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B5G5R5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_A1R5G5B5_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R8_UNORM, {1, 1}},
        {VK_FORMAT_R8_SNORM, {1, 1}},
        {VK_FORMAT_R8_USCALED, {1, 1}},
        {VK_FORMAT_R8_SSCALED, {1, 1}},
        {VK_FORMAT_R8_UINT, {1, 1}},
        {VK_FORMAT_R8_SINT, {1, 1}},
        {VK_FORMAT_R8_SRGB, {1, 1}},
        {VK_FORMAT_R8G8_UNORM, {2, 2}},
        {VK_FORMAT_R8G8_SNORM, {2, 2}},
        {VK_FORMAT_R8G8_USCALED, {2, 2}},
        {VK_FORMAT_R8G8_SSCALED, {2, 2}},
        {VK_FORMAT_R8G8_UINT, {2, 2}},
        {VK_FORMAT_R8G8_SINT, {2, 2}},
        {VK_FORMAT_R8G8_SRGB, {2, 2}},
        {VK_FORMAT_R8G8B8_UNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_SNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_USCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_SSCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_UINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SRGB, {3, 3}},
        {VK_FORMAT_B8G8R8_UNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_SNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_USCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_SSCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_UINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SRGB, {3, 3}},
        {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_USCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SSCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_UINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SRGB, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_USCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SSCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SRGB, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SRGB_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_R16_UNORM, {2, 1}},
        {VK_FORMAT_R16_SNORM, {2, 1}},
        {VK_FORMAT_R16_USCALED, {2, 1}},
        {VK_FORMAT_R16_SSCALED, {2, 1}},
        {VK_FORMAT_R16_UINT, {2, 1}},
        {VK_FORMAT_R16_SINT, {2, 1}},
        {VK_FORMAT_R16_SFLOAT, {2, 1}},
        {VK_FORMAT_R16G16_UNORM, {4, 2}},
        {VK_FORMAT_R16G16_SNORM, {4, 2}},
        {VK_FORMAT_R16G16_USCALED, {4, 2}},
        {VK_FORMAT_R16G16_SSCALED, {4, 2}},
        {VK_FORMAT_R16G16_UINT, {4, 2}},
        {VK_FORMAT_R16G16_SINT, {4, 2}},
        {VK_FORMAT_R16G16_SFLOAT, {4, 2}},
        {VK_FORMAT_R16G16B16_UNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_SNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_USCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_SSCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_UINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SFLOAT, {6, 3}},
        {VK_FORMAT_R16G16B16A16_UNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_USCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SSCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_UINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SFLOAT, {8, 4}},
        {VK_FORMAT_R32_UINT, {4, 1}},
        {VK_FORMAT_R32_SINT, {4, 1}},
        {VK_FORMAT_R32_SFLOAT, {4, 1}},
        {VK_FORMAT_R32G32_UINT, {8, 2}},
        {VK_FORMAT_R32G32_SINT, {8, 2}},
        {VK_FORMAT_R32G32_SFLOAT, {8, 2}},
        {VK_FORMAT_R32G32B32_UINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SFLOAT, {12, 3}},
        {VK_FORMAT_R32G32B32A32_UINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SFLOAT, {16, 4}},
        {VK_FORMAT_R64_UINT, {8, 1}},
        {VK_FORMAT_R64_SINT, {8, 1}},
        {VK_FORMAT_R64_SFLOAT, {8, 1}},
        {VK_FORMAT_R64G64_UINT, {16, 2}},
        {VK_FORMAT_R64G64_SINT, {16, 2}},
        {VK_FORMAT_R64G64_SFLOAT, {16, 2}},
        {VK_FORMAT_R64G64B64_UINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SFLOAT, {24, 3}},
        {VK_FORMAT_R64G64B64A64_UINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SFLOAT, {32, 4}},
        {VK_FORMAT_B10G11R11_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_D16_UNORM, {2, 1}},
        {VK_FORMAT_X8_D24_UNORM_PACK32, {4, 1}},
        {VK_FORMAT_D32_SFLOAT, {4, 1}},
        {VK_FORMAT_S8_UINT, {1, 1}},
        {VK_FORMAT_D16_UNORM_S8_UINT, {3, 2}},
        {VK_FORMAT_D24_UNORM_S8_UINT, {4, 2}},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, {8, 2}},
        {VK_FORMAT_BC1_RGB_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGB_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC2_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC2_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC4_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC4_SNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC5_SNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_UFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_SFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_EAC_R11_UNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11_SNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11G11_UNORM_BLOCK, {16, 2}},
        {VK_FORMAT_EAC_R11G11_SNORM_BLOCK, {16, 2}},
        {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_4x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG, {8, 4}},
        // KHR_sampler_YCbCr_conversion extension - single-plane variants
        // 'PACK' formats are normal, uncompressed
        {VK_FORMAT_R10X6_UNORM_PACK16, {2, 1}},
        {VK_FORMAT_R10X6G10X6_UNORM_2PACK16, {4, 2}},
        {VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_R12X4_UNORM_PACK16, {2, 1}},
        {VK_FORMAT_R12X4G12X4_UNORM_2PACK16, {4, 2}},
        {VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16, {8, 4}},
        // _422 formats encode 2 texels per entry with B, R components shared - treated as compressed w/ 2x1 block size
        {VK_FORMAT_G8B8G8R8_422_UNORM, {4, 4}},
        {VK_FORMAT_B8G8R8G8_422_UNORM, {4, 4}},
        {VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_G16B16G16R16_422_UNORM, {8, 4}},
        {VK_FORMAT_B16G16R16G16_422_UNORM, {8, 4}},
        // KHR_sampler_YCbCr_conversion extension - multi-plane variants
        // Formats that 'share' components among texels (_420 and _422), size represents total bytes for the smallest possible texel block
        // _420 share B, R components within a 2x2 texel block
        {VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, {6, 3}},
        {VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, {6, 3}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM, {12, 3}},
        {VK_FORMAT_G16_B16R16_2PLANE_420_UNORM, {12, 3}},
        // _422 share B, R components within a 2x1 texel block
        {VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM, {4, 3}},
        {VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, {4, 3}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM, {8, 3}},
        {VK_FORMAT_G16_B16R16_2PLANE_422_UNORM, {8, 3}},
        // _444 do not share
        {VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, {3, 3}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, {6, 3}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16, {6, 3}},
        {VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM, {6, 3}}};

}

ERROR_TYPE vk_utils::load_texture(
  const char* path, 
  VkQueue transfer_queue, 
  uint32_t transfer_queue_family_index, 
  VkCommandPool cmd_pool, 
  const sampler_info& sampler, 
  vk_utils::vma_image_handler& out_image, 
  vk_utils::image_view_handler& out_image_view, 
  vk_utils::sampler_handler& out_image_sampler)
{
    constexpr const char* ktx_formats[] {".ktx", ".ktx2"};
    constexpr const char* stb_formats[] {".png", ".jpg", ".jpeg"};

    auto find_cond = [path](const char* ext) {
        return strstr(path, ext) != nullptr;
    };

    if (std::find_if(std::begin(ktx_formats), std::end(ktx_formats), find_cond) != std::end(ktx_formats)) {
        PASS_ERROR(load_ktx_texture(path, transfer_queue, transfer_queue_family_index, cmd_pool, sampler, out_image, out_image_view, out_image_sampler));
    } else if (std::find_if(std::begin(stb_formats), std::end(stb_formats), find_cond) != std::end(stb_formats)) {
        PASS_ERROR(load_texture_2D(path, transfer_queue, transfer_queue_family_index, cmd_pool, sampler, out_image, out_image_view, out_image_sampler));
    } else {
        RAISE_ERROR_WARN(-1, "unsupported image type.");
    }

    RAISE_ERROR_OK();
}

ERROR_TYPE vk_utils::load_texture_2D(
    const char* path,
    VkQueue transfer_queue,
    uint32_t transfer_queue_family_index,
    VkCommandPool cmd_pool,
    const sampler_info& sampler,
    vk_utils::vma_image_handler& out_image,
    vk_utils::image_view_handler& out_image_view,
    vk_utils::sampler_handler& out_image_sampler,
    bool gen_mips)
{
    int w, h, c;
    std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> image_handler{
        stbi_load(path, &w, &h, &c, 0),
        [](stbi_uc* ptr) {if (ptr != nullptr) stbi_image_free(ptr); }};

    if (image_handler == nullptr) {
        RAISE_ERROR_WARN(-1, "cannot load image.");
    }

    std::vector<uint8_t> rgba_image_buffer;
    const uint8_t* img_data_ptr = image_handler.get();

    if (c == STBI_rgb && !check_opt_tiling_format(VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) && !check_opt_tiling_format(VK_FORMAT_R8G8B8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        c = STBI_rgb_alpha;

        rgba_image_buffer.reserve(w * h * 4);

        for (size_t i = 0; i < w * h; ++i) {
            rgba_image_buffer.push_back(img_data_ptr[i * 3]);
            rgba_image_buffer.push_back(img_data_ptr[i * 3 + 1]);
            rgba_image_buffer.push_back(img_data_ptr[i * 3 + 2]);
            rgba_image_buffer.push_back(255);
        }

        img_data_ptr = rgba_image_buffer.data();
    }

    VkFormat fmt{};

    switch (c) {
        case STBI_grey: {
            if (check_opt_tiling_format(VK_FORMAT_R8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported grey image format.");
            }
            break;
        }
        case STBI_grey_alpha: {
            if (check_opt_tiling_format(VK_FORMAT_R8G8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8G8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported grey_alpha image format.");
            }
            break;
        }
        case STBI_rgb: {
            if (check_opt_tiling_format(VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8G8B8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported rgb image format.");
            }
            break;
        }
        case STBI_rgb_alpha: {
            if (check_opt_tiling_format(VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8A8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8A8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported rgba image format.");
            }
            break;
        }
        default:
            RAISE_ERROR_WARN(-1, "invalid img format.");
    }

    PASS_ERROR(create_texture_2D(transfer_queue, transfer_queue_family_index, cmd_pool, sampler, w, h, fmt, gen_mips, img_data_ptr, out_image, out_image_view, out_image_sampler));

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::create_texture_2D(
    VkQueue transfer_queue,
    uint32_t transfer_queue_family_index,
    VkCommandPool command_pool,
    const sampler_info& sampler,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    bool gen_mips,
    const void* data,
    vk_utils::vma_image_handler& out_image,
    vk_utils::image_view_handler& out_image_view,
    vk_utils::sampler_handler& out_image_sampler)
{
    vk_utils::vma_image_handler image;
    vk_utils::image_view_handler image_view;
    vk_utils::sampler_handler image_sampler;

    size_t pixel_size = 1;
    VkComponentMapping components;

    switch (format) {
        case VK_FORMAT_R8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8_UINT:
            pixel_size = 1;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_R,
                .b = VK_COMPONENT_SWIZZLE_R,
                .a = VK_COMPONENT_SWIZZLE_ONE};
            break;
        case VK_FORMAT_R8G8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8G8_UINT:
            pixel_size = 2;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_ZERO,
                .b = VK_COMPONENT_SWIZZLE_ZERO,
                .a = VK_COMPONENT_SWIZZLE_G};
            break;
        case VK_FORMAT_R8G8B8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8G8B8_UINT:
            pixel_size = 3;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_ONE};
            break;
        case VK_FORMAT_R8G8B8A8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8G8B8A8_UINT:
            pixel_size = 4;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A};
            break;
        default:
            RAISE_ERROR_FATAL(-1, "unsupported pixels format.");
    }

    const uint32_t mip_levels = gen_mips ? log2(std::max(width, height)) : 1;

    vk_utils::vma_buffer_handler staging_buffer{};

    if (data != nullptr) {
        create_buffer(staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, width * height * pixel_size, data);
    }

    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1},
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &transfer_queue_family_index,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo image_alloc_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    if (const auto e = image.init(vk_utils::context::get().allocator(), &image_info, &image_alloc_info); e != VK_SUCCESS) {
        RAISE_ERROR_WARN(e, "Cannot init image.");
    }

    VkImageViewCreateInfo img_view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_info.format,
        .components = components,
     
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    if (const auto e = image_view.init(vk_utils::context::get().device(), &img_view_info); e != VK_SUCCESS) {
        image.destroy();
        RAISE_ERROR_WARN(e, "Cannot init image view.");
    }

    VkSamplerCreateInfo sampler_info = get_sampler_info(sampler, mip_levels);

    if (const auto e = image_sampler.init(vk_utils::context::get().device(), &sampler_info); e != VK_SUCCESS) {
        image.destroy();
        image_view.destroy();
        RAISE_ERROR_WARN(e, "Cannot init sampler.");
    }

    VkCommandBufferAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_alloc_info.pNext = nullptr;
    buffer_alloc_info.commandBufferCount = 1;
    buffer_alloc_info.commandPool = command_pool;
    buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vk_utils::cmd_buffers_handler images_data_transfer_buffer;
    images_data_transfer_buffer.init(vk_utils::context::get().device(), command_pool, &buffer_alloc_info, 1);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.pInheritanceInfo = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(images_data_transfer_buffer[0], &begin_info);

    VkImageMemoryBarrier img_transfer_barrier{};
    img_transfer_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_transfer_barrier.pNext = nullptr;
    img_transfer_barrier.image = image;
    img_transfer_barrier.srcAccessMask = 0;
    img_transfer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_transfer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_transfer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_transfer_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_transfer_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    img_transfer_barrier.subresourceRange.layerCount = 1;
    img_transfer_barrier.subresourceRange.baseArrayLayer = 0;
    img_transfer_barrier.subresourceRange.layerCount = 1;
    img_transfer_barrier.subresourceRange.baseMipLevel = 0;
    img_transfer_barrier.subresourceRange.levelCount = mip_levels;
    img_transfer_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdPipelineBarrier(images_data_transfer_buffer[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_transfer_barrier);

    VkBufferImageCopy img_copy{};
    img_copy.imageExtent = image_info.extent;
    img_copy.imageOffset = {0, 0, 0};
    img_copy.bufferRowLength = 0;
    img_copy.bufferImageHeight = 0;
    img_copy.bufferOffset = 0;
    img_copy.imageSubresource.mipLevel = 0;
    img_copy.imageSubresource.baseArrayLayer = 0;
    img_copy.imageSubresource.layerCount = 1;
    img_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdCopyBufferToImage(images_data_transfer_buffer[0], staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &img_copy);
    if (gen_mips) {
        std::vector<VkImageMemoryBarrier> barriers_list{};
        barriers_list.reserve(mip_levels + 1);

        VkImageMemoryBarrier mip_gen_barriers{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }};

        uint32_t mip_width = width;
        uint32_t mip_height = height;

        for (uint32_t i = 1; i < mip_levels; ++i) {
            mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            mip_gen_barriers.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            mip_gen_barriers.subresourceRange.baseMipLevel = i - 1;

            vkCmdPipelineBarrier(
                images_data_transfer_buffer[0],
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &mip_gen_barriers);

            VkImageBlit blit_region{};

            blit_region.srcOffsets[0].x = 0;
            blit_region.srcOffsets[0].y = 0;
            blit_region.srcOffsets[0].z = 0;
            blit_region.srcOffsets[1].x = mip_width;
            blit_region.srcOffsets[1].y = mip_height;
            blit_region.srcOffsets[1].z = 1;

            blit_region.srcSubresource.baseArrayLayer = 0;
            blit_region.srcSubresource.layerCount = 1;
            blit_region.srcSubresource.mipLevel = i - 1;
            blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            mip_width = std::max(1u, mip_width / 2);
            mip_height = std::max(1u, mip_height / 2);

            blit_region.dstOffsets[0].x = 0;
            blit_region.dstOffsets[0].y = 0;
            blit_region.dstOffsets[0].z = 0;
            blit_region.dstOffsets[1].x = mip_width;
            blit_region.dstOffsets[1].y = mip_height;
            blit_region.dstOffsets[1].z = 1;

            blit_region.dstSubresource.baseArrayLayer = 0;
            blit_region.dstSubresource.layerCount = 1;
            blit_region.dstSubresource.mipLevel = i;
            blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdBlitImage(
                images_data_transfer_buffer[0],
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit_region,
                VK_FILTER_LINEAR);
        }
        mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mip_gen_barriers.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        mip_gen_barriers.subresourceRange.baseMipLevel = 0;
        mip_gen_barriers.subresourceRange.levelCount = mip_levels - 1;

        vkCmdPipelineBarrier(
            images_data_transfer_buffer[0],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &mip_gen_barriers);

        mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mip_gen_barriers.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        mip_gen_barriers.subresourceRange.baseMipLevel = mip_levels - 1;
        mip_gen_barriers.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(
            images_data_transfer_buffer[0],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &mip_gen_barriers);
    } else {
        VkImageMemoryBarrier img_barrier{};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.pNext = nullptr;
        img_barrier.image = image;
        img_barrier.srcAccessMask = 0;
        img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        img_barrier.subresourceRange.layerCount = 1;
        img_barrier.subresourceRange.baseArrayLayer = 0;
        img_barrier.subresourceRange.layerCount = 1;
        img_barrier.subresourceRange.baseMipLevel = 0;
        img_barrier.subresourceRange.levelCount = mip_levels;
        img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        vkCmdPipelineBarrier(images_data_transfer_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);
    }

    vkEndCommandBuffer(images_data_transfer_buffer[0]);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.pCommandBuffers = images_data_transfer_buffer;
    submit_info.commandBufferCount = 1;

    const auto fence = create_fence();
    vkQueueSubmit(transfer_queue, 1, &submit_info, fence);
    vkWaitForFences(vk_utils::context::get().device(), 1, fence, VK_TRUE, UINT64_MAX);

    out_image = std::move(image);
    out_image_view = std::move(image_view);
    out_image_sampler = std::move(image_sampler);

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::create_buffer(
    vk_utils::vma_buffer_handler& out_buffer,
    VkBufferUsageFlags buffer_usage,
    VmaMemoryUsage memory_usage,
    uint32_t size,
    const void* data)
{
    vk_utils::vma_buffer_handler buffer;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = nullptr;
    buffer_info.size = size;
    buffer_info.usage = buffer_usage;
    buffer_info.queueFamilyIndexCount = 1;
    const uint32_t family_index = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
    buffer_info.pQueueFamilyIndices = &family_index;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = memory_usage;
    alloc_info.flags = 0;

    if (const auto err = buffer.init(vk_utils::context::get().allocator(), &buffer_info, &alloc_info); err != VK_SUCCESS) {
        RAISE_ERROR_WARN(err, "cannot init buffer.");
    }

    if (data != nullptr) {
        void* mapped_data;
        vmaMapMemory(vk_utils::context::get().allocator(), buffer, &mapped_data);
        std::memcpy(mapped_data, data, size);
        vmaUnmapMemory(vk_utils::context::get().allocator(), buffer);
        vmaFlushAllocation(
            vk_utils::context::get().allocator(),
            buffer,
            0,
            VK_WHOLE_SIZE);
    }

    out_buffer = std::move(buffer);

    RAISE_ERROR_OK();
}


vk_utils::fence_handler vk_utils::create_fence(VkFenceCreateFlagBits flags)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.pNext = nullptr;
    fence_info.flags = flags;
    vk_utils::fence_handler fence;
    fence.init(vk_utils::context::get().device(), &fence_info);

    return fence;
}


vk_utils::semaphore_handler vk_utils::create_semaphore()
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = nullptr;
    semaphore_info.flags = 0;

    vk_utils::semaphore_handler semaphore;
    semaphore.init(vk_utils::context::get().device(), &semaphore_info);

    return semaphore;
}


bool vk_utils::check_opt_tiling_format(VkFormat req_fmt, VkFormatFeatureFlagBits features_flags)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(vk_utils::context::get().gpu(), req_fmt, &props);
    return props.optimalTilingFeatures & features_flags;
}


bool vk_utils::check_linear_tiling_format(VkFormat req_fmt, VkFormatFeatureFlagBits features_flags)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(vk_utils::context::get().gpu(), req_fmt, &props);
    return props.linearTilingFeatures & features_flags;
}

ERROR_TYPE vk_utils::load_shader(
    const char* shader_path,
    vk_utils::shader_module_handler& handle,
    VkShaderStageFlagBits& stage)
{
    struct shader_kind
    {
        shaderc_shader_kind shaderc_kind{};
        const char* shaderc_string{};
        VkShaderStageFlagBits vk_shader_stage{};
    };

    shader_kind curr_shader_kind{};

    static std::unordered_map<const char*, shader_kind> kinds{
        {".vert", {shaderc_shader_kind::shaderc_glsl_vertex_shader, "vs", VK_SHADER_STAGE_VERTEX_BIT}},
        {".frag", {shaderc_shader_kind::shaderc_glsl_fragment_shader, "fs", VK_SHADER_STAGE_FRAGMENT_BIT}},
        {".geom", {shaderc_shader_kind::shaderc_glsl_geometry_shader, "gs", VK_SHADER_STAGE_GEOMETRY_BIT}},
        {".tesc", {shaderc_shader_kind::shaderc_glsl_tess_control_shader, "tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT}},
        {".tese", {shaderc_shader_kind::shaderc_glsl_tess_evaluation_shader, "tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT}},
        {".comp", {shaderc_shader_kind::shaderc_glsl_compute_shader, "comp", VK_SHADER_STAGE_COMPUTE_BIT}},
        {".mesh", {shaderc_shader_kind::shaderc_glsl_mesh_shader, "mesh", VK_SHADER_STAGE_MESH_BIT_NV}},
        {".task", {shaderc_shader_kind::shaderc_glsl_task_shader, "task", VK_SHADER_STAGE_TASK_BIT_NV}},
        {".rgen", {shaderc_shader_kind::shaderc_glsl_raygen_shader, "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR}},
        {".rint", {shaderc_shader_kind::shaderc_glsl_intersection_shader, "rint", VK_SHADER_STAGE_INTERSECTION_BIT_KHR}},
        {".rahit", {shaderc_shader_kind::shaderc_glsl_anyhit_shader, "rahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR}},
        {".rchit", {shaderc_shader_kind::shaderc_glsl_closesthit_shader, "rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}},
        {".rmiss", {shaderc_shader_kind::shaderc_glsl_miss_shader, "rmiss", VK_SHADER_STAGE_MESH_BIT_NV}},
        {".rcall", {shaderc_shader_kind::shaderc_glsl_callable_shader, "rcall", VK_SHADER_STAGE_CALLABLE_BIT_KHR}}};

    bool shader_kind_found = false;

    for (const auto& [stage_name, kind_val] : kinds) {
        if (strstr(shader_path, stage_name) != nullptr) {
            curr_shader_kind = kind_val;
            shader_kind_found = true;
            break;
        }
    }

    if (!shader_kind_found) {
        RAISE_ERROR_WARN(-1, "cannot find actual shader kind.");
    }

    std::unique_ptr<FILE, std::function<void(FILE*)>> f_handle(
        nullptr, [](FILE* f) { fclose(f); });

    bool is_spv = strstr(shader_path, ".spv") != nullptr;

    if (is_spv) {
        f_handle.reset(fopen(shader_path, "rb"));
    } else {
        f_handle.reset(fopen(shader_path, "r"));
    }

    if (f_handle == nullptr) {
        RAISE_ERROR_WARN(-1, "cannot load shader file.");
    }

    fseek(f_handle.get(), 0L, SEEK_END);
    auto size = ftell(f_handle.get());
    fseek(f_handle.get(), 0L, SEEK_SET);

    if (is_spv) {
        std::vector<char> code(size);
        fread(code.data(), 1, size, f_handle.get());
        PASS_ERROR(create_shader_module(reinterpret_cast<const uint32_t*>(code.data()), code.size(), handle));
    } else {
        static shaderc::Compiler compiler{};

        std::string source;
        source.resize(size);
        fread(source.data(), 1, size, f_handle.get());

        shaderc::CompileOptions options;

#ifndef NDEBUG
        options.SetOptimizationLevel(
            shaderc_optimization_level_zero);
        options.SetGenerateDebugInfo();
#else
        options.SetOptimizationLevel(
            shaderc_optimization_level_performance);
#endif
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, curr_shader_kind.shaderc_kind, curr_shader_kind.shaderc_string, options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            LOG_ERROR(result.GetErrorMessage());
            RAISE_ERROR_WARN(-1, "cannot load shader file.");
        }

        std::vector<uint32_t> shaderSPRV;
        shaderSPRV.assign(result.begin(), result.end());
        PASS_ERROR(create_shader_module(shaderSPRV.data(), shaderSPRV.size() * sizeof(uint32_t), handle));
    }

    stage = curr_shader_kind.vk_shader_stage;
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::create_shader_module(
    const uint32_t* code,
    uint32_t code_size,
    vk_utils::shader_module_handler& handle)
{
    VkShaderModuleCreateInfo shader_module_info{};
    shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.pNext = nullptr;
    shader_module_info.pCode = code;
    shader_module_info.codeSize = code_size;

    vk_utils::shader_module_handler module;

    if (module.init(vk_utils::context::get().device(), &shader_module_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot create shader module.");
    }

    handle = std::move(module);

    RAISE_ERROR_OK();
}

ERROR_TYPE vk_utils::load_ktx_texture(
  const char* path, 
  VkQueue transfer_queue,
  uint32_t transfer_queue_family_index, 
  VkCommandPool cmd_pool, 
  const sampler_info& sampler, 
  vk_utils::vma_image_handler& out_image, 
  vk_utils::image_view_handler& out_image_view, 
  vk_utils::sampler_handler& out_image_sampler)
{
    std::unique_ptr<FILE, std::function<void(FILE*)>> f_handle(nullptr, [](FILE* f) { fclose(f); });
    f_handle.reset(fopen(path, "rb"));

    if (f_handle == nullptr) {
        RAISE_ERROR_WARN(-1, "cannot load texture file.");
    }

    fseek(f_handle.get(), 0L, SEEK_END);
    size_t size = ftell(f_handle.get());
    fseek(f_handle.get(), 0L, SEEK_SET);

    std::vector<uint8_t> file_data(size);
    fread(file_data.data(), 1, size, f_handle.get());

    PASS_ERROR(create_ktx_texture(
      file_data.data(),
      size,
      transfer_queue, 
      transfer_queue_family_index, 
      cmd_pool, 
      sampler, 
      out_image,
      out_image_view, 
      out_image_sampler));

    RAISE_ERROR_OK();
}

 #define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

ERROR_TYPE vk_utils::create_ktx_texture(
    const void* data,
    size_t data_size,
    VkQueue transfer_queue,
    uint32_t transfer_queue_family_index,
    VkCommandPool cmd_pool,
    const sampler_info& sampler,
    vk_utils::vma_image_handler& out_image,
    vk_utils::image_view_handler& out_img_view,
    vk_utils::sampler_handler& out_sampler)
{

    const uint8_t* header_begin = static_cast<const uint8_t*>(data);

    const auto header = read_struct<ktx2_header>(&header_begin);

    auto c = strncmp(ktx2_identifier, header.identifier, 12);

    if (c != 0) {
        RAISE_ERROR_WARN(-1, "bad ktx file.");
    }
  
    if (!check_opt_tiling_format(static_cast<VkFormat>(header.vk_format), VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        RAISE_ERROR_WARN(-1, "unsupported vk format.");
    }
  
    vk_utils::vma_image_handler image{};
    vk_utils::image_view_handler image_view{};
    vk_utils::sampler_handler image_sampler{};

    uint32_t level_count = header.level_count;
    uint32_t layer_count = header.layer_count == 0 ? 1 : header.layer_count;

    bool gen_mips = false;

    if (level_count == 0) {
        level_count = std::log2(std::max(header.pixel_width, header.pixel_height));
        gen_mips = true;
    }

    const auto image_type = header.pixel_depth != 0 ? VK_IMAGE_TYPE_3D : (header.pixel_height != 0 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
    uint32_t flags = 0;

    if (header.layer_count > 0 && image_type != VK_IMAGE_TYPE_1D) {
        if (image_type == VK_IMAGE_TYPE_3D) {
            RAISE_ERROR_WARN(-1, "3D textures cannot be arrays.");
        }

        flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }

    if (header.face_count == 6) {
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .imageType = image_type,
        .format = static_cast<VkFormat>(header.vk_format),
        .extent = {header.pixel_width, header.pixel_height, header.pixel_depth == 0 ? 1 : header.pixel_depth},
        .mipLevels = level_count,
        .arrayLayers = layer_count * header.face_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &transfer_queue_family_index,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo alloc_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    if (auto res = image.init(vk_utils::context::get().allocator(), &image_info, &alloc_info); res != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init image");
    }

    VkImageViewType image_view_type = VK_IMAGE_VIEW_TYPE_3D;

    if (image_type == VK_IMAGE_TYPE_3D) {
        image_view_type = VK_IMAGE_VIEW_TYPE_3D;
    } else if (image_type == VK_IMAGE_TYPE_1D) {
        image_view_type = header.layer_count > 0 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
    } else {
        if (header.face_count == 6) {
            image_view_type = header.layer_count > 0 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        } else {
            image_view_type = header.layer_count > 0 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
    }

    VkImageViewCreateInfo image_view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = image_view_type,
        .format = image_info.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = level_count,
            .baseArrayLayer = 0,
            .layerCount = layer_count * header.face_count,
        }
    };

    if (image_view.init(vk_utils::context::get().device(), &image_view_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init image view");
    }

    VkSamplerCreateInfo sampler_info = get_sampler_info(sampler, level_count);

    if (image_sampler.init(vk_utils::context::get().device(), &sampler_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init image sampler");
    }
    
    uint32_t faces_count = header.face_count;
    uint32_t level_width = header.pixel_width;
    uint32_t level_height = header.pixel_height;
    uint32_t level_depth = header.pixel_depth == 0 ? 1 : header.pixel_depth;
    uint32_t pixel_size = vk_format_table[static_cast<VkFormat>(header.vk_format)].size;

    uint32_t image_size{0};

    std::vector<VkBufferImageCopy> image_copies;
    image_copies.reserve(level_count * layer_count * faces_count);

    auto level_index_begin = header_begin;
    const uint8_t* data_begin = reinterpret_cast<const uint8_t*>(data);

    for (uint32_t level = 0; level < level_count; ++level) {
        const auto curr_level_index = read_struct<level_index>(&level_index_begin);

        auto level_size = level_width * level_height * level_depth * pixel_size;
        auto mip_padding = 4 - level_size & (4 - 1);
                  
        for (uint32_t layer = 0; layer < layer_count; ++layer) {
            for (uint32_t face = 0; face < faces_count; ++face) {
                VkBufferImageCopy curr_copy_info{
                    .bufferOffset = curr_level_index.byte_offset,
                    .bufferRowLength = level_width,
                    .bufferImageHeight = level_height,

                    .imageSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = level,
                   
                        .baseArrayLayer = layer + face,
                        .layerCount = 1,
                    },

                    .imageOffset = {.x = 0, .y = 0, .z = 0},

                    .imageExtent = {.width = level_width, .height = level_height, .depth = level_depth}
                };

                image_size += level_width * level_height * level_depth * pixel_size;

                image_copies.emplace_back() = curr_copy_info;
            }
        }
        
        image_size += mip_padding;

        level_width = std::max(1u, level_width / 2u);
        level_height = std::max(1u, level_height / 2u);
        level_depth = std::max(1u, level_depth / 2u);
    }

    VkBufferCreateInfo staging_buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = data_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &transfer_queue_family_index
    };

    VmaAllocationCreateInfo staging_buffer_alloc_info{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    vk_utils::vma_buffer_handler staging_buffer{};

    if (staging_buffer.init(vk_utils::context::get().allocator(), &staging_buffer_info, &staging_buffer_alloc_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init staging buffer");
    }

    void* staging_buffer_mapped_data{nullptr};
    if (vmaMapMemory(vk_utils::context::get().allocator(), staging_buffer, &staging_buffer_mapped_data) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot map vk memory.");
    }

    std::memcpy(staging_buffer_mapped_data, data, staging_buffer_info.size);

    vk_utils::cmd_buffers_handler copy_cmd_buffer{};
    VkCommandBufferAllocateInfo copy_buffer_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    copy_cmd_buffer.init(vk_utils::context::get().device(), cmd_pool, &copy_buffer_alloc_info, 1);
    
    VkCommandBufferBeginInfo copy_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    vkBeginCommandBuffer(copy_cmd_buffer[0], &copy_buffer_begin_info);

        VkImageMemoryBarrier image_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,

            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,

            .oldLayout = image_info.initialLayout,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

            .image = image,

            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = level_count,
                .baseArrayLayer = 0,
                .layerCount = layer_count * faces_count,
            }
        };

    vkCmdPipelineBarrier(copy_cmd_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
    
    vkCmdCopyBufferToImage(copy_cmd_buffer[0], staging_buffer, image, image_barrier.newLayout, image_copies.size(), image_copies.data());
    
    image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_barrier.oldLayout = image_barrier.newLayout;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    vkCmdPipelineBarrier(copy_cmd_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);

    if (vkEndCommandBuffer(copy_cmd_buffer[0]) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot map write cmd buffer.");
    }

    VkSubmitInfo submit_info
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,

        .commandBufferCount = 1,
        .pCommandBuffers = copy_cmd_buffer,

        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };

    auto fence = create_fence();

    vkQueueSubmit(transfer_queue, 1, &submit_info, fence);
    vkWaitForFences(vk_utils::context::get().device(), 1, fence, VK_TRUE, UINT64_MAX);

    out_image = std::move(image);
    out_img_view = std::move(image_view);
    out_sampler = std::move(image_sampler);

    RAISE_ERROR_OK();
}