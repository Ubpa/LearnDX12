#include "Executor.h"

#include "RsrcMngr.h"

using namespace Ubpa::DX12;

void FG::Executor::Execute(
	const Ubpa::FG::FrameGraph& fg,
	const Ubpa::FG::Compiler::Result& crst,
	RsrcMngr& rsrcMngr)
{
	rsrcMngr.DHReserve();
	rsrcMngr.AllocateHandle();
	const auto& passnodes = fg.GetPassNodes();
	for (auto passNodeIdx : crst.sortedPasses) {
		const auto& passinfo = crst.idx2info.find(passNodeIdx)->second;

		for (const auto& rsrc : passinfo.constructRsrcs)
			rsrcMngr.Construct(rsrc);

		auto passRsrcs = rsrcMngr.RequestPassRsrcs(passNodeIdx);
		passFuncs[passNodeIdx](passRsrcs);

		//cout << "[Execute] " << passnodes[i].Name() << endl;

		for (const auto& rsrc : passinfo.destructRsrcs)
			rsrcMngr.Destruct(rsrc);
	}
}
