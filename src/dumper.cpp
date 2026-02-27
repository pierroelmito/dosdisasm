
#include "dumper.hpp"

#include <cinttypes>

#include <boost/algorithm/string.hpp>

#include "dosdisasm.hpp"

extern const std::map<uint16_t, std::pair<const char*, const char*>> portInfo;
extern const std::map<uint8_t, const char*> interruptInfo0;
extern const std::map<std::pair<uint8_t, uint8_t>, const char*> interruptInfo1;

Dumper::Dumper(const std::set<ZyanU64>& el, const JumpFlagsMap& jl)
	: existingLabels(el)
	, jumpLabels(jl)
{
}

void makeLabel(const MetaData::Labels& labels, char buffer[64], ZyanU64 ra, uint8_t flags)
{
	if (const auto itf = labels.find(ra); itf != labels.end()) {
		strncpy(buffer, itf->second.c_str(), 64);
	} else if (flags == uint8_t(JumpFlag::CALL)) {
		snprintf(buffer, 64, "C_%04" PRIX64, ra);
	} else if (flags == uint8_t(JumpFlag::JUMP)) {
		snprintf(buffer, 64, "J_%04" PRIX64, ra);
	} else {
		snprintf(buffer, 64, "D_%04" PRIX64, ra);
	}
}

void Dumper::onSkip(const Ctx& ctx, ZyanUSize size) const
{
	// - use dw if size == 2
	// - is only printable, use string
	if (size > 4) {
		auto* b = ctx.data;
		auto* e = ctx.data + size;
		auto itf = std::find_if(b, e, [&](auto c) {
			return c != *b;
		});
		if (itf == e) {
			char label[64];
			makeLabel(ctx.md.labels, label, ctx.runtime_address, uint8_t(JumpFlag::DATA));
			dump(ctx, { CType::Dup, size, label, std::nullopt, nullptr }, "times %d db 0x%02X", size, *b);
			return;
		}
	}
	if (size == 2) {
		char label[64];
		makeLabel(ctx.md.labels, label, ctx.runtime_address, uint8_t(JumpFlag::DATA));
		const uint16_t s = (ctx.data[1] << 8) | ctx.data[0];
		const std::string comment = getLabel(ctx.md, s);
		if (comment.empty()) {
			dump(ctx, { CType::Dup, size, label, std::nullopt, nullptr }, "dw 0x%04X", s);
		} else {
			dump(ctx, { CType::Dup, size, label, std::nullopt, nullptr }, "dw %s", comment.c_str());
		}
		return;
	}
	char buffer[2048];
	char bi {};
	auto mkCtx = [&](auto i) -> Ctx {
		return {
			ctx.p,
			ctx.md,
			ctx.runtime_address + i,
			ctx.data + i,
			ctx.offset + i
		};
	};
	ZyanUSize lastNl = 0;
	for (size_t i = 0u; i < size; ++i) {
		const ZyanU8 byte = ctx.data[i];
		if (printable(byte)) {
			const auto itf = std::find_if(ctx.data + i, ctx.data + size, [&](char c) {
				return !printable(c);
			});
			const size_t slen = std::distance(ctx.data + i, itf);
			if (slen > 5 || (slen > 3 && slen == size) || (slen > 3 && slen == size - 1 && ctx.data[slen] == 0)) {
				if (bi != 0) {
					dump(mkCtx(lastNl), { CType::Str, i - lastNl, nullptr, std::nullopt, nullptr }, "%s", buffer);
				}
				for (size_t si = 0, di = 0; si < slen; ++si, ++di) {
					buffer[di] = ctx.data[i + si];
					if (buffer[di] == '\\')
						buffer[++di] = '\\';
					buffer[di + 1] = 0;
				}
				char label[64];
				makeLabel(ctx.md.labels, label, ctx.runtime_address + i, uint8_t(JumpFlag::DATA));
				if (slen < size && ctx.data[slen] == 0) {
					dump(mkCtx(i), { CType::Str, slen, label, std::nullopt, nullptr }, "db `%s`, 0", buffer);
					lastNl = i + slen + 1;
					i += slen;
				} else {
					dump(mkCtx(i), { CType::Str, slen, label, std::nullopt, nullptr }, "db `%s`", buffer);
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
			makeLabel(ctx.md.labels, label, ctx.runtime_address, uint8_t(JumpFlag::DATA));
			dump(mkCtx(lastNl), { CType::Dup, 1 + i - lastNl, lastNl == 0 ? label : nullptr, std::nullopt, nullptr }, "%s", buffer);
			lastNl = i + 1;
			bi = 0;
		}
	}
	if (bi != 0) {
		char label[64];
		makeLabel(ctx.md.labels, label, ctx.runtime_address, uint8_t(JumpFlag::DATA));
		dump(mkCtx(lastNl), { CType::Dup, size - lastNl, lastNl == 0 ? label : nullptr, std::nullopt, nullptr }, "%s", buffer);
	}
}

void Dumper::onUnkByte(const Ctx& ctx, ZyanU8 skip) const
{
	dump(ctx, { CType::Db, 1, nullptr, std::nullopt, nullptr }, "db 0x%02X", skip);
}

std::optional<ZyanI64> getImmOperand(const ZydisDecodedOperand& op)
{
	if (op.type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
		return std::nullopt;
	auto v = op.imm.value.s;
	return v;
}

std::string Dumper::getLabel(const MetaData& md, const ZyanU64 address) const
{
	const auto itf = existingLabels.find(address);
	if (itf == existingLabels.end())
		return {};
	if (const auto itl = md.labels.find(address); itl != md.labels.end())
		return itl->second;
	return Fmt<32>("D_%04X", address);
}

void Dumper::makeReplaces(const Ctx& ctx, const ZydisDisassembledInstruction& instruction, std::vector<std::string>& tokens) const
{
#if 1
	for (int i = 0; i < instruction.info.operand_count_visible; ++i) {
		if (auto mem = GetMemOperand(instruction.operands[i]); mem) {
			const auto l = getLabel(ctx.md, *mem);
			if (!l.empty()) {
				const auto op = ctx.p.getOperandStr(instruction.runtime_address, &instruction.info, &instruction.operands[i]);
				for (auto& token : tokens) {
					if (token.compare(0, op.size(), op) == 0) {
						token = "[" + l + "]" + token.substr(op.size());
					}
				}
			}
		}
	}
#endif
}

std::string Dumper::getComment(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const
{
	if (instruction.info.mnemonic == ZYDIS_MNEMONIC_INT) {
		if (instruction.info.operand_count_visible == 1) {
			const uint8_t int0 = uint8_t(instruction.operands[0].imm.value.u);
			const std::pair<uint8_t, uint8_t> int1 { int0, trk.regs[0].H };
			if (auto itf0 = interruptInfo0.find(int0); itf0 != interruptInfo0.end())
				return Fmt<32>("%02X -> ", int0) + itf0->second;
			if (auto itf1 = interruptInfo1.find(int1); itf1 != interruptInfo1.end())
				return Fmt<32>("%02X.%02X -> ", int1.first, int1.second) + itf1->second;
			else
				return Fmt<32>("%02X", int0);
		}
	} else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_IN) {
		if (instruction.info.operand_count_visible == 2) {
			if (instruction.operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
				const auto p = uint16_t(instruction.operands[1].imm.value.u);
				auto itf = portInfo.find(p);
				if (itf == portInfo.end())
					return Fmt<32>("%03X", p);
				else
					return Fmt<32>("%03X -> ", p) + itf->second.first;
			} else {
				return "???";
			}
		}
	} else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_OUT) {
		if (instruction.info.operand_count_visible == 2) {
			if (instruction.operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
				const auto p = uint16_t(instruction.operands[0].imm.value.u);
				auto itf = portInfo.find(p);
				if (itf == portInfo.end())
					return Fmt<32>("%03X", p);
				else
					return Fmt<32>("%03X -> ", p) + itf->second.first;
			} else {
				return "???";
			}
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
	for (int i = 0; i < instruction.info.operand_count_visible; ++i) {
		if (auto imm = getImmOperand(instruction.operands[i]); imm) {
			const auto l = getLabel(ctx.md, *imm);
			if (!l.empty())
				return l;
		}
	}
	return {};
}

void Dumper::onIns(const Ctx& ctx, const ZydisDisassembledInstruction& instruction) const
{
	auto ct = CType::Code;
	const char* label = nullptr;
	char buffer[64];
	if (auto itf = jumpLabels.find(ctx.runtime_address); itf != jumpLabels.end()) {
		makeLabel(ctx.md.labels, buffer, ctx.runtime_address, itf->second);
		label = buffer;
	}
	std::vector<std::string> s;
	boost::algorithm::split(
		s,
		instruction.text,
		boost::is_any_of(" "));
	s.erase(std::remove(s.begin(), s.end(), "dword"), s.end());
	if (s[0] == "ret") {
		ct = CType::Ret;
		if (s.size() >= 2 && s[1] == "far") {
			s.erase(s.begin());
			s[0] = "retf";
		}
	}
	for (auto& w : s) {
		if (w == "ptr")
			w = "near";
	}

	makeReplaces(ctx, instruction, s);

	// s.erase(std::remove(s.begin(), s.end(), "ptr"), s.end());
	// const bool isJmp = s[0] == "jmp";
	const auto comment = getComment(ctx, instruction);
	const char* cmtPtr = comment.empty() ? nullptr : comment.c_str();
	// const bool isCall = true; // s[0] == "call";
	const bool isCall = s[0] == "call";
	if (const auto ona = IsShortJump(instruction, ctx.runtime_address, isCall)) {
		if (s.size() == 2) {
			const char* prefix = isCall ? " near" : " short";
			// const char* prefix = " near";
			// const char* prefix = "";
			if (existingLabels.find(*ona) != existingLabels.end()) {
				char jlabel[64];
				const auto itf = jumpLabels.find(*ona);
				makeLabel(ctx.md.labels, jlabel, *ona, itf->second);
				dump(ctx, { ct, instruction.info.length, label, ona, cmtPtr }, "%s%s %s", s[0].c_str(), prefix, jlabel);
			} else {
				dump(ctx, { ct, instruction.info.length, label, ona, cmtPtr }, "%s%s %s", s[0].c_str(), prefix, s[1].c_str());
			}
		} else {
			const std::string j = boost::algorithm::join(s, " ");
			dump(ctx, { ct, instruction.info.length, label, ona, cmtPtr }, "%s", j.c_str());
		}
	} else {
		const std::string j = boost::algorithm::join(s, " ");
		dump(ctx, { ct, instruction.info.length, label, GetMemRef(instruction, ctx.runtime_address), cmtPtr }, "%s", j.c_str());
	}
}

/*

PORTS Common I/O Port Addresses


	Port addresses are not always constant across PC, AT and PS/2
	Unless marked, port addresses are relative to PC and XT only


	010-01F  8237 DMA Controller (PS/2 model 60 & 80), reserved (AT)

	020-02F  8259A Master Programmable Interrupt Controller
	020 8259 Command port  (see 8259)
	021 8259 Interrupt mask register  (see 8259)

	030-03F  8259A Slave Programmable Interrupt Controller (AT,PS/2)

	040-05F  8253 or 8254 Programmable Interval Timer (PIT, see ~8253~)
	040 8253 channel 0, counter divisor
	041 8253 channel 1, RAM refresh counter
	042 8253 channel 2, Cassette and speaker functions
	043 8253 mode control  (see 8253)
	044 8254 PS/2 extended timer
	047 8254 Channel 3 control byte

	060-067  8255 Programmable Peripheral Interface  (PC,XT, PCjr)
	060 8255 Port A keyboard input/output buffer (output PCjr)
	061 8255 Port B output
	062 8255 Port C input
	063 8255 Command/Mode control register

	070 CMOS RAM/RTC, also NMI enable/disable (AT,PS/2, see RTC)
	071 CMOS RAM data  (AT,PS/2)

	080 Manufacturer systems checkpoint port (used during POST)
	080-090  DMA Page Registers
	081 High order 4 bits of DMA channel 2 address
	082 High order 4 bits of DMA channel 3 address
	083 High order 4 bits of DMA channel 1 address

	090-097  POS/Programmable Option Select  (PS/2)
	090 Central arbitration control Port
	091 Card selection feedback
	092 System control and status register
	094 System board enable/setup register
	095 Reserved
	096 Adapter enable/setup register
	097 Reserved

	0A0 NMI Mask Register (PC,XT) (write 80h to enable NMI, 00h disable)
	0A0-0BF  Second 8259 Programmable Interrupt Controller (AT, PS/2)
	0A0 Second 8259 Command port  (see 8259)
	0A1 Second 8259 Interrupt mask register  (see 8259)

	0C0 TI SN76496 Programmable Tone/Noise Generator (PCjr)
	0C0-0DF  8237 DMA controller 2 (AT)
	0C2 DMA channel 3 selector  (see ports 6 & 82)

	0E0-0EF  Reserved

	0F0-0FF  Math coprocessor (AT, PS/2)
	0F0-0F5  PCjr Disk Controller
	0F0 Disk Controller
	0F2 Disk Controller control port
	0F4 Disk Controller status register
	0F5 Disk Controller data port

	0F8-0FF  Reserved for future microprocessor extensions

	100-10F  POS Programmable Option Select (PS/2)
	100 POS Register 0, Adapter ID byte (LSB)
	101 POS Register 1, Adapter ID byte (MSB)
	102 POS Register 2, Option select data byte 1
		Bit 0 is card enable (CDEN)
	103 POS Register 3, Option select data byte 2
	104 POS Register 4, Option select data byte 3
	105 POS Register 5, Option select data byte 4
		Bit 7 is (-CHCK)
		Bit 6 is reserved
	106 POS Register 6, subaddress extension (LSB)
	107 POS Register 7, subaddress extension (MSB)

	110-1EF  System I/O channel

	170-17F  Fixed disk 1 (AT)
	170 disk 1 data
	171 disk 1 error
	172 disk 1 sector count
	173 disk 1 sector number
	174 disk 1 cylinder low
	175 disk 1 cylinder high
	176 disk 1 drive/head
	177 disk 1 status

	1F0-1FF  Fixed disk 0 (AT)
	1F0 disk 0 data
	1F1 disk 0 error
	1F2 disk 0 sector count
	1F3 disk 0 sector number
	1F4 disk 0 cylinder low
	1F5 disk 0 cylinder high
	1F6 disk 0 drive/head
	1F7 disk 0 status

	200-20F  Game Adapter (see GAME PORT or ~JOYSTICK~)

	210-217  Expansion Card Ports (XT)
	210 Write: latch expansion bus data
		read:  verify expansion bus data
	211 Write: clear wait,test latch
		Read:  MSB of data address
	212 Read:  LSB of data address
	213 Write: 0=enable, 1=/disable expansion unit
	214-215  Receiver Card Ports
	214 write: latch data, read: data
	215 read:  MSB of address, next read: LSB of address

	21F Reserved

	220-26F  Reserved for I/O channel

	270-27F  Third parallel port (see ~PARALLEL PORT~)
	278 data port
	279 status port
	27A control port

	280-2AF  Reserved for I/O channel

	2A2-2A3  MSM58321RS clock

	2B0-2DF  Alternate EGA, or 3270 PC video (XT, AT)

	2E0 Alternate EGA/VGA
	2E1 GPIB Adapter  (AT)

	2E2-2E3  Data acquisition adapter (AT)

	2E8-2EF  COM4 non PS/2 UART (Reserved by IBM) (see ~UART~)

	2F0-2F7  Reserved

	2F8-2FF  COM2 Second Asynchronous Adapter (see UART)
		 Primary Asynchronous Adapter for PCjr

	300-31F  Prototype Experimentation Card (except PCjr)
		 Periscope hardware debugger
	320-32F  Hard Disk Controller  (XT)
	320 Read from/Write to controller
	321 Read: Controller Status, Write: controller reset
	322 Write: generate controller select pulse
	323 Write: Pattern to DMA and interrupt mask register
		(see ports 0F,21,C2)
	324 disk attention/status

	330-33F  Reserved for XT/370

	340-35F  Reserved for I/O channel

	360-36F  PC Network

	370-377  Floppy disk controller (except PCjr)
	372 Diskette digital output
	374 Diskette controller status
	375 Diskette controller data
	376 Diskette controller data
	377 Diskette digital input

	378-37F  Second Parallel Printer (see ~PARALLEL PORT~)
		 First Parallel Printer (see PARALLEL PORT)
	378 data port
	379 status port
	37A control port

	380-38F  Secondary Binary Synchronous Data Link Control (SDLC) adapter
	380 On board 8255 port A, internal/external sense
	381 On board 8255 port B, external modem interface
	382 On board 8255 port C, internal control and gating
	383 On board 8255 mode register
	384 On board 8253 channel square wave generator
	385 On board 8253 channel 1 inactivity time-out
	386 On board 8253 channel 2 inactivity time-out
	387 On board 8253 mode register
	388 On board 8273 read: status; Write: Command
	389 On board 8273 write: parameter; read: response
	38A On board 8273 transmit interrupt status
	38B On board 8273 receiver interrupt status
	38C On board 8273 data

	390-39F  Cluster Adapter

	3A0-3AF  Primary Binary Synchronous Data Link Control (SDLC) adapter
	3A0 On board 8255 port A, internal/external sense
	3A1 On board 8255 port B, external modem interface
	3A2 On board 8255 port C, internal control and gating
	3A3 On board 8255 mode register
	3A4 On board 8253 counter 0 unused
	3A5 On board 8253 counter 1 inactivity time-outs
	3A6 On board 8253 counter 2 inactivity time-outs
	3A7 On board 8253 mode register
	3A8 On board 8251 data
	3A9 On board 8251 command/mode/status register

	3B0-3BF Monochrome Display Adapter (write only, see ~6845~)
	3B0 port address decodes to 3B4
	3B1 port address decodes to 3B5
	3B2 port address decodes to 3B4
	3B3 port address decodes to 3B5
	3B4 6845 index register, selects which register [0-11h]
		is to be accessed through port 3B5
	3B5 6845 data register [0-11h] selected by port 3B4,
		registers 0C-0F may be read.  If a read occurs without
		the adapter installed, FFh is returned.  (see 6845)
	3B6 port address decodes to 3B4
	3B7 port address decodes to 3B5
	3B8 6845 Mode control register
	3B9 reserved for color select register on color adapter
	3BA status register (read only)
	3BB reserved for light pen strobe reset

	3BC-3BF  Primary Parallel Printer Adapter (see ~PARALLEL PORT~)
	3BC parallel 1, data port
	3BD parallel 1, status port
	3BE parallel 1, control port

	3C0-3CF  EGA/VGA
	3C0 VGA attribute and sequencer register
	3C1 Other video attributes
	3C2 EGA, VGA, CGA input status 0
	3C3 Video subsystem enable
	3C4 CGA, EGA, VGA sequencer index
	3C5 CGA, EGA, VGA sequencer
	3C6 VGA video DAC PEL mask
	3C7 VGA video DAC state
	3C8 VGA video DAC PEL address
	3C9 VGA video DAC
	3CA VGA graphics 2 position
	3CC VGA graphics 1 position
	3CD VGA feature control
	3CE VGA graphics index
	3CF Other VGA graphics

	3D0-3DF Color Graphics Monitor Adapter (ports 3D0-3DB are
		write only, see 6845)
	3D0 port address decodes to 3D4
	3D1 port address decodes to 3D5
	3D2 port address decodes to 3D4
	3D3 port address decodes to 3D5
	3D4 6845 index register, selects which register [0-11h]
		is to be accessed through port 3D5
	3D5 6845 data register [0-11h] selected by port 3D4,
		registers 0C-0F may be read.  If a read occurs without
		the adapter installed, FFh is returned.  (see 6845)
	3D6 port address decodes to 3D4
	3D7 port address decodes to 3D5
	3D8 6845 Mode control register (CGA, EGA, VGA, except PCjr)
	3D9 color select palette register (CGA, EGA, VGA, see 6845)
	3DA status register (read only, see 6845, PCjr VGA access)
	3DB Clear light pen latch (any write)
	3DC Preset Light pen latch
	3DF CRT/CPU page register (PCjr only)

	3E8-3EF  COM3 non PS/2 UART (Reserved by IBM) (see ~UART~)

	3F0-3F7  Floppy disk controller (except PCjr)
	3F0 Diskette controller status A
	3F1 Diskette controller status B
	3F2 controller control port
	3F4 controller status register
	3F5 data register (write 1-9 byte command, see INT 13)
	3F6 Diskette controller data
	3F7 Diskette digital input

	3F8-3FF  COM1 Primary Asynchronous Adapter  (see ~UART~)

	3220-3227  PS/2 COM3 (see UART)
	3228-322F  PS/2 COM4 (see UART)
	4220-4227  PS/2 COM5 (see UART)
	4228-422F  PS/2 COM6 (see UART)
	5220-5227  PS/2 COM7 (see UART)
	5228-522F  PS/2 COM8 (see UART)

	- many cards designed for the ISA BUS only uses the lower 10 bits
	  of the port address but some ISA adapters use addresses beyond
	  3FF.  Any address that matches in the lower 10 bits will decode
	  to the same card.   It is up to the adapters to resolve or ignore
	  the high bits of the port addresses.   An example would be the
	  Cluster adapter that has a port address of 390h.  The second
	  cluster adapter has a port address of 790h which resolves to
	  the same port address with the cards determining which one
	  actually gets the data.

*/

const std::map<uint16_t, std::pair<const char*, const char*>> portInfo = {
	// 000-00F  8237 DMA controller
	{ 0x000, { "Channel 0 address register", "" } },
	{ 0x001, { "Channel 0 word count", "" } },
	{ 0x002, { "Channel 1 address register", "" } },
	{ 0x003, { "Channel 1 word count", "" } },
	{ 0x004, { "Channel 2 address register", "" } },
	{ 0x005, { "Channel 2 word count", "" } },
	{ 0x006, { "Channel 3 address register", "" } },
	{ 0x007, { "Channel 3 word count", "" } },
	{ 0x008, { "Status/command register", "" } },
	{ 0x009, { "Request register", "" } },
	{ 0x00A, { "Mask register", "" } },
	{ 0x00B, { "Mode register", "" } },
	{ 0x00C, { "Clear MSB/LSB flip flop", "" } },
	{ 0x00D, { "Master clear temp register", "" } },
	{ 0x00E, { "Clear mask register", "" } },
	{ 0x00F, { "Multiple mask register", "" } },
	// 3F0-3F7  Floppy disk controller (except PCjr)
	{ 0x3F0, { "Diskette controller status A", "" } },
	{ 0x3F1, { "Disiskette controller status B", "" } },
	{ 0x3F2, { "conistroller control port", "" } },
	{ 0x3F4, { "conistroller status register", "" } },
	{ 0x3F5, { "datisa register (write 1-9 byte command, see INT 13)", "" } },
	{ 0x3F6, { "Disiskette controller data", "" } },
	{ 0x3F7, { "Disiskette digital input", "" } },
	// 040-05F  8253 or 8254 Programmable Interval Timer (PIT, see ~8253~)
	{ 0x040, { "8253 channel 0, counter divisor", "" } },
	{ 0x041, { "8253 channel 1, RAM refresh counter", "" } },
	{ 0x042, { "8253 channel 2, Cassette and speaker functions", "" } },
	{ 0x043, { "8253 mode control  (see 8253)", "" } },
	{ 0x044, { "8254 PS/2 extended timer", "" } },
	{ 0x047, { "8254 Channel 3 control byte", "" } },
	// 060-06F  8042 Keyboard Controller  (AT,PS/2)
	{ 0x060, { "8042 Keyboard input/output buffer register", "" } },
	{ 0x061, { "8042 system control port (for compatability with 8255)", "" } },
	{ 0x064, { "8042 Keyboard command/status register", "" } },
};

const std::map<uint8_t, const char*> interruptInfo0 = {
	{ 0x20, "Program Terminate" },
};

const std::map<std::pair<uint8_t, uint8_t>, const char*> interruptInfo1 = {
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
