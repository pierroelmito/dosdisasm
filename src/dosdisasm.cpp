
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <ranges>
#include <set>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <Zydis/Formatter.h>
#include <Zydis/Zydis.h>

#include "rogueutil.hpp"

namespace po = boost::program_options;
namespace ru = rogueutil;

std::vector<ZyanU8> read_file(const char* filename)
{
	FILE* file = fopen(filename, "rb");
	if (!file)
		return {};
	std::vector<ZyanU8> buffer;
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	buffer.resize(size);
	fread(buffer.data(), 1, size, file);
	fclose(file);
	// printf("; Read %zu bytes from file.\n", content.size());
	return buffer;
}

size_t FromString(const std::string& str)
{
	size_t result = 0;
	const int base = [&]() {
		if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
			return 16;
		}
		return 0;
	}();
	try {
		result = std::stoull(str, nullptr, base);
	} catch (...) {
	}
	return result;
}

bool HandleOptions(int ac, char** av, po::variables_map& vm)
{
	po::options_description desc("Allowed options");
	desc.add_options()("help,h", "produce help message")(
		"tui", po::bool_switch(), "use interface")(
		"input,i", po::value<std::string>(), "input binary file to disassemble")(
		"output,o", po::value<std::string>(),
		"output asm file")("skip,k", po::value<std::vector<std::string>>(),
		"<start>,<length> skip <length> bytes from <start>");

	po::store(po::parse_command_line(ac, av, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return false;
	}

	if (vm.count("input") == 0) {
		std::cout << "No input file specified.\n";
		return false;
	}

	return true;
}

inline bool printable(char c)
{
	return (c >= 32 && c < 127);
}

std::vector<std::pair<size_t, size_t>> DetectStrings(const std::vector<ZyanU8>& content)
{
	std::vector<std::pair<size_t, size_t>> r;
	const ZyanUSize size = content.size();
	const ZyanU8* const data = content.data();
	for (auto i = 0u; i < size; ++i) {
		const ZyanU8 byte = data[i];
		if (printable(byte)) {
			auto itf = std::find_if(data + i, data + size, [&](char c) {
				return !printable(c);
			});
			if (itf != data + size && *itf == 0)
				++itf;
			const auto slen = std::distance(data + i, itf);
			if (slen > 5) {
				r.push_back({ i, slen });
				i += slen - 1;
				continue;
			}
		}
	}
	return r;
}

std::vector<std::pair<size_t, size_t>> DetectRepeats(const std::vector<ZyanU8>& content)
{
	std::vector<std::pair<size_t, size_t>> r;
	const ZyanUSize size = content.size();
	const ZyanU8* const data = content.data();
	for (auto i = 0u; i < size - 1; ++i) {
		const ZyanU8 byte = data[i];
		if (byte == data[i + 1]) {
			auto itf = std::find_if(data + i, data + size, [&](char c) {
				return c != byte;
			});
			const auto slen = std::distance(data + i, itf);
			if (slen > 8) {
				r.push_back({ i, slen });
				i += slen - 1;
				continue;
			}
		}
	}
	return r;
}

std::vector<std::pair<size_t, size_t>>
MakeRanges(const po::variables_map& vm)
{
	std::vector<std::pair<size_t, size_t>> skip_ranges;
	if (vm.count("skip") != 0) {
		const auto skips = vm["skip"].as<std::vector<std::string>>();
		for (const auto& skip : skips) {
			std::vector<std::string> s;
			boost::algorithm::split(
				s,
				skip,
				boost::is_any_of(","));
			if (s.size() == 2) {
				const size_t start = FromString(s[0]);
				const size_t length = FromString(s[1]);
				skip_ranges.emplace_back(start, length);
			}
		}
	}
	return skip_ranges;
}

void CleanRanges(std::vector<std::pair<size_t, size_t>>& ranges)
{
	std::sort(
		ranges.begin(),
		ranges.end(),
		[](const auto& a, const auto& b) {
			return a.first < b.first;
		});

	for (const auto& [i, range] : std::views::enumerate(ranges)) {
		if (i < int(ranges.size()) - 1) {
			auto& next_range = ranges[i + 1];
			if (range.first + range.second > next_range.first) {
				range.second = std::max(range.second, (next_range.first + next_range.second) - range.first);
				next_range = {};
			}
		}
	}

	ranges.erase(
		std::remove_if(
			ranges.begin(),
			ranges.end(),
			[](const auto& range) {
				return range.second == 0;
			}),
		ranges.end());

	// for (const auto& range : ranges)
	// 	printf("; Skip %zu bytes from offset %zu\n", range.second, range.first);
}

struct Process;

struct Cb {
	struct Ctx {
		const Process& p;
		ZyanU64 runtime_address;
		const ZyanU8* const data;
		ZyanUSize offset;
	};
	virtual void start() const { };
	virtual void finish() const { };
	virtual void onSkip(const Ctx&, ZyanUSize /*size*/) const { }
	virtual void onUnkByte(const Ctx&, ZyanU8 /*skip*/) const { }
	virtual void onIns(const Ctx&, const ZydisDisassembledInstruction& /*instruction*/) const { }
};

struct Process {
	ZydisDecoder decoder {};
	ZydisFormatter formatter {};
	Process()
	{
		ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_REAL_16, ZYDIS_STACK_WIDTH_16);
		ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
	}
	void loop(const std::vector<ZyanU8>& content, const std::vector<std::pair<size_t, size_t>>& skip_ranges, const Cb& cb)
	{
		const size_t file_size = content.size();
		const ZyanU8* const data = &content[0];

		ZyanU64 runtime_address = 0x100;
		ZyanUSize offset = 0;
		ZydisDisassembledInstruction instruction;
		auto itskip = skip_ranges.begin();

		cb.start();

		while (offset < file_size) {
			if (itskip != skip_ranges.end()) {
				const auto& [skip_start, skip_length] = *itskip;
				if (offset >= skip_start && offset < skip_start + skip_length) {
					const auto next_offset = skip_start + skip_length;
					const auto delta = next_offset - offset;
					cb.onSkip({ *this, runtime_address, data + offset, offset }, next_offset - offset);
					offset += delta;
					runtime_address += delta;
					++itskip;
					continue;
				}
			}
			auto nsync = itskip != skip_ranges.end() ? itskip->first : file_size;
			const bool ok = ZYAN_SUCCESS(ZydisDisassembleIntel(
				// ZYDIS_MACHINE_MODE_LONG_COMPAT_16,
				// ZYDIS_MACHINE_MODE_LEGACY_16,
				ZYDIS_MACHINE_MODE_REAL_16,
				runtime_address,
				data + offset,
				nsync - offset,
				&instruction));
			if (!ok) {
				cb.onUnkByte({ *this, runtime_address, data + offset, offset }, data[offset]);
				offset += 1;
				runtime_address += 1;
			} else {
				cb.onIns({ *this, runtime_address, data + offset, offset }, instruction);
				offset += instruction.info.length;
				runtime_address += instruction.info.length;
			}
		}

		if (offset < file_size) {
			printf("; missing %lu bytes", file_size - offset);
		}

		cb.finish();
	}
};

bool isShortJump(const ZydisDisassembledInstruction& instruction)
{
	if (instruction.info.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE) {
		if (instruction.info.operand_count_visible == 1) {
			if (instruction.operands[0].size == 8)
				return true;
		}
	}
	return false;
}

struct AnalyzeLabels : Cb {
	std::set<ZyanU64>& existingLabels;
	std::set<ZyanU64>& jumpLabels;
	AnalyzeLabels(std::set<ZyanU64>& el, std::set<ZyanU64>& jl)
		: existingLabels(el)
		, jumpLabels(jl)
	{
	}
	virtual void onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const override
	{
		existingLabels.insert(ctx.runtime_address);
		if (isShortJump(instruction)) {
			ZyanU64 na {};
			ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], ctx.runtime_address, &na);
			jumpLabels.insert(na);
		}
	}
};

struct Dumper : Cb {
	const std::set<ZyanU64>& existingLabels;
	const std::set<ZyanU64>& jumpLabels;
	Dumper(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl)
		: existingLabels(el)
		, jumpLabels(jl)
	{
	}
	virtual void dumpStr(const Ctx& ctx, bool skip, ZyanUSize sz, const char* label, const char* const str) const = 0;
	template <typename... T>
	inline constexpr void dump(const Ctx& ctx, bool skip, ZyanUSize sz, const char* label, const char* const fmt, const T&... args) const
	{
		char buffer[2048] {};
		snprintf(buffer, sizeof(buffer), fmt, args...);
		dumpStr(ctx, skip, sz, label, buffer);
	}
	virtual void onSkip(const Ctx& ctx, ZyanUSize size) const override
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
				dump(ctx, true, size, label, "  times %d db 0x%02X", size, *b);
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
						dump(mkCtx(lastNl), true, i - lastNl, nullptr, "%s", buffer);
					}
					memcpy(buffer, ctx.data + i, slen);
					buffer[slen] = 0;
					char label[64];
					snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address + i);
					dump(mkCtx(i), true, slen, label, "  db \"%s\"", buffer);
					lastNl = i + slen;
					i += slen - 1;
					bi = 0;
					continue;
				}
			}
			if (bi == 0)
				bi += snprintf(buffer + bi, sizeof(buffer) - bi, "  db 0x%02X", byte);
			else
				bi += snprintf(buffer + bi, sizeof(buffer) - bi, ", 0x%02X", byte);
			if (lastNl + 15 == i) {
				dump(mkCtx(lastNl), true, 1 + i - lastNl, nullptr, "%s", buffer);
				lastNl = i + 1;
				bi = 0;
			}
		}
		if (bi != 0) {
			dump(mkCtx(lastNl), true, size - lastNl, nullptr, "%s", buffer);
		}
	}
	virtual void onUnkByte(const Ctx& ctx, ZyanU8 skip) const override
	{
		dump(ctx, 1, true, nullptr, "  db 0x%02X", skip);
	}
	virtual void onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const override
	{
		const char* label = nullptr;
		char buffer[64];
		if (jumpLabels.find(ctx.runtime_address) != jumpLabels.end()) {
			snprintf(buffer, sizeof(buffer), "l0x%04" PRIX64, ctx.runtime_address);
			label = buffer;
		}
		if (isShortJump(instruction)) {
			std::vector<std::string> s;
			boost::algorithm::split(
				s,
				instruction.text,
				boost::is_any_of(" "));
			if (s.size() == 2) {
				if (existingLabels.find(ctx.runtime_address) != existingLabels.end()) {
					dump(ctx, false, instruction.info.length, label, "  %s l%s", s[0].c_str(), s[1].c_str());
				} else {
					dump(ctx, false, instruction.info.length, label, "  %s short %s", s[0].c_str(), s[1].c_str());
				}
			} else {
				dump(ctx, false, instruction.info.length, label, "  %s", instruction.text);
			}
		} else {
			dump(ctx, false, instruction.info.length, label, "  %s", instruction.text);
		}
	}
};

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
	virtual void dumpStr(const Ctx& ctx, bool skip, ZyanUSize sz, const char* label, const char* const str) const override
	{
		ru::tprint(ru::Color::GREEN);
		if (label != nullptr)
			fprintf(outFile, "%s:\n", label);
		ru::tprint(skip ? ru::Color::BLUE : ru::Color::WHITE);
		fprintf(outFile, "%s", str);
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
	virtual void dumpStr(const Ctx& ctx, bool, ZyanUSize sz, const char* label, const char* const str) const override
	{
		if (label != nullptr)
			fprintf(outFile, "%s:\n", label);
		fprintf(outFile, "%s", str);
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

struct Item {
	ZyanU64 ra;
	ZyanUSize sz;
	std::string label;
	std::string asmc;
	bool skip;
};

using Listing = std::vector<Item>;

struct TuiCtx {
	const std::vector<ZyanU8>& content;
	Listing l;
	size_t s {};
};

struct LoadCode : Dumper {
	Listing& listing;
	LoadCode(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, Listing& l)
		: Dumper(el, jl)
		, listing(l)
	{
	}
	virtual void dumpStr(const Ctx& ctx, bool skip, ZyanUSize sz, const char* label, const char* const str) const override
	{
		listing.push_back({ ctx.runtime_address, sz, label ? label : "", str, skip });
	}
};

void tui_main(TuiCtx& ctx)
{
	std::string spaces;
	auto draw = [&](ru::Vec d) {
		using V = ru::Vec;
		using A = std::pair<ru::Color, ru::Color>;
		if (spaces.size() != d.x)
			spaces = std::string(d.x, ' ');
		ru::cls();
		ru::tprint(V { 1, 1 }, A{ ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
		ru::tprint(V { 1, d.y }, A{ ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
		ru::tprint(V { 1, 1 }, ru::Color::WHITE, "%d %d", d.x, d.y);
		ru::resetColor();
		for (int i = 0u; i < d.y - 4; ++i) {
			if (i < ctx.l.size()) {
				const bool selected = i == ctx.s;
				auto col = selected ? A { ru::Color::WHITE, ru::Color::RED } : A { ru::Color::BROWN, ru::Color::NONE };
				ru::tprint(col);
				const auto& o = ctx.l[i];
				ru::tprint(V { 1, i + 2 });
				ru::tprint(o.skip ? ru::Color::BLUE : ru::Color::WHITE);
				int lsz = ru::tprint("%3d ", i);
				if (o.label.empty())
					lsz += ru::tprint(ru::Color::BROWN, "%04X ", o.ra);
				else
					lsz += ru::tprint(ru::Color::GREEN, "%s: ", o.label.c_str());
				lsz += ru::tprint(ru::Color::WHITE, "%s", o.asmc.c_str());
				ru::tprint("%s", spaces.data() + lsz);
				if (selected)
					ru::resetColor();
			}
		}
		fflush(stdout);
	};

	auto cd = ru::dim();
	draw(cd);
	for (;;) {
		if (ru::kbhit()) {
			char k = ru::getkey();
			if (k == 'q' || k == ru::KeyCode::KEY_ESCAPE) {
				break;
			} else if (k == 'e') {
			} else if (k == ru::KeyCode::KEY_UP) {
				if (ctx.s > 0)
					ctx.s--;
			} else if (k == ru::KeyCode::KEY_DOWN) {
				if (ctx.s < ctx.l.size() - 1)
					ctx.s++;
			}
			draw(cd);
		} else {
			const auto nd = ru::dim();
			if (cd != nd) {
				cd = nd;
				draw(cd);
			}
		}
	}
}

void tui(const std::vector<ZyanU8>& content, Listing&& listing)
{
	TuiCtx ctx { content, listing, 0 };
	tui_main(ctx);
}

int main(int ac, char** av)
{
	// options
	po::variables_map vm;
	if (!HandleOptions(ac, av, vm))
		return 1;

	// load bin file
	const std::vector<ZyanU8> content = read_file(vm["input"].as<std::string>().c_str());
	if (content.empty())
		return 1;

	// handle skip list
	std::vector<std::pair<size_t, size_t>> skip_ranges;
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
	Process prc;
	std::set<ZyanU64> existingLabels;
	std::set<ZyanU64> jumpLabels;

	{
		AnalyzeLabels anLbl { existingLabels, jumpLabels };
		prc.loop(content, skip_ranges, anLbl);
	}

	if (vm.count("tui") != 0 && vm["tui"].as<bool>()) {
		Listing l;
		LoadCode load { existingLabels, jumpLabels, l };
		prc.loop(content, skip_ranges, load);
		ru::cls();
		ru::setCursor(false);
		tui(content, std::move(l));
		ru::cls();
		ru::setCursor(true);
		ru::resetColor();
	} else {
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
