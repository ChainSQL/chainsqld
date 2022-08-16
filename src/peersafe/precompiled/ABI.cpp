
#include <peersafe/precompiled/ABI.h>
#include <peersafe/basics/TypeTransform.h>
using namespace std;
using namespace eth;

namespace ripple {

const int ContractABI::MAX_BYTE_LENGTH;

bool
ContractABI::abiOutByFuncSelector(
    bytesConstRef _data,
    const std::vector<std::string>& _allTypes,
    std::vector<std::string>& _out)
{
    data = _data;
    offset = 0;

    for (const std::string& type : _allTypes)
    {
        if ("int" == type || "int256" == type)
        {
            s256 s;
            deserialise(s, offset);
            _out.push_back(toString(s));
        }
        else if ("uint" == type || "uint256" == type)
        {
            u256 u;
            deserialise(u, offset);
            _out.push_back(toString(u));
        }
        else if ("address" == type)
        {
            Address addr;
            deserialise(addr, offset);
            //_out.push_back(addr.hex());
            _out.push_back(toHex(addr));
        }
        else if ("string" == type)
        {
            u256 stringOffset;
            deserialise(stringOffset, offset);

            std::string str;
            deserialise(str, static_cast<std::size_t>(stringOffset));
            _out.push_back(str);
        }
        else
        {  // unsupported type
            return false;
        }

        offset += MAX_BYTE_LENGTH;
    }

    return true;
}

// unsigned integer type uint256.
bytes
ContractABI::serialise(const int& _in)
{
    return serialise((s256)_in);
}

// unsigned integer type uint256.
bytes
ContractABI::serialise(const u256& _in)
{
    bytes retBlob(32);
    eth::toBigEndian(_in, retBlob);
    return retBlob;
}

// signed integer type int256.
bytes
ContractABI::serialise(const s256& _in)
{
    u256 data = _in.convert_to<u256>();
    bytes retBlob(32);
    eth::toBigEndian(data, retBlob);
    return retBlob;
}

// equivalent to uint8 restricted to the values 0 and 1. For computing the
// function selector, bool is used
bytes
ContractABI::serialise(const bool& _in)
{
    auto var = uint256(_in ? 1 : 0);
    return bytes(var.begin(),var.end());
}

// equivalent to uint160, except for the assumed interpretation and language
// typing. For computing the function selector, address is used. bool is used.
bytes
ContractABI::serialise(const Address& _in)
{
    return bytes(12, 0) + toBytes(_in);
}

// binary type of 32 bytes
bytes
ContractABI::serialise(const string32& _in)
{
    bytes ret(32, 0);
    bytesConstRef((uint8_t const*)_in.data(), 32).populate(bytesRef(&ret));
    return ret;
}

// dynamic sized Unicode string assumed to be UTF-8 encoded.
bytes
ContractABI::serialise(const std::string& _in)
{
    uint256 tmp(_in.size());
    bytes ret(tmp.begin(),tmp.end());
    //ret = h256(u256(_in.size())).asBytes();
    ret.resize(
        ret.size() + (_in.size() + 31) / MAX_BYTE_LENGTH * MAX_BYTE_LENGTH);
    bytesConstRef(&_in).populate(bytesRef(&ret).cropped(32));
    return ret;
}

void
ContractABI::deserialise(s256& out, std::size_t _offset)
{
    validOffset(_offset + MAX_BYTE_LENGTH - 1);

    u256 u = eth::fromBigEndian<u256>(data.cropped(_offset, MAX_BYTE_LENGTH));
    if (u > u256("0x8fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"))
    {
        auto r = (u256("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - u) +
            1;
        out = s256("-" + r.str());
    }
    else
    {
        out = u.convert_to<s256>();
    }
}

void
ContractABI::deserialise(u256& _out, std::size_t _offset)
{
    validOffset(_offset + MAX_BYTE_LENGTH - 1);

    _out = eth::fromBigEndian<u256>(data.cropped(_offset, MAX_BYTE_LENGTH));
}

void
ContractABI::deserialise(bool& _out, std::size_t _offset)
{
    validOffset(_offset + MAX_BYTE_LENGTH - 1);

    u256 ret = eth::fromBigEndian<u256>(data.cropped(_offset, MAX_BYTE_LENGTH));
    _out = ret > 0 ? true : false;
}

void
ContractABI::deserialise(Address& _out, std::size_t _offset)
{
    validOffset(_offset + MAX_BYTE_LENGTH - 1);
    bytesRef bytes(_out.data(), _out.size());
    data.cropped(_offset + MAX_BYTE_LENGTH - 20, 20).populate(bytes);
}

void
ContractABI::deserialise(string32& _out, std::size_t _offset)
{
    validOffset(_offset + MAX_BYTE_LENGTH - 1);

    data.cropped(_offset, MAX_BYTE_LENGTH)
        .populate(bytesRef((uint8_t*)_out.data(), MAX_BYTE_LENGTH));
}

void
ContractABI::deserialise(std::string& _out, std::size_t _offset)
{
    validOffset(_offset + MAX_BYTE_LENGTH - 1);

    u256 len = eth::fromBigEndian<u256>(data.cropped(_offset, MAX_BYTE_LENGTH));
    validOffset(_offset + MAX_BYTE_LENGTH + (std::size_t)len - 1);
    auto result =
        data.cropped(_offset + MAX_BYTE_LENGTH, static_cast<size_t>(len));
    _out.assign((const char*)result.data(), result.size());
}
}  // namespace ripple