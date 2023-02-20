//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <cstdint>

namespace elf {

constexpr uint8_t MAX_TENSOR_REF_DIMS = 8;
constexpr uint8_t MAX_TENSOR_REF_STRIDES = MAX_TENSOR_REF_DIMS + 1;
constexpr uint8_t MAX_METADATA_IO = 32;
constexpr uint8_t MAX_OV_NODES = MAX_METADATA_IO;
constexpr uint8_t MAX_STRING_LEN = 32;

using TensorName = char[MAX_STRING_LEN];

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

struct TensorRef {
    float strides[MAX_TENSOR_REF_STRIDES];
    uint32_t dimensions[MAX_TENSOR_REF_DIMS];
    TensorName name;
    uint64_t order;
    DType data_type;
    uint32_t dimensions_size, strides_size;
};

struct PreprocessingInfo {
    TensorName input_name;
    PreProcessColorSpace input_format;
    PreProcessColorSpace output_format;
    PreProcessResizeAlgorithm algorithm;
};

struct OVNode {
    TensorName tensor_names[MAX_METADATA_IO];
    uint64_t shape[MAX_TENSOR_REF_DIMS];
    TensorName friendly_name;
    TensorName input_name;
    OVNodeType type;
    uint32_t shape_size;
    uint32_t tensor_names_count = 0;
};

struct ResourceRequirements{
    uint32_t nn_slice_length_;
    uint32_t ddr_scratch_length_;
    uint16_t nn_barrier_count_;
    uint8_t nn_slice_count_;
    uint8_t nn_barriers_;
};

struct NetworkMetadata {
    ResourceRequirements resource_requirements;

    TensorRef net_input[MAX_METADATA_IO];
    TensorRef net_output[MAX_METADATA_IO];

    TensorRef in_tensor_desc[MAX_METADATA_IO];
    TensorRef out_tensor_desc[MAX_METADATA_IO];

    TensorRef profiling_output[MAX_METADATA_IO];

    OVNode ov_parameters[MAX_OV_NODES];
    OVNode ov_results[MAX_OV_NODES];

    PreprocessingInfo pre_process_info[MAX_METADATA_IO];
    TensorName blob_name;

    uint32_t net_input_count = 0, net_output_count = 0;
    uint32_t in_tenosr_count = 0, out_tensor_count = 0;
    uint32_t profiling_output_count = 0;
    uint32_t ov_parameters_count = 0, ov_results_count = 0;
    uint32_t pre_process_info_count = 0;
};

} // namespace elf
