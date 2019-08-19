#include <peersafe/app/consensus/ViewChange.h>

namespace ripple {

	uint256
		viewChangeUniqueId(
			std::uint32_t const prevSeq,
			uint256 const& prevHash,
			PublicKey const nodePublic,
			std::uint64_t const& toView
		)
	{
		Serializer s(512);
		s.add32(prevSeq);
		s.add256(prevHash);
		s.addVL(nodePublic);
		s.add64(toView);

		return s.getSHA512Half();
	}
}