
#include "dosdisasm.hpp"

#include <cinttypes>

#include <boost/algorithm/string.hpp>

#include "rogueutil.hpp"

namespace ru = rogueutil;

const std::map<std::pair<uint8_t, uint8_t>, const char*> interruptInfo = {
	// 0x10
	{ { 0x10, 0x00 }, "Set video mode" },
	{ { 0x10, 0x01 }, "Set cursor type" },
	{ { 0x10, 0x02 }, "Set cursor position" },
	{ { 0x10, 0x03 }, "Read cursor position" },
	{ { 0x10, 0x04 }, "Read light pen" },
	{ { 0x10, 0x05 }, "Select active display page" },
	{ { 0x10, 0x06 }, "Scroll active page up" },
	{ { 0x10, 0x07 }, "Scroll active page down" },
	{ { 0x10, 0x08 }, "Read character and attribute at cursor" },
	{ { 0x10, 0x09 }, "Write character and attribute at cursor" },
	{ { 0x10, 0x0A }, "Write character at current cursor" },
	{ { 0x10, 0x0B }, "Set color palette" },
	{ { 0x10, 0x0C }, "Write graphics pixel at coordinate" },
	{ { 0x10, 0x0D }, "Read graphics pixel at coordinate" },
	{ { 0x10, 0x0E }, "Write text in teletype mode" },
	{ { 0x10, 0x0F }, "Get current video state" },
	{ { 0x10, 0x10 }, "Set/get palette registers (EGA/VGA)" },
	{ { 0x10, 0x11 }, "Character generator routine (EGA/VGA)" },
	{ { 0x10, 0x12 }, "Video subsystem configuration (EGA/VGA)" },
	{ { 0x10, 0x13 }, "Write string (BIOS after 1/10/86)" },
	{ { 0x10, 0x14 }, "Load LCD char font (convertible)" },
	{ { 0x10, 0x15 }, "Return physical display parms (convertible)" },
	{ { 0x10, 0x1A }, "Video Display Combination (VGA)" },
	{ { 0x10, 0x1B }, "Video BIOS Functionality/State Information (MCGA/VGA)" },
	{ { 0x10, 0x1C }, "Save/Restore Video State  (VGA only)" },
	{ { 0x10, 0xFE }, "Get DESQView/TopView Virtual Screen Regen Buffer" },
	{ { 0x10, 0xFF }, "Update DESQView/TopView Virtual Screen Regen Buffer" },
	// 0x13
	{ { 0x13, 0x00 }, "Reset disk system" },
	{ { 0x13, 0x01 }, "Get disk status" },
	{ { 0x13, 0x02 }, "Read disk sectors" },
	{ { 0x13, 0x03 }, "Write disk sectors" },
	{ { 0x13, 0x04 }, "Verify disk sectors" },
	{ { 0x13, 0x05 }, "Format disk track" },
	{ { 0x13, 0x06 }, "Format track and set bad sector flag (XT & portable)" },
	{ { 0x13, 0x07 }, "Format the drive starting at track (XT & portable)" },
	{ { 0x13, 0x08 }, "Get current drive parameters (XT & newer, see note Ų)" },
	{ { 0x13, 0x09 }, "Initialize 2 fixed disk base tables (XT & newer, see note Ų)" },
	{ { 0x13, 0x0A }, "Read long sector (XT & newer, see note Ų)" },
	{ { 0x13, 0x0B }, "Write long sector (XT & newer, see note Ų)" },
	{ { 0x13, 0x0C }, "Seek to cylinder (XT & newer, see note Ų)" },
	{ { 0x13, 0x0D }, "Alternate disk reset (XT & newer, see note Ų)" },
	{ { 0x13, 0x0E }, "Read sector buffer (XT & portable only)" },
	{ { 0x13, 0x0F }, "Write sector buffer (XT & portable only)" },
	{ { 0x13, 0x10 }, "Test for drive ready (XT & newer, see note Ų)" },
	{ { 0x13, 0x11 }, "Recalibrate drive (XT & newer, see note Ų)" },
	{ { 0x13, 0x12 }, "Controller ram diagnostic (XT & portable only)" },
	{ { 0x13, 0x13 }, "Drive diagnostic (XT & portable only)" },
	{ { 0x13, 0x14 }, "Controller internal diagnostic (XT & newer, see note Ų)" },
	{ { 0x13, 0x15 }, "Read disk type/DASD type (XT BIOS from 1/10/86 & newer)" },
	{ { 0x13, 0x16 }, "Disk change line status (XT BIOS from 1/10/86 & newer)" },
	{ { 0x13, 0x17 }, "Set dasd type for format (XT BIOS from 1/10/86 & newer)" },
	{ { 0x13, 0x18 }, "Set media type for format (BIOS date specific)" },
	{ { 0x13, 0x19 }, "Park fixed disk heads (AT & newer)" },
	{ { 0x13, 0x1A }, "Format ESDI drive unit (PS/2 50+)" },
	// 0x16
	{ { 0x16, 0x00 }, "Wait for keystroke and read" },
	{ { 0x16, 0x01 }, "Get keystroke status" },
	{ { 0x16, 0x02 }, "Get shift status" },
	{ { 0x16, 0x03 }, "Set keyboard typematic rate (AT+)" },
	{ { 0x16, 0x04 }, "Keyboard click adjustment (AT+)" },
	{ { 0x16, 0x05 }, "Keyboard buffer write  (AT,PS/2 enhanced keyboards)" },
	{ { 0x16, 0x10 }, "Wait for keystroke and read  (AT,PS/2 enhanced keyboards)" },
	{ { 0x16, 0x11 }, "Get keystroke status  (AT,PS/2 enhanced keyboards)" },
	{ { 0x16, 0x12 }, "Get shift status  (AT,PS/2 enhanced keyboards)" },
	// 0x21
	{ { 0x21, 0x00 }, "Program terminate" },
	{ { 0x21, 0x01 }, "Keyboard input with echo" },
	{ { 0x21, 0x02 }, "Display output" },
	{ { 0x21, 0x03 }, "Wait for auxiliary device input" },
	{ { 0x21, 0x04 }, "Auxiliary output" },
	{ { 0x21, 0x05 }, "Printer output" },
	{ { 0x21, 0x06 }, "Direct console I/O" },
	{ { 0x21, 0x07 }, "Wait for direct console input without echo" },
	{ { 0x21, 0x08 }, "Wait for console input without echo" },
	{ { 0x21, 0x09 }, "Print string" },
	{ { 0x21, 0x0A }, "Buffered keyboard input" },
	{ { 0x21, 0x0B }, "Check standard input status" },
	{ { 0x21, 0x0C }, "Clear keyboard buffer, invoke keyboard function" },
	{ { 0x21, 0x0D }, "Disk reset" },
	{ { 0x21, 0x0E }, "Select disk" },
	{ { 0x21, 0x0F }, "Open file using FCB" },
	{ { 0x21, 0x10 }, "Close file using FCB" },
	{ { 0x21, 0x11 }, "Search for first entry using FCB" },
	{ { 0x21, 0x12 }, "Search for next entry using FCB" },
	{ { 0x21, 0x13 }, "Delete file using FCB" },
	{ { 0x21, 0x14 }, "Sequential read using FCB" },
	{ { 0x21, 0x15 }, "Sequential write using FCB" },
	{ { 0x21, 0x16 }, "Create a file using FCB" },
	{ { 0x21, 0x17 }, "Rename file using FCB" },
	{ { 0x21, 0x18 }, "DOS dummy function (CP/M) (not used/listed)" },
	{ { 0x21, 0x19 }, "Get current default drive" },
	{ { 0x21, 0x1A }, "Set disk transfer address" },
	{ { 0x21, 0x1B }, "Get allocation table information" },
	{ { 0x21, 0x1C }, "Get allocation table info for specific device" },
	{ { 0x21, 0x1D }, "DOS dummy function (CP/M) (not used/listed)" },
	{ { 0x21, 0x1E }, "DOS dummy function (CP/M) (not used/listed)" },
	{ { 0x21, 0x1F }, "Get pointer to default drive parameter table (undocumented)" },
	{ { 0x21, 0x20 }, "DOS dummy function (CP/M) (not used/listed)" },
	{ { 0x21, 0x21 }, "Random read using FCB" },
	{ { 0x21, 0x22 }, "Random write using FCB" },
	{ { 0x21, 0x23 }, "Get file size using FCB" },
	{ { 0x21, 0x24 }, "Set relative record field for FCB" },
	{ { 0x21, 0x25 }, "Set interrupt vector" },
	{ { 0x21, 0x26 }, "Create new program segment" },
	{ { 0x21, 0x27 }, "Random block read using FCB" },
	{ { 0x21, 0x28 }, "Random block write using FCB" },
	{ { 0x21, 0x29 }, "Parse filename for FCB" },
	{ { 0x21, 0x2A }, "Get date" },
	{ { 0x21, 0x2B }, "Set date" },
	{ { 0x21, 0x2C }, "Get time" },
	{ { 0x21, 0x2D }, "Set time" },
	{ { 0x21, 0x2E }, "Set/reset verify switch" },
	{ { 0x21, 0x2F }, "Get disk transfer address" },
	{ { 0x21, 0x30 }, "Get DOS version number" },
	{ { 0x21, 0x31 }, "Terminate process and remain resident" },
	{ { 0x21, 0x32 }, "Get pointer to drive parameter table (undocumented)" },
	{ { 0x21, 0x33 }, "Get/set Ctrl-Break check state & get boot drive" },
	{ { 0x21, 0x34 }, "Get address to DOS critical flag (undocumented)" },
	{ { 0x21, 0x35 }, "Get vector" },
	{ { 0x21, 0x36 }, "Get disk free space" },
	{ { 0x21, 0x37 }, "Get/set switch character (undocumented)" },
	{ { 0x21, 0x38 }, "Get/set country dependent information" },
	{ { 0x21, 0x39 }, "Create subdirectory (mkdir)" },
	{ { 0x21, 0x3A }, "Remove subdirectory (rmdir)" },
	{ { 0x21, 0x3B }, "Change current subdirectory (chdir)" },
	{ { 0x21, 0x3C }, "Create file using handle" },
	{ { 0x21, 0x3D }, "Open file using handle" },
	{ { 0x21, 0x3E }, "Close file using handle" },
	{ { 0x21, 0x3F }, "Read file or device using handle" },
	{ { 0x21, 0x40 }, "Write file or device using handle" },
	{ { 0x21, 0x41 }, "Delete file" },
	{ { 0x21, 0x42 }, "Move file pointer using handle" },
	{ { 0x21, 0x43 }, "Change file mode" },
	{ { 0x21, 0x44 }, "I/O control for devices (IOCTL)" },
	{ { 0x21, 0x45 }, "Duplicate file handle" },
	{ { 0x21, 0x46 }, "Force duplicate file handle" },
	{ { 0x21, 0x47 }, "Get current directory" },
	{ { 0x21, 0x48 }, "Allocate memory blocks" },
	{ { 0x21, 0x49 }, "Free allocated memory blocks" },
	{ { 0x21, 0x4A }, "Modify allocated memory blocks" },
	{ { 0x21, 0x4B }, "EXEC load and execute program (func 1 undocumented)" },
	{ { 0x21, 0x4C }, "Terminate process with return code" },
	{ { 0x21, 0x4D }, "Get return code of a sub-process" },
	{ { 0x21, 0x4E }, "Find first matching file" },
	{ { 0x21, 0x4F }, "Find next matching file" },
	{ { 0x21, 0x50 }, "Set current process id (undocumented)" },
	{ { 0x21, 0x51 }, "Get current process id (undocumented)" },
	{ { 0x21, 0x52 }, "Get pointer to DOS INVARS (undocumented)" },
	{ { 0x21, 0x53 }, "Generate drive parameter table (undocumented)" },
	{ { 0x21, 0x54 }, "Get verify setting" },
	{ { 0x21, 0x55 }, "Create PSP (undocumented)" },
	{ { 0x21, 0x56 }, "Rename file" },
	{ { 0x21, 0x57 }, "Get/set file date and time using handle" },
	{ { 0x21, 0x58 }, "Get/set memory allocation strategy (3.x+, undocumented)" },
	{ { 0x21, 0x59 }, "Get extended error information (3.x+)" },
	{ { 0x21, 0x5A }, "Create temporary file (3.x+)" },
	{ { 0x21, 0x5B }, "Create new file (3.x+)" },
	{ { 0x21, 0x5C }, "Lock/unlock file access (3.x+)" },
	{ { 0x21, 0x5D }, "Critical error information (undocumented 3.x+)" },
	{ { 0x21, 0x5E }, "Network services (3.1+)" },
	{ { 0x21, 0x5F }, "Network redirection (3.1+)" },
	{ { 0x21, 0x60 }, "Get fully qualified file name (undocumented 3.x+)" },
	{ { 0x21, 0x62 }, "Get address of program segment prefix (3.x+)" },
	{ { 0x21, 0x63 }, "Get system lead byte table (MSDOS 2.25 only)" },
	{ { 0x21, 0x64 }, "Set device driver look ahead  (undocumented 3.3+)" },
	{ { 0x21, 0x65 }, "Get extended country information (3.3+)" },
	{ { 0x21, 0x66 }, "Get/set global code page (3.3+)" },
	{ { 0x21, 0x67 }, "Set handle count (3.3+)" },
	{ { 0x21, 0x68 }, "Flush buffer (3.3+)" },
	{ { 0x21, 0x69 }, "Get/set disk serial number (undocumented DOS 4.0+)" },
	{ { 0x21, 0x6A }, "DOS reserved (DOS 4.0+)" },
	{ { 0x21, 0x6B }, "DOS reserved" },
	{ { 0x21, 0x6C }, "Extended open/create (4.x+)" },
	{ { 0x21, 0xF8 }, "Set OEM INT 21 handler (functions F9-FF) (undocumented)" },
};

std::optional<ZyanU64> isShortJump(const ZydisDisassembledInstruction& instruction, ZyanU64 runtime_address, bool anySize)
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

AnalyzeLabels::AnalyzeLabels(std::set<ZyanU64>& el, std::set<ZyanU64>& jl)
	: existingLabels(el)
	, jumpLabels(jl)
{
}

void AnalyzeLabels::onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const
{
	existingLabels.insert(ctx.runtime_address);
	if (const auto ona = isShortJump(instruction, ctx.runtime_address, true)) {
		jumpLabels.insert(*ona);
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
			dump(ctx, CType::Dup, size, label, std::nullopt, nullptr, "times %d db 0x%02X", size, *b);
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
					dump(mkCtx(lastNl), CType::Dup, i - lastNl, nullptr, std::nullopt, nullptr, "%s", buffer);
				}
				memcpy(buffer, ctx.data + i, slen);
				buffer[slen] = 0;
				char label[64];
				snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address + i);
				if (slen < int(size) && ctx.data[slen] == 0) {
					dump(mkCtx(i), CType::Dup, slen, label, std::nullopt, nullptr, "db '%s', 0", buffer);
					lastNl = i + slen + 1;
					i += slen;
				} else {
					dump(mkCtx(i), CType::Dup, slen, label, std::nullopt, nullptr, "db '%s'", buffer);
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
			dump(mkCtx(lastNl), CType::Dup, 1 + i - lastNl, lastNl == 0 ? label : nullptr, std::nullopt, nullptr, "%s", buffer);
			lastNl = i + 1;
			bi = 0;
		}
	}
	if (bi != 0) {
		char label[64];
		snprintf(label, sizeof(label), "l0x%04" PRIX64, ctx.runtime_address);
		dump(mkCtx(lastNl), CType::Dup, size - lastNl, lastNl == 0 ? label : nullptr, std::nullopt, nullptr, "%s", buffer);
	}
}

void Dumper::onUnkByte(const Ctx& ctx, ZyanU8 skip) const
{
	dump(ctx, CType::Db, 1, nullptr, std::nullopt, nullptr, "db 0x%02X", skip);
}

std::string Dumper::getComment(const Ctx&, const ZydisDisassembledInstruction& instruction) const
{
	if (instruction.info.mnemonic == ZYDIS_MNEMONIC_INT) {
		if (instruction.info.operand_count_visible == 1) {
			const std::pair<uint8_t, uint8_t> inti { uint8_t(instruction.operands[0].imm.value.u), trk.regs[0].H };
			auto itf = interruptInfo.find(inti);
			if (itf == interruptInfo.end())
				return Fmt<32>("%02X.%02X", inti.first, inti.second);
			else
				return Fmt<32>("%02X.%02X -> ", inti.first, inti.second) + itf->second;
		}
	} else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_MOV) {
		if (instruction.info.operand_count_visible == 2) {
			if (instruction.operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && instruction.operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
				for (int i = 0; i < 4; ++i) {
					if (instruction.operands[0].reg.value == ZYDIS_REGISTER_AX + i) {
						trk.regs[i].X = uint16_t(instruction.operands[1].imm.value.u);
					} else if (instruction.operands[0].reg.value == ZYDIS_REGISTER_AH + i) {
						trk.regs[i].H = uint8_t(instruction.operands[1].imm.value.u);
					} else if (instruction.operands[0].reg.value == ZYDIS_REGISTER_AL + i) {
						trk.regs[i].L = uint8_t(instruction.operands[1].imm.value.u);
					}
				}
			}
		}
	}
	return {};
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
	const auto comment = getComment(ctx, instruction);
	const char* cmtPtr = comment.empty() ? nullptr : comment.c_str();
	const bool isCall = s[0] == "call";
	if (const auto ona = isShortJump(instruction, ctx.runtime_address, isCall)) {
		if (s.size() == 2) {
			// const char* prefix = isCall ? " near" : " short";
			const char* prefix = "";
			if (existingLabels.find(*ona) != existingLabels.end()) {
				dump(ctx, ct, instruction.info.length, label, ona, cmtPtr, "%s%s l%s", s[0].c_str(), prefix, s[1].c_str());
			} else {
				dump(ctx, ct, instruction.info.length, label, ona, cmtPtr, "%s%s %s", s[0].c_str(), prefix, s[1].c_str());
			}
		} else {
			const std::string j = boost::algorithm::join(s, " ");
			dump(ctx, ct, instruction.info.length, label, ona, cmtPtr, "%s", j.c_str());
		}
	} else {
		const std::string j = boost::algorithm::join(s, " ");
		dump(ctx, ct, instruction.info.length, label, ona, cmtPtr, "%s", j.c_str());
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
	virtual void dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, std::optional<ZyanU64>, const char* const str, const char* const comment) const override
	{
		ru::tprint(ru::Color::GREEN);
		if (label != nullptr)
			fprintf(outFile, "%s:\n", label);
		ru::tprint(ct != CType::Code ? ru::Color::BLUE : ru::Color::WHITE);
		fprintf(outFile, "  %s", str);
		if (comment) {
			ru::tprint(ru::Color::CYAN);
			fprintf(outFile, " ; %s", comment);
		} else {
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
	virtual void dumpStr(const Ctx& ctx, CType, ZyanUSize sz, const char* label, std::optional<ZyanU64>, const char* const str, const char* const comment) const override
	{
		if (label != nullptr)
			fprintf(outFile, "%s:\n", label);
		fprintf(outFile, "  %s", str);
		if (comment) {
			fprintf(outFile, " ; %s", comment);
		} else {
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
		Tui(content, skip_ranges);
	} else if (vm.count("gui") != 0 && vm["gui"].as<bool>()) {
		Gui(content, skip_ranges);
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
