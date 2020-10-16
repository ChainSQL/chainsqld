#include <peersafe/app/tx/SchemaTx.h>

namespace ripple {
	NotTEC SchemaTx::preflight(PreflightContext const& ctx)
	{
		return tesSUCCESS;
	}

	TER SchemaTx::preclaim(PreclaimContext const& ctx)
	{
		return tesSUCCESS;
	}

	TER SchemaTx::doApply()
	{
		return tesSUCCESS;
	}
}
