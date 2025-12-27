
#pragma once

#include <array>
#include <functional>
#include <string>
#include <tuple>
#include <vector>

#include <boost/program_options.hpp>

#include <Zydis/Formatter.h>
#include <Zydis/Zydis.h>

template <size_t N, typename... T>
std::string Fmt(const char* fmt, T... args)
{
	char buffer[N];
	snprintf(buffer, N, fmt, args...);
	return buffer;
}

namespace po = boost::program_options;

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

struct AnalyzeLabels : Cb {
	std::set<ZyanU64>& existingLabels;
	std::set<ZyanU64>& jumpLabels;
	AnalyzeLabels(std::set<ZyanU64>& el, std::set<ZyanU64>& jl);
	virtual void onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const override;
};

enum class CType {
	Code,
	Dup,
	Db,
};

struct Tracker {
	struct Reg {
		union {
			uint16_t X;
			struct {
				uint8_t L;
				uint8_t H;
			};
		};
	};
	std::array<Reg, 4> regs;
};

struct Dumper : Cb {
	const std::set<ZyanU64>& existingLabels;
	const std::set<ZyanU64>& jumpLabels;
	std::map<ZyanU64, const std::string> userLabels;
	mutable Tracker trk {};
	Dumper(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl);
	virtual void dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, std::optional<ZyanU64> jump, const char* const str, const char* const comment) const = 0;
	template <typename... T>
	inline constexpr void dump(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, std::optional<ZyanU64> jump, const char* comment, const char* const fmt, const T&... args) const
	{
		char buffer[2048] {};
		snprintf(buffer, sizeof(buffer), fmt, args...);
		dumpStr(ctx, ct, sz, label, jump, buffer, comment);
	}
	virtual void onSkip(const Ctx& ctx, ZyanUSize size) const override;
	virtual void onUnkByte(const Ctx& ctx, ZyanU8 skip) const override;
	virtual void onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const override;
	std::string getComment(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const;
};

using Skips = std::vector<std::pair<size_t, size_t>>;
using Content = std::vector<ZyanU8>;

struct Process {
	ZydisDecoder decoder {};
	ZydisFormatter formatter {};
	Process();
	void loop(ZyanU64 ra, const Content& content, const Skips& skip_ranges, const Cb& cb);
};

enum class Cmp {
	None,
	Same,
	Equi,
	Diff,
};

struct Item {
	ZyanU64 ra {};
	std::optional<ZyanU64> jump {};
	ZyanUSize sz {};
	std::string label;
	std::string asmc;
	std::string comment;
	CType ct {};
	Cmp cmp { Cmp::None };
};

using Listing = std::vector<Item>;

Content ReadFile(const char* filename);
size_t FromString(const std::string& str);
bool HandleOptions(int ac, char** av, po::variables_map& vm);
Skips DetectStrings(const Content& content);
Skips DetectRepeats(const Content& content);
Skips MakeRanges(const po::variables_map& vm);
void CleanRanges(Skips& ranges);
void Tui(const Content& content, const Skips& skips);
void Gui(const Content& content, const Skips& skips);

inline bool printable(char c)
{
	if (c == '`')
		return false;
	return (c >= 32 && c < 127);
}
