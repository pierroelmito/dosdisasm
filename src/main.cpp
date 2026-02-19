
#include "dosdisasm.hpp"

#include <boost/algorithm/string.hpp>

#include "rupp.hpp"

#include "dumper.hpp"

namespace ru = rupp;

std::optional<ZyanI64> GetMemOperand(const ZydisDecodedOperand& op)
{
	if (op.type != ZYDIS_OPERAND_TYPE_MEMORY)
		return std::nullopt;
	//if (op.mem.segment != ZYDIS_REGISTER_NONE) return std::nullopt;
	if (op.mem.base != ZYDIS_REGISTER_NONE) return std::nullopt;
	if (op.mem.index != ZYDIS_REGISTER_NONE) return std::nullopt;
	auto v = op.mem.disp.value;
	return v;
}

std::optional<ZyanU64> IsShortJump(const ZydisDisassembledInstruction& instruction, ZyanU64 runtime_address, bool anySize)
{
	if (instruction.info.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE) {
		if (instruction.info.operand_count_visible == 1) {
			if (anySize || instruction.operands[0].size == 8) {
				ZyanU64 na {};
				ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], runtime_address, &na);
				return na;
			}
		}
	}
	return std::nullopt;
}

std::optional<ZyanU64> GetMemRef(const ZydisDisassembledInstruction& instruction, ZyanU64 runtime_address)
{
	if (instruction.info.mnemonic == ZYDIS_MNEMONIC_MOV) {
		if (instruction.info.operand_count_visible == 2) {
			if (auto mem = GetMemOperand(instruction.operands[0]); mem) {
				return mem;
			} else if (auto mem = GetMemOperand(instruction.operands[1]); mem) {
				return mem;
			}
		}
	}
	return std::nullopt;
}

AnalyzeLabels::AnalyzeLabels(std::set<ZyanU64>& el, JumpFlagsMap& jl)
	: existingLabels(el)
	, jumpLabels(jl)
{
}

void AnalyzeLabels::onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const
{
	existingLabels.insert(ctx.runtime_address);
	if (const auto ona = IsShortJump(instruction, ctx.runtime_address, true)) {
		const bool isCall = instruction.info.mnemonic == ZYDIS_MNEMONIC_CALL;
		jumpLabels[*ona] |= uint8_t(isCall ? JumpFlag::CALL : JumpFlag::JUMP);
	}
}

struct GenerateAsmColor : Dumper {
	FILE* const outFile {};
	GenerateAsmColor(const std::set<ZyanU64>& el, const JumpFlagsMap& jl, FILE* o)
		: Dumper(el, jl)
		, outFile(o)
	{
	}
	virtual void finish() const override
	{
		ru::put(ru::Reset);
	}
	virtual void dumpStr(const Ctx& ctx, DumpParams p, const char* const str) const override
	{
		ru::put(ru::FgGreen);
		if (p.label != nullptr)
			fprintf(outFile, "%s:\n", p.label);
		ru::put(p.ct != CType::Code ? ru::FgBlue : ru::FgWhite);
		fprintf(outFile, "  %s", str);
		if (p.comment) {
			ru::put(ru::FgCyan);
			fprintf(outFile, " ; %s", p.comment);
		} else {
			if (p.sz > 0) {
				ru::put(ru::FgCyan);
				if (p.sz < 8) {
					fprintf(outFile, " ; ");
					for (ZyanUSize i = 0; i < p.sz; ++i)
						fprintf(outFile, "%02X", ctx.data[i]);
				} else {
					fprintf(outFile, " ; %lu bytes", p.sz);
				}
				fprintf(outFile, " at %lu / 0x%0lX", ctx.offset, ctx.offset);
			}
		}
		fprintf(outFile, "\n");
	}
};

struct GenerateAsmNoColor : Dumper {
	FILE* const outFile {};
	GenerateAsmNoColor(const std::set<ZyanU64>& el, const JumpFlagsMap& jl, FILE* o)
		: Dumper(el, jl)
		, outFile(o)
	{
	}
	virtual void dumpStr(const Ctx& ctx, DumpParams p, const char* const str) const override
	{
		if (p.label != nullptr)
			fprintf(outFile, "%s:\n", p.label);
		fprintf(outFile, "  %s", str);
		if (p.comment) {
			fprintf(outFile, " ; %s", p.comment);
		} else {
			if (p.sz > 0) {
				if (p.sz < 8) {
					fprintf(outFile, " ; ");
					for (ZyanUSize i = 0; i < p.sz; ++i)
						fprintf(outFile, "%02X", ctx.data[i]);
				} else {
					fprintf(outFile, " ; %lu bytes", p.sz);
				}
				fprintf(outFile, " at %lu / 0x%0lX", ctx.offset, ctx.offset);
			}
		}
		fprintf(outFile, "\n");
	}
};

int main(int ac, char** av)
{
	// options
	po::variables_map vm;
	if (!HandleOptions(ac, av, vm))
		return 1;

	// load bin file
	const auto filename = vm["input"].as<std::string>();
	const auto content = ReadFile(filename.c_str());
	if (content.empty())
		return 1;

	// handle skip list
	MetaData meta;
	std::optional<std::string> workFilename;
	if (vm.count("workfile") != 0) {
		workFilename = vm["workfile"].as<std::string>();
		meta = MetaDataReadFromFile(*workFilename);
	}
	if (meta.skips.empty()) {
		{
			auto a = [&](auto&& nr) {
				meta.skips.insert(meta.skips.end(), nr.begin(), nr.end());
			};
			a(MakeRanges(vm));
			a(DetectStrings(content));
			a(DetectRepeats(content));
		}
		CleanRanges(meta.skips);
		if (workFilename) {
			MetaDataSaveToFile(meta, *workFilename);
		}
	}

	meta.labels = {
		{ 0x2D1, "pwait" },
		{ 0x2D5, "lwait" },
		{ 0x317, "set_cur" },
		{ 0x324, "read_cur" },
	};

	const ZyanU64 ra = 0x100;

	// run main loop
#if ENABLE_TUI
	if (vm.count("tui") != 0 && vm["tui"].as<bool>()) {
		Tui({ filename, workFilename.value_or({}), content, meta, ra });
		return 0;
	}
#endif
#if ENABLE_GUI
	if (vm.count("gui") != 0 && vm["gui"].as<bool>()) {
		Gui({ filename, workFilename.value_or({}), content, meta, ra });
		return 0;
	}
#endif
	{
		Process prc;
		std::set<ZyanU64> existingLabels;
		JumpFlagsMap jumpLabels;
		{
			AnalyzeLabels anLbl { existingLabels, jumpLabels };
			prc.loop(ra, content, meta, anLbl);
		}
		if (vm.count("output") != 0) {
			FILE* out = fopen(vm["output"].as<std::string>().c_str(), "w");
			GenerateAsmNoColor genAsm { existingLabels, jumpLabels, out };
			prc.loop(ra, content, meta, genAsm);
			fclose(out);
		} else {
			FILE* out = stdout;
			GenerateAsmColor genAsm { existingLabels, jumpLabels, out };
			prc.loop(ra, content, meta, genAsm);
		}
	}

	return 0;
}
