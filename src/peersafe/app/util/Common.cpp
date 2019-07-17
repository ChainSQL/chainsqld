#include <chrono>
#include <peersafe/app/util/Common.h>

namespace ripple {
	/// get utc time(ms)
	uint64_t utcTime()
	{
		auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
		auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
		return tmp.count();
		//std::time_t timestamp = std::chrono::system_clock::to_time_t(tp);
	}
}