#include "chainsql/intrinsics.hpp"

using namespace chainsql::native;

extern "C" {
   void chainsql_assert(int32_t test, const void *msg)
   {
      return intrinsics::get().call<intrinsics::chainsql_assert>(test, msg);
   }

   void chainsql_assert_message(int32_t test, const void *msg, int32_t msg_len)
   {
      return intrinsics::get().call<intrinsics::chainsql_assert_message>(test, msg, msg_len);
   }

   void chainsql_memcpy(void *dst, uint64_t src, uint32_t len)
   {
      return intrinsics::get().call<intrinsics::chainsql_memcpy>(dst, src, len);
   }

   const void *get_tx_context()
   {
      return intrinsics::get().call<intrinsics::get_tx_context>();
   }

   int32_t account_exists(const void *address, int32_t len)
   {
      return intrinsics::get().call<intrinsics::account_exists>(address, len);
   }

   int32_t get_balance(void *balance_buf, int32_t *buf_size)
   {
      return intrinsics::get().call<intrinsics::get_balance>(balance_buf, buf_size);
   }

   int32_t get_block_hash(int64_t block, void *hash_buf, int32_t *buf_size)
   {
      return intrinsics::get().call<intrinsics::get_block_hash>(block, hash_buf, buf_size);
   }

   void emit_log(const void *topics, int32_t topic_number, const void *data, int32_t data_size)
   {
      return intrinsics::get().call<intrinsics::emit_log>(topics, topic_number, data, data_size);
   }

   int64_t account_set(int32_t flag, int32_t set)
   {
      return intrinsics::get().call<intrinsics::account_set>(flag, set);
   }

   int64_t transfer_fee_set(const void *rate, int32_t rate_len,
                            const void *min, int32_t min_len,
                            const void *max, int32_t max_len)
   {
      return intrinsics::get().call<intrinsics::transfer_fee_set>(rate, rate_len,
                                                             min, min_len,
                                                             max, max_len);
   }

   int64_t trust_set(const void *value, int32_t value_len,
                     const void *currency, int32_t currency_len)
   {
      return intrinsics::get().call<intrinsics::trust_set>(value, value_len,
                                                           currency, currency_len);
   }

   int64_t trust_limit(const void *currency, int32_t currency_len,
                       int64_t power, const void *gateway, int32_t gateway_len)
   {
      return intrinsics::get().call<intrinsics::trust_limit>(currency, currency_len,
                                                           power, gateway, gateway_len);
   }

   int64_t gateway_balance(const void *currency, int32_t currency_len,
                           int64_t power, const void *gateway, int32_t gateway_len)
   {
      return intrinsics::get().call<intrinsics::gateway_balance>(currency, currency_len,
                                                                 power, gateway, gateway_len);
   }

   int64_t pay(const void *to, int32_t to_len,
               const void *value, int32_t value_len,
               const void *max, int32_t max_len,
               const void *currency, int32_t currency_len,
               const void *gateway, int32_t gateway_len)
   {
      return intrinsics::get().call<intrinsics::pay>(to, to_len,
                                                     value, value_len,
                                                     max, max_len,
                                                     currency, currency_len,
                                                     gateway, gateway_len);
   }
}