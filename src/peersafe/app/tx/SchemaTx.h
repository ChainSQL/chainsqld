#ifndef RIPPLE_APP_TX_SCHEMATX_H_INCLUDED
#define RIPPLE_APP_TX_SCHEMATX_H_INCLUDED

#include <ripple/app/tx/impl/Transactor.h>#pragma once

namespace ripple {

	class SchemaTx : public Transactor
	{		
	public:
		explicit
			SchemaTx(ApplyContext& ctx)
			: Transactor(ctx)
		{
		}
		static
			NotTEC
			preflight(PreflightContext const& ctx);

		static
			TER
			preclaim(PreclaimContext const& ctx);

		TER doApply() override;
	};

}

#endif