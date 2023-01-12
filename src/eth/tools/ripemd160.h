#pragma once

#include <ripple/basics/Blob.h>
#include <ripple/basics/Slice.h>

using namespace ripple;

namespace eth {
    Blob eth_ripemd160(Slice const& _input);
}