
#pragma once

#include <string>
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

struct MetaData {
	using Skips = std::vector<std::pair<size_t, size_t>>;
	using Labels = std::map<ZyanU64, std::string>;
	Skips skips {};
	Labels labels {};
};

struct Cb {
	struct Ctx {
		const Process& p;
		const MetaData::Labels& labels;
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

enum class JumpFlag : uint8_t {
	JUMP = 1 << 0,
	CALL = 1 << 1,
	DATA = 1 << 2,
};

using JumpFlags = uint8_t;
using JumpFlagsMap = std::map<ZyanU64, JumpFlags>;

struct AnalyzeLabels : Cb {
	std::set<ZyanU64>& existingLabels;
	JumpFlagsMap& jumpLabels;
	AnalyzeLabels(std::set<ZyanU64>& el, JumpFlagsMap& jl);
	virtual void onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const override;
};

using Content = std::vector<ZyanU8>;

struct Process {
	ZydisDecoder decoder {};
	ZydisFormatter formatter {};
	Process();
	void loop(ZyanU64 ra, const Content& content, const MetaData& meta, const Cb& cb);
};

Content ReadFile(const char* filename);
size_t FromString(const std::string& str);
bool HandleOptions(int ac, char** av, po::variables_map& vm);

MetaData::Skips DetectStrings(const Content& content);
MetaData::Skips DetectRepeats(const Content& content);
MetaData::Skips MakeRanges(const po::variables_map& vm);
void CleanRanges(MetaData::Skips& ranges);

MetaData MetaDataReadFromFile(const std::string& path);
void MetaDataSaveToFile(const MetaData& meta, const std::string& path);

std::optional<ZyanI64> GetMemOperand(const ZydisDecodedOperand& op);
std::optional<ZyanU64> IsShortJump(const ZydisDisassembledInstruction& instruction, ZyanU64 runtime_address, bool anySize);
std::optional<ZyanU64> GetMemRef(const ZydisDisassembledInstruction& instruction, ZyanU64 runtime_address);

struct UiCtxParams {
	const std::string& binFilename;
	const std::string& workFilename;
	const Content& content;
	const MetaData& meta;
	ZyanU64 ra {};
};

#if ENABLE_TUI
void Tui(const UiCtxParams& params);
#endif

#if ENABLE_GUI
void Gui(const UiCtxParams& params);
#endif

inline bool printable(unsigned char c)
{
	if (c == '`')
		return false;
	if (c == 0xa0)
		return true;
	return (c >= 32 && c < 127);
}
