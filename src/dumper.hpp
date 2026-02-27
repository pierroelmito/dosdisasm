
#pragma once

#include "dosdisasm.hpp"

#include <array>

enum class CType {
	Code,
	Str,
	Dup,
	Db,
	Ret,
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
	struct DumpParams {
		CType ct {};
		ZyanUSize sz {};
		const char* label {};
		std::optional<ZyanU64> jump {};
		const char* comment {};
	};
	const std::set<ZyanU64>& existingLabels;
	const JumpFlagsMap& jumpLabels;
	std::map<ZyanU64, const std::string> userLabels;
	mutable Tracker trk {};
	Dumper(const std::set<ZyanU64>& el, const JumpFlagsMap& jl);
	virtual void dumpStr(const Ctx& ctx, DumpParams p, const char* const str) const = 0;
	template <typename... T>
	inline constexpr void dump(const Ctx& ctx, DumpParams p, const char* const fmt, const T&... args) const
	{
		char buffer[2048] {};
		snprintf(buffer, sizeof(buffer), fmt, args...);
		dumpStr(ctx, p, buffer);
	}
	virtual void onSkip(const Ctx& ctx, ZyanUSize size) const override;
	virtual void onUnkByte(const Ctx& ctx, ZyanU8 skip) const override;
	virtual void onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const override;
	void makeReplaces(const Ctx& ctx, const ZydisDisassembledInstruction& instruction, std::vector<std::string>& tokens) const;
	std::string getComment(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const;
	std::string getLabel(const MetaData& md, const ZyanU64 address) const;
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
	uint8_t jumpFlags {};
	CType ct {};
	Cmp cmp { Cmp::None };
};

using Listing = std::vector<Item>;
