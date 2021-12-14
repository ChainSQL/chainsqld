#ifndef RIPPLE_APP_TX_SCHEMATX_H_INCLUDED
#define RIPPLE_APP_TX_SCHEMATX_H_INCLUDED

#include <ripple/app/tx/impl/Transactor.h>

namespace ripple {

	class SchemaCreate : public Transactor
	{		
	public:
		explicit
			SchemaCreate(ApplyContext& ctx)
			: Transactor(ctx)
		{
		}
		static NotTEC preflight(PreflightContext const& ctx);

		static TER preclaim(PreclaimContext const& ctx);

		TER doApply() override;
	};

	class SchemaModify : public Transactor
	{
	public:
		explicit
			SchemaModify(ApplyContext& ctx)
			: Transactor(ctx)
		{
		}
		static NotTEC preflight(PreflightContext const& ctx);

		static TER preclaim(PreclaimContext const& ctx);

		TER doApply() override;
	};

	
	class SchemaDelete : public Transactor
	{
	public:
		explicit
			SchemaDelete(ApplyContext& ctx)
			: Transactor(ctx)
		{
		}
		static NotTEC preflight(PreflightContext const& ctx);

		static TER preclaim(PreclaimContext const& ctx);

		TER doApply() override;
	};
}

#endif