
#include "dosdisasm.hpp"

#include <cinttypes>

#include <boost/algorithm/string.hpp>

#include "rogueutil.hpp"

namespace ru = rogueutil;

bool isShortJump(const ZydisDisassembledInstruction& instruction, bool anySize)
{
	if (instruction.info.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE) {
		if (instruction.info.operand_count_visible == 1) {
			if (anySize || instruction.operands[0].size == 8)
				return true;
		}
	}
	return false;
}

AnalyzeLabels::AnalyzeLabels(std::set<ZyanU64>& el, std::set<ZyanU64>& jl)
	: existingLabels(el)
	, jumpLabels(jl)
{
}

void AnalyzeLabels::onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const
{
	existingLabels.insert(ctx.runtime_address);
	if (isShortJump(instruction, true)) {
		ZyanU64 na {};
		ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], ctx.runtime_address, &na);
		jumpLabels.insert(na);
	}
}

Dumper::Dumper(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl)
	: existingLabels(el)
	, jumpLabels(jl)
{
}

void Dumper::onSkip(const Ctx& ctx, ZyanUSize size) const
{
	if (size > 4) {
		auto* b = ctx.data;
		auto* e = ctx.data + size;
		auto itf = std::find_if(b, e, [&](auto c) {
			return c != *b;
		});
		if (itf == e) {
			char label[64];
			snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address);
			dump(ctx, CType::Dup, size, label, "times %d db 0x%02X", size, *b);
			return;
		}
	}
	char buffer[2048];
	char bi {};
	auto mkCtx = [&](auto i) -> Ctx {
		return {
			ctx.p,
			ctx.runtime_address + i,
			ctx.data + i,
			ctx.offset + i
		};
	};
	ZyanUSize lastNl = 0;
	for (auto i = 0u; i < size; ++i) {
		const ZyanU8 byte = ctx.data[i];
		if (printable(byte)) {
			const auto itf = std::find_if(ctx.data + i, ctx.data + size, [&](char c) {
				return !printable(c);
			});
			const auto slen = std::distance(ctx.data + i, itf);
			if (slen > 5) {
				if (bi != 0) {
					dump(mkCtx(lastNl), CType::Dup, i - lastNl, nullptr, "%s", buffer);
				}
				memcpy(buffer, ctx.data + i, slen);
				buffer[slen] = 0;
				char label[64];
				snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address + i);
				if (slen < size && ctx.data[slen] == 0) {
					dump(mkCtx(i), CType::Dup, slen, label, "db \"%s\", 0", buffer);
					lastNl = i + slen + 1;
					i += slen;
				} else {
					dump(mkCtx(i), CType::Dup, slen, label, "db \"%s\"", buffer);
					lastNl = i + slen;
					i += slen - 1;
				}
				bi = 0;
				continue;
			}
		}
		if (bi == 0)
			bi += snprintf(buffer + bi, sizeof(buffer) - bi, "db 0x%02X", byte);
		else
			bi += snprintf(buffer + bi, sizeof(buffer) - bi, ", 0x%02X", byte);
		if (lastNl + 15 == i) {
			char label[64];
			snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address);
			dump(mkCtx(lastNl), CType::Dup, 1 + i - lastNl, lastNl == 0 ? label : nullptr, "%s", buffer);
			lastNl = i + 1;
			bi = 0;
		}
	}
	if (bi != 0) {
		char label[64];
		snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address);
		dump(mkCtx(lastNl), CType::Dup, size - lastNl, lastNl == 0 ? label : nullptr, "%s", buffer);
	}
}

void Dumper::onUnkByte(const Ctx& ctx, ZyanU8 skip) const
{
	dump(ctx, CType::Db, 1, nullptr, "db 0x%02X", skip);
}

void Dumper::onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const
{
	const auto ct = CType::Code;
	const char* label = nullptr;
	char buffer[64];
	if (jumpLabels.find(ctx.runtime_address) != jumpLabels.end()) {
		snprintf(buffer, sizeof(buffer), "l0x%04" PRIX64, ctx.runtime_address);
		label = buffer;
	}
	std::vector<std::string> s;
	boost::algorithm::split(
		s,
		instruction.text,
		boost::is_any_of(" "));
	if (s.size() >= 2 && s[0] == "ret" && s[1] == "far") {
		s.erase(s.begin());
		s[0] = "retf";
	}
	for (auto& w : s)
		if (w == "ptr")
			w = "near";
	// s.erase(std::remove(s.begin(), s.end(), "ptr"), s.end());
	// const bool isJmp = s[0] == "jmp";
	const bool isCall = s[0] == "call";
	if (isShortJump(instruction, isCall)) {
		ZyanU64 na {};
		ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], ctx.runtime_address, &na);
		if (s.size() == 2) {
			// const char* prefix = isCall ? " near" : " short";
			const char* prefix = "";
			if (existingLabels.find(na) != existingLabels.end()) {
				// dump(ctx, ct, instruction.info.length, label, "%s near l%s", s[0].c_str(), s[1].c_str());
				dump(ctx, ct, instruction.info.length, label, "%s%s l%s", s[0].c_str(), prefix, s[1].c_str());
			} else {
				dump(ctx, ct, instruction.info.length, label, "%s%s %s", s[0].c_str(), prefix, s[1].c_str());
			}
		} else {
			const std::string j = boost::algorithm::join(s, " ");
			dump(ctx, ct, instruction.info.length, label, "%s", j.c_str());
		}
	} else {
		const std::string j = boost::algorithm::join(s, " ");
		dump(ctx, ct, instruction.info.length, label, "%s", j.c_str());
	}
}

struct GenerateAsmColor : Dumper {
	FILE* const outFile {};
	GenerateAsmColor(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, FILE* o)
		: Dumper(el, jl)
		, outFile(o)
	{
	}
	virtual void finish() const override
	{
		ru::resetColor();
	}
	virtual void dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, const char* const str) const override
	{
		ru::tprint(ru::Color::GREEN);
		if (label != nullptr)
			fprintf(outFile, "%s:\n", label);
		ru::tprint(ct != CType::Code ? ru::Color::BLUE : ru::Color::WHITE);
		fprintf(outFile, "  %s", str);
		if (sz > 0) {
			ru::tprint(ru::Color::CYAN);
			if (sz < 8) {
				fprintf(outFile, " ; ");
				for (ZyanUSize i = 0; i < sz; ++i)
					fprintf(outFile, "%02X", ctx.data[i]);
			} else {
				fprintf(outFile, " ; %lu bytes", sz);
			}
			fprintf(outFile, " at %lu / 0x%0lX", ctx.offset, ctx.offset);
		}
		fprintf(outFile, "\n");
	}
};

struct GenerateAsmNoColor : Dumper {
	FILE* const outFile {};
	GenerateAsmNoColor(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, FILE* o)
		: Dumper(el, jl)
		, outFile(o)
	{
	}
	virtual void dumpStr(const Ctx& ctx, CType, ZyanUSize sz, const char* label, const char* const str) const override
	{
		if (label != nullptr)
			fprintf(outFile, "%s:\n", label);
		fprintf(outFile, "  %s", str);
		if (sz > 0) {
			if (sz < 8) {
				fprintf(outFile, " ; ");
				for (ZyanUSize i = 0; i < sz; ++i)
					fprintf(outFile, "%02X", ctx.data[i]);
			} else {
				fprintf(outFile, " ; %lu bytes", sz);
			}
			fprintf(outFile, " at %lu / 0x%0lX", ctx.offset, ctx.offset);
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
	const auto content = ReadFile(vm["input"].as<std::string>().c_str());
	if (content.empty())
		return 1;

	// handle skip list
	Skips skip_ranges;
	{
		auto a = [&](auto&& nr) {
			skip_ranges.insert(skip_ranges.end(), nr.begin(), nr.end());
		};
		a(MakeRanges(vm));
		a(DetectStrings(content));
		a(DetectRepeats(content));
	}
	CleanRanges(skip_ranges);

	// run main loop
	if (vm.count("tui") != 0 && vm["tui"].as<bool>()) {
		ru::cls();
		ru::setCursor(false);
		Tui(content, skip_ranges);
		ru::cls();
		ru::setCursor(true);
		ru::resetColor();
	} else {
		Process prc;
		std::set<ZyanU64> existingLabels;
		std::set<ZyanU64> jumpLabels;
		{
			AnalyzeLabels anLbl { existingLabels, jumpLabels };
			prc.loop(content, skip_ranges, anLbl);
		}
		if (vm.count("output") != 0) {
			FILE* out = fopen(vm["output"].as<std::string>().c_str(), "w");
			GenerateAsmNoColor genAsm { existingLabels, jumpLabels, out };
			prc.loop(content, skip_ranges, genAsm);
			fclose(out);
		} else {
			FILE* out = stdout;
			GenerateAsmColor genAsm { existingLabels, jumpLabels, out };
			prc.loop(content, skip_ranges, genAsm);
		}
	}

	return 0;
}
