
#pragma once

#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include <Zydis/Formatter.h>
#include <Zydis/Zydis.h>

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

using Skips = std::vector<std::pair<size_t, size_t>>;
using Content = std::vector<ZyanU8>;

struct Process {
	ZydisDecoder decoder {};
	ZydisFormatter formatter {};
	Process();
	void loop(const Content& content, const Skips& skip_ranges, const Cb& cb);
};

enum class CType {
	Code,
	Dup,
	Db,
};

struct Item {
	ZyanU64 ra {};
	ZyanUSize sz {};
	std::string label;
	std::string asmc;
	CType ct {};
};

using Listing = std::vector<Item>;

struct TuiCtx {
	const Content& content;
	Listing l;
	size_t start {};
	size_t s {};
};

Content ReadFile(const char* filename);
size_t FromString(const std::string& str);
bool HandleOptions(int ac, char** av, po::variables_map& vm);
Skips DetectStrings(const Content& content);
Skips DetectRepeats(const Content& content);
Skips MakeRanges(const po::variables_map& vm);
void CleanRanges(Skips& ranges);
void Tui(const Content& content, Listing&& listing);

inline bool printable(char c)
{
	return (c >= 32 && c < 127);
}
