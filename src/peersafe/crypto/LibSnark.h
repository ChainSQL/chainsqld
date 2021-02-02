#pragma once

#include <eth/vm/Common.h>

namespace peersafe
{

	std::pair<bool, eth::bytes> alt_bn128_pairing_product(eth::bytesConstRef _in);
	std::pair<bool, eth::bytes> alt_bn128_G1_add(eth::bytesConstRef _in);
	std::pair<bool, eth::bytes> alt_bn128_G1_mul(eth::bytesConstRef _in);

}
