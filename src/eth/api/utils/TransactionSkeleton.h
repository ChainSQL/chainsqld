#ifndef ETH_API_UTIL_SKELETON_H_INCLUDED
#define ETH_API_UTIL_SKELETON_H_INCLUDED

#include <eth/tools/FixedHash.h>

using namespace eth;
namespace ripple {
class TransactionSkeleton
{
public:
    bool creation = false;
    h160 from;
    h160 to;
    u256 value;
    bytes data;
    u256 nonce = Invalid256;
    u256 gas = Invalid256;
    u256 gasPrice = Invalid256;
};

}
#endif