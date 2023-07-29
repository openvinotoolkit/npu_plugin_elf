//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#if defined(_MSC_VER)
#define VPUX_ALIGNED_STRUCT(alignment) __declspec(align(alignment))
#elif defined(__GNUC__) || defined(__clang__)
#define VPUX_ALIGNED_STRUCT(alignment) __attribute__((aligned(alignment)))
#else
#error Define alignment macro
#endif

#include <cstddef>
#include <cstdint>

namespace elf {

#pragma pack(push, 1)

constexpr uint8_t MAX_TENSOR_REF_DIMS = 8;
constexpr uint8_t MAX_TENSOR_REF_STRIDES = MAX_TENSOR_REF_DIMS + 1;
constexpr uint8_t MAX_METADATA_IO = 32;
constexpr uint8_t MAX_OV_NODES = MAX_METADATA_IO;

// Common string size used by the drivers
// It is highly recommended to keep this size identical between loader and drivers
constexpr uint16_t MAX_STRING_LEN = 256;

using BasicString = char[MAX_STRING_LEN];
using ArchName = BasicString;
using BlobName = BasicString;
using TensorName = BasicString;

enum class DType {
    DType_NOT_SET = 0,
    DType_FP64 = 1,
    DType_FP32 = 2,
    DType_FP16 = 3,
    DType_FP8 = 4,
    DType_U64 = 5,
    DType_U32 = 6,
    DType_U16 = 7,
    DType_U8 = 8,
    DType_I64 = 9,
    DType_I32 = 10,
    DType_I16 = 11,
    DType_I8 = 12,
    DType_I4 = 13,
    DType_I2 = 14,
    DType_I4X = 15,
    DType_BIN = 16,
    DType_LOG = 17,
    DType_I2X = 18,
    DType_BFP16 = 19,
    DType_U4 = 20,
    DType_MIN = DType_NOT_SET,
    DType_MAX = DType_U4
};

enum PreProcessColorSpace {
    PreProcessColorSpace_DEFAULT = 0,
    PreProcessColorSpace_BGR = 1,
    PreProcessColorSpace_RGB = 2,
    PreProcessColorSpace_NV12 = 3,
    PreProcessColorSpace_I420 = 4,
    PreProcessColorSpace_MIN = PreProcessColorSpace_DEFAULT,
    PreProcessColorSpace_MAX = PreProcessColorSpace_I420
};

enum PreProcessResizeAlgorithm {
    PreProcessResizeAlgorithm_NO_RESIZE = 0,
    PreProcessResizeAlgorithm_RESIZE_BILINEAR = 1,
    PreProcessResizeAlgorithm_RESIZE_AREA = 2,
    PreProcessResizeAlgorithm_MIN = PreProcessResizeAlgorithm_NO_RESIZE,
    PreProcessResizeAlgorithm_MAX = PreProcessResizeAlgorithm_RESIZE_AREA
};

enum OVNodeType {
    OVNodeType_UNDEFINED = 0,
    OVNodeType_DYNAMIC = 1,
    OVNodeType_BOOLEAN = 2,
    OVNodeType_BF16 = 3,
    OVNodeType_F16 = 4,
    OVNodeType_F32 = 5,
    OVNodeType_F64 = 6,
    OVNodeType_I4 = 7,
    OVNodeType_I8 = 8,
    OVNodeType_I16 = 9,
    OVNodeType_I32 = 10,
    OVNodeType_I64 = 11,
    OVNodeType_U1 = 12,
    OVNodeType_U4 = 13,
    OVNodeType_U8 = 14,
    OVNodeType_U16 = 15,
    OVNodeType_U32 = 16,
    OVNodeType_U64 = 17,
    OVNodeType_MIN = OVNodeType_UNDEFINED,
    OVNodeType_MAX = OVNodeType_U64
};

struct VPUX_ALIGNED_STRUCT(8) TensorRef {
    float strides[MAX_TENSOR_REF_STRIDES];
    uint32_t dimensions[MAX_TENSOR_REF_DIMS];
    uint8_t pad0_[4] = {0};
    TensorName name;
    uint64_t order;
    DType data_type;
    uint32_t dimensions_size;
    uint32_t strides_size;
    uint8_t pad1_[4] = {0};
};

static_assert(sizeof(TensorRef) == 352, "TensorRef size != 352");
static_assert(offsetof(TensorRef, name) % 8 == 0, "Alignment error");

struct VPUX_ALIGNED_STRUCT(4) PreprocessingInfo {
    TensorName input_name;
    PreProcessColorSpace input_format;
    PreProcessColorSpace output_format;
    PreProcessResizeAlgorithm algorithm;
};

static_assert(sizeof(PreprocessingInfo) == 268, "PreprocessingInfo size != 268");
static_assert(offsetof(PreprocessingInfo, input_format) % 8 == 0, "Alignment error");

struct VPUX_ALIGNED_STRUCT(8) OVNode {
    TensorName tensor_names[MAX_METADATA_IO];
    uint64_t shape[MAX_TENSOR_REF_DIMS];
    TensorName friendly_name;
    TensorName input_name;
    OVNodeType type;
    uint32_t shape_size;
    uint32_t tensor_names_count = 0;
    uint8_t pad_[4] = {0};
};

static_assert(sizeof(OVNode) == 8784, "OVNode size != 8784");

struct VPUX_ALIGNED_STRUCT(4) ResourceRequirements {
    uint32_t nn_slice_length_;
    uint32_t ddr_scratch_length_;
    uint8_t pad_[2] = {0};
    uint8_t nn_slice_count_;
    uint8_t nn_barriers_;
};

static_assert(sizeof(ResourceRequirements) == 12, "ResourceRequirements size != 12");

struct VPUX_ALIGNED_STRUCT(8) NetworkMetadata {
    // Contains the VPU architecture name null-terminated string provided by the compiler.
    // The loader expects an identical string to the one provided by the comiler.
    ArchName arch_name;
    // Contains the network name null-terminated string provided by the compiler.
    // If no name is provided by the compiler, a default name is used.
    // The driver expects an identical string as initially provided to the compiler.
    BlobName blob_name;

    ResourceRequirements resource_requirements;
    uint8_t pad0_[4] = {0};
    TensorRef net_input[MAX_METADATA_IO];
    TensorRef net_output[MAX_METADATA_IO];

    TensorRef in_tensor_desc[MAX_METADATA_IO];
    TensorRef out_tensor_desc[MAX_METADATA_IO];

    TensorRef profiling_output[MAX_METADATA_IO];

    OVNode ov_parameters[MAX_OV_NODES];
    OVNode ov_results[MAX_OV_NODES];

    PreprocessingInfo pre_process_info[MAX_METADATA_IO];

    uint32_t net_input_count = 0;
    uint32_t net_output_count = 0;
    uint32_t in_tenosr_count = 0;
    uint32_t out_tensor_count = 0;
    uint32_t profiling_output_count = 0;
    uint32_t ov_parameters_count = 0;
    uint32_t ov_results_count = 0;
    uint32_t pre_process_info_count = 0;
};

static_assert(sizeof(NetworkMetadata) == 627632, "NetworkMetadata size != 627632");
static_assert(offsetof(NetworkMetadata, arch_name) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, blob_name) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, resource_requirements) % 4 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, net_input) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, net_output) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, in_tensor_desc) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, out_tensor_desc) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, profiling_output) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, ov_parameters) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, ov_results) % 8 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, pre_process_info) % 4 == 0, "Alignment error");
static_assert(offsetof(NetworkMetadata, net_input_count) % 4 == 0, "Alignment error");

#pragma pack(pop)

}  // namespace elf
