// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2018-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include <eth/evmc/include/evmc/evmc.h>
#include <eth/evmc/include/evmc/utils.h>

//#define EVMC_EXPORT
//
//#if __cplusplus
//#define EVMC_NOEXCEPT noexcept
//#else
//#define EVMC_NOEXCEPT
//#endif

#if __cplusplus
extern "C" {
#endif

EVMC_EXPORT struct evmc_vm* evmc_create_aleth_interpreter() EVMC_NOEXCEPT;

#if __cplusplus
}
#endif
