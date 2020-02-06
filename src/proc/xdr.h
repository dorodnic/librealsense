// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "synthetic-stream.h"

namespace rs2
{
    class stream_profile;
}

namespace librealsense 
{
    class xdr : public generic_processing_block
    {
    public:
        xdr();

    protected:
        rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;
        bool should_process(const rs2::frame& frame) override { return true; }

    private:
        std::map<int, rs2::frameset> _frames;
        rs2::asynchronous_syncer _s;
        rs2::frame_queue _queue;
    };
}
