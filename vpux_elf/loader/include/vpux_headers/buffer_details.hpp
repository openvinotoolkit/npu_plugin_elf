//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

struct BufferDetails {
    bool mIsProcessed;
    bool mIsShared;
    bool mHasData;

    BufferDetails(): mIsProcessed(false), mIsShared(false), mHasData(false) {
    }
};
