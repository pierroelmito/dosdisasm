
#include "dosdisasm.hpp"

#include <boost/algorithm/string.hpp>

#include "rupp.hpp"

#include "dumper.hpp"

namespace ru = rupp;

std::optional<ZyanI64> GetMemOperand(const ZydisDecodedOperand& op)
{
	if (op.type != ZYDIS_OPERAND_TYPE_MEMORY)
		return std::nullopt;
	// if (op.mem.segment != ZYDIS_REGISTER_NONE) return std::nullopt;
	if (op.mem.base != ZYDIS_REGISTER_NONE)
		return std::nullopt;
	if (op.mem.index != ZYDIS_REGISTER_NONE)
		return std::nullopt;
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

void AnalyzeLabels::onSkip(const Ctx& ctx, ZyanUSize /*size*/) const
{
	existingLabels.insert(ctx.runtime_address);
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

	int varIndex{};
	const auto newVar = [&] () {
		return Fmt<32>("var_%03d", varIndex++);
	};

	int fnIndex{};
	const auto newFn = [&] () {
		return Fmt<32>("fn_%03d", fnIndex++);
	};

#if 0
	meta.labels = {
		{ 0x103, newVar() },
		{ 0x123, "sz_Infogrames" },
		{ 0x140, "sz_FR" },
		{ 0x146, "sz_GB" },
		{ 0x14C, "sz_E" },
		{ 0x151, "sz_D" },
		{ 0x156, "p_szFR" },
		{ 0x158, "p_szGB" },
		{ 0x15A, "p_szE" },
		{ 0x15C, "p_szD" },
		{ 0x15E, "sz_PC1512" },
		{ 0x171, "sz_Tandy" },
		{ 0x17A, "sz_OtherFR" },
		{ 0x183, "sz_OtherEN" },
		{ 0x18C, "sz_OtherES" },
		{ 0x1A1, "sz_OtherDE" },
		{ 0x1AB, "p_szPC1512" },
		{ 0x1AD, "p_szTandy" },
		{ 0x1AF, "p_szOther" },
		{ 0x1B1, "sz_SlowFR" },
		{ 0x1C2, "sz_FastFR" },
		{ 0x1D4, "sz_FastEN" },
		{ 0x1E5, "sz_SlowEN" },
		{ 0x1F6, "sz_SlowES" },
		{ 0x209, "sz_FastES" },
		{ 0x21D, "sz_FastDE" },
		{ 0x233, "sz_SlowDE" },
		{ 0x249, "p_szSlow" },
		{ 0x24B, "p_szFast" },
		{ 0x24D, "sz_Prohibition" },
		{ 0x265, "sz_Version" },
		{ 0x28C, "var_idxComputer" },
		{ 0x28D, "sz_Image" },
		{ 0x297, "sz_P1" },
		{ 0x29E, "sz_P2" },
		{ 0x2A5, "sz_P3" },
		{ 0x2AC, "sz_P4" },
		{ 0x2B3, "p_szP1" },
		{ 0x2B5, "p_szP2" },
		{ 0x2B7, "p_szP3" },
		{ 0x2B9, "p_szP4" },
		{ 0x2D1, "fn_pwait" },
		{ 0x2D5, "lwait" },
		{ 0x2D9, newFn() },
		{ 0x2F3, newFn() },
		{ 0x2FE, newFn() },
		{ 0x317, "fn_set_cur" },
		{ 0x324, "fn_read_cur" },
		{ 0x331, newFn() },
		{ 0x33A, newFn() },
		{ 0x342, newFn() },
		{ 0x351, newFn() },
		{ 0x370, newFn() },
		{ 0x3CB, newFn() },
		{ 0x426, newFn() },
		{ 0x473, newFn() },
		{ 0x48E, newFn() },
		{ 0x4A2, "fn_print_str" },
		{ 0x6A1, newVar() },
		{ 0x759, newVar() },
		{ 0x75A, newVar() },
		{ 0x801, "fn_set_lang" },
		{ 0x815, "set_EN" },
		{ 0x82D, "set_ES" },
		{ 0x845, "set_DE" },
		{ 0x85A, "keep_FR" },
		{ 0x929, newVar() },
		{ 0xF35, newFn() },
		{ 0xF46, newFn() },
		{ 0x10C7, newFn() },
	};
#endif

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
