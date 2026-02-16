
#include "dosdisasm.hpp"

#include <cstdio>
#include <cstring>

#include <filesystem>
#include <iostream>
#include <ranges>

#include <boost/algorithm/string.hpp>

#include <sqlite3.h>

Content ReadFile(const char* filename)
{
	FILE* file = fopen(filename, "rb");
	if (!file)
		return {};
	Content buffer;
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
	// clang-format off
	desc.add_options()
		("help,h", "produce help message")
#if ENABLE_TUI
		("tui", po::bool_switch(), "use text mode interface")
#endif
#if ENABLE_GUI
		("gui", po::bool_switch(), "use graphical mode interface")
#endif
		("input,i", po::value<std::string>(), "input binary file to disassemble")
		("workfile,w", po::value<std::string>(), "work file")
		("output,o", po::value<std::string>(), "output asm file")
		("skip,k", po::value<std::vector<std::string>>(), "<start>,<length> skip <length> bytes from <start>");
	// clang-format on

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

Skips DetectStrings(const Content& content)
{
	Skips r;
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

Skips DetectRepeats(const Content& content)
{
	Skips r;
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

Skips MakeRanges(const po::variables_map& vm)
{
	Skips skip_ranges;
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

namespace SQL {

const char* InitDb = R"(
DROP TABLE IF EXISTS skips;
CREATE TABLE "skips" (
"start" INTEGER,
"length" INTEGER
);
)";

}

Skips MetaDataReadFromFile(const std::string& path)
{
	sqlite3* h {};
	sqlite3_open(path.c_str(), &h);
	if (!h)
		return {};
	Skips r;
	auto cb = [](void* data, int c, char** val, char**) -> int {
		if (c != 2)
			return SQLITE_ERROR;
		Skips& r = *((Skips*)data);
		const int s = atol(val[0]);
		const int l = atol(val[1]);
		r.push_back({ s, l });
		return SQLITE_OK;
	};
	const int resGet = sqlite3_exec(h, "SELECT * FROM skips;", cb, &r, nullptr);
	if (resGet != SQLITE_OK)
		return {};
	return r;
}

void MetaDataSaveToFile(const Skips& skips, const std::string& path)
{
	sqlite3* h {};
	sqlite3_open(path.c_str(), &h);
	if (!h)
		return;
	const int resInit = sqlite3_exec(h, SQL::InitDb, nullptr, nullptr, nullptr);
	if (resInit != SQLITE_OK)
		return;
	for (const auto& [s, l] : skips) {
		const std::string query = "insert into skips values (" + std::to_string(s) + "," + std::to_string(l) + ");";
		const int resInsert = sqlite3_exec(h, query.c_str(), nullptr, nullptr, nullptr);
		if (resInsert != SQLITE_OK)
			return;
	}
}

void CleanRanges(Skips& ranges)
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

Process::Process()
{
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_REAL_16, ZYDIS_STACK_WIDTH_16);
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
}

void Process::loop(ZyanU64 ra, const Content& content, const Skips& skip_ranges, const Cb& cb)
{
	const size_t file_size = content.size();
	const ZyanU8* const data = &content[0];

	ZyanU64 runtime_address = ra;
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
		auto nsync = std::min(offset + 6, itskip != skip_ranges.end() ? itskip->first : file_size);
		const bool ok = ZYAN_SUCCESS(ZydisDisassembleIntel(
			// ZYDIS_MACHINE_MODE_LONG_COMPAT_16,
			ZYDIS_MACHINE_MODE_LEGACY_16,
			// ZYDIS_MACHINE_MODE_REAL_16,
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
