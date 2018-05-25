#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <evmjit.h>

struct evmc_uint256be balance(struct evmc_context* context,
	const struct evmc_address* address)
{
	struct evmc_uint256be ret = { .bytes = { 1, 2, 3, 4 } };
	return ret;
}

struct evmc_address address(struct evmc_context* context)
{
	struct evmc_address ret = { .bytes = { 1, 2, 3, 4 } };
	return ret;
}

static void print_address(const struct evmc_address* address)
{
	int i = 0;
	for (i = 0; i < sizeof(address->bytes); ++i)
		printf("%x", address->bytes[i] & 0xff);
}

static int account_exists(struct evmc_context* context,
	const struct evmc_address* address) {
	printf("EVM-C: EXISTS @");
	print_address(address);
	printf("\n");
	return 0;
}

static void get_storage(struct evmc_uint256be* result,
	struct evmc_context* context,
	const struct evmc_address* address,
	const struct evmc_uint256be* key)
{
	printf("EVM-C: SLOAD @");
	print_address(address);
	printf("\n");
}

static void set_storage(struct evmc_context* context,
	const struct evmc_address* address,
	const struct evmc_uint256be* key,
	const struct evmc_uint256be* value)
{
	printf("EVM-C: SSTORE @");
	print_address(address);
	printf("\n");
}

static void get_balance(struct evmc_uint256be* result,
	struct evmc_context* context,
	const struct evmc_address* address)
{
	printf("EVM-C: BALANCE @");
	print_address(address);
	printf("\n");
	*result = balance(context, address);
}

static size_t get_code_size(struct evmc_context* context, const struct evmc_address* address)
{
	printf("EVM-C: CODESIZE @");
	print_address(address);
	printf("\n");
	return 0;
}

static size_t copy_code(struct evmc_context* context, const struct evmc_address* address,
	size_t code_offset, uint8_t* buffer_data, size_t buffer_size)
{
	printf("EVM-C: COPYCODE @");
	print_address(address);
	printf("\n");
	return 0;
}

static void selfdestruct(struct evmc_context* context,
	const struct evmc_address* address,
	const struct evmc_address* beneficiary)
{
	printf("EVM-C: SELFDESTRUCT ");
	print_address(address);
	printf(" -> ");
	print_address(beneficiary);
	printf("\n");
}

static void call(struct evmc_result* result,
	struct evmc_context* context,
	const struct evmc_message* msg)
{
	printf("EVM-C: CALL (depth: %d)\n", msg->depth);
	const uint8_t* data = "peersafe";
	result->release = NULL;
	result->output_data = data;
	result->output_size = strlen(data);
	result->status_code = EVMC_SUCCESS;
}

static void get_tx_context(struct evmc_tx_context* result, struct evmc_context* context)
{

}

static void get_block_hash(struct evmc_uint256be* result, struct evmc_context* context,
	int64_t number)
{

}

/// EVM log callback.
///
/// @note The `evm_log` name is used to avoid conflict with `log()` C function.
static void evm_log(struct evmc_context* context, const struct evmc_address* address,
	const uint8_t* data, size_t data_size,
	const struct evmc_uint256be topics[], size_t topics_count)
{
	printf("EVM-C: LOG%d\n", (int)topics_count);
}

static const struct evmc_context_fn_table ctx_fn_table = {
	account_exists,
	get_storage,
	set_storage,
	get_balance,
	get_code_size,
	copy_code,
	selfdestruct,
	call,
	get_tx_context,
	get_block_hash,
	evm_log
};

int fromHexChar(char _i)
{
	if (_i >= '0' && _i <= '9')
		return _i - '0';
	if (_i >= 'a' && _i <= 'f')
		return _i - 'a' + 10;
	if (_i >= 'A' && _i <= 'F')
		return _i - 'A' + 10;
	return -1;
}

size_t fromHex(const uint8_t * hex, size_t size, uint8_t* binary)
{
	size_t offset = 0;
	size_t s = (size >= 2 && hex[0] == '0' && hex[1] == 'x') ? 2 : 0;

	if (size % 2)
	{
		int h = fromHexChar(hex[s++]);
		if (h != -1) {
			//ret.push_back(h);
			binary[offset++] = h;
		}
		else
			return 0;
	}
	for (size_t i = s; i < size; i += 2)
	{
		int h = fromHexChar(hex[i]);
		int l = fromHexChar(hex[i + 1]);
		if (h != -1 && l != -1)
			binary[offset++] = (uint8_t)(h * 16 + l);
		else
			return 0;
	}
	return offset;
}

int main(int argc, char** argv)
{
	uint8_t* hex_code = "";
	uint8_t* hex_input = "";
	for (int i = 1; i < argc - 1;) {
		if (strncmp(argv[i], "--code", strlen("--code")) == 0) {
			hex_code = argv[++i];
		} 
		else if (strncmp(argv[i], "--input", strlen("--input")) == 0) {
			hex_input = argv[++i];
		}
		i++;
	}

	if (strlen(hex_code) == 0) {
		printf("contract is empty.");
		exit(0);
	}

    struct evmc_instance* jit = evmjit_create();
    if (jit->abi_version != EVMC_ABI_VERSION)
        return 1;  // Incompatible ABI version.

	uint8_t code[8096] = { 0 };
    size_t code_size = fromHex(hex_code, strlen(hex_code), code);

    struct evmc_uint256be code_hash = {.bytes = {1, 2, 3,}};
	uint8_t input[4096] = {0};
	size_t input_size = fromHex(hex_input, strlen(hex_input), input);

    struct evmc_uint256be value = {{0, 0, 0,}};
    struct evmc_address addr = {{1, 2, 3,}};
    struct evmc_address caller = {{4, 5, 6}};
    int64_t gas = 200000;

    struct evmc_context ctx = {&ctx_fn_table};

    struct evmc_message msg = {addr, caller, value, input, input_size,
                              code_hash, gas, 0, strlen(hex_input)?EVMC_CALL:EVMC_CREATE, 0};

    struct evmc_result result =
        jit->execute(jit, &ctx, EVMC_FRONTIER, &msg, code, code_size);

    printf("Execution result:\n");
    if (result.status_code != EVMC_SUCCESS) {
      printf("  EVM execution failure: %d\n", result.status_code);
    } else {
        printf("  Gas used: %" PRId64 "\n", gas - result.gas_left);
        printf("  Gas left: %" PRId64 "\n", result.gas_left);
        printf("  Output size: %zd\n", result.output_size);

        printf("  Output: ");
        size_t i = 0;
        for (i = 0; i < result.output_size; i++) {
            printf("%02x", result.output_data[i]);
        }
        printf("\n");
    }

    if (result.release)
        result.release(&result);
    jit->destroy(jit);
    return 0;
}
