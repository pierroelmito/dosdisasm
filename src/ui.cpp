
#include "ui.hpp"

#include <chrono>

#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/process/popen.hpp>

UiCtx::UiCtx(const UiCtxParams& params)
	: binFilename(params.binFilename)
	, workFilename(params.workFilename)
	, ra(params.ra)
	, content(params.content)
	, meta(params.meta)
{
}

struct LoadCode : Dumper {
	Listing& listing;
	LoadCode(const std::set<ZyanU64>& el, const JumpFlagsMap& jl, Listing& l);
	virtual void dumpStr(const Ctx& ctx, DumpParams p, const char* const str) const override;
};

LoadCode::LoadCode(const std::set<ZyanU64>& el, const JumpFlagsMap& jl, Listing& l)
	: Dumper(el, jl)
	, listing(l)
{
}

void LoadCode::dumpStr(const Ctx& ctx, DumpParams p, const char* const str) const
{
	listing.push_back({ ctx.runtime_address, p.jump, p.sz, p.label ? p.label : "", str, p.comment ? p.comment : "", 0, p.ct });
}

std::vector<std::string> RunNasm()
{
	std::vector<std::string> r;
#if 1
	FILE* in = popen("nasm gsgsgsgsgsg.asm -o gsgsgsgsgsg.com", "r");
	if (in != nullptr) {
		char line[1024];
		while (fgets(line, sizeof(line), in) != nullptr) {
			r.push_back(line);
		}
		pclose(in);
	} else {
		r = { "unable to run nasm" };
	}
#else
	boost::asio::io_context io_context;
	boost::process::popen proc(io_context.get_executor(), "nasm", { "gsgsgsgsgsg.asm", "-o", "gsgsgsgsgsg.com" });
	boost::asio::streambuf buf;
	boost::asio::read_until(proc, buf, '\0');
	std::istream is(&buf);
	std::string line;
	while (std::getline(is, line)) {
		r.push_back(line);
	}
	proc.wait();
	// asio::write(proc, asio::buffer("main\n"));
	// std::string line;
	// asio::read_until(proc, asio::dynamic_buffer(line), '\n');
#endif
	return r;
}

void CheckRecompile(UiCtx& ctx)
{
	if (!ctx.dirty)
		return;
	ctx.dirty = false;

	ctx.header.clear();
	auto current = std::chrono::high_resolution_clock::now();
	auto q = [&](const char* lbl) {
		auto nc = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(nc - current).count();
		if (duration > 0) {
			current = nc;
			ctx.header.push_back(Fmt<256>("%s %lld ms", lbl, duration));
		}
	};

	Process prc;
	std::set<ZyanU64> existingLabels;
	JumpFlagsMap jumpLabels;
	{
		AnalyzeLabels anLbl { existingLabels, jumpLabels };
		prc.loop(ctx.ra, ctx.content, ctx.meta, anLbl);
	}
	ctx.l.clear();
	LoadCode load { existingLabels, jumpLabels, ctx.l };
	prc.loop(ctx.ra, ctx.content, ctx.meta, load);

	q("load");

	FILE* out = fopen("gsgsgsgsgsg.asm", "w");
	fprintf(out, "section .foo vstart=0x%04lX\n", ctx.ra);
	for (const auto& i : ctx.l) {
		if (!i.label.empty())
			fprintf(out, "%s: ", i.label.c_str());
		fprintf(out, "  %s\n", i.asmc.c_str());
	}
	fclose(out);

	q("write");

	ctx.nasmErrors = RunNasm();
	// std::system("nasm gsgsgsgsgsg.asm -o gsgsgsgsgsg.com");
	// std::system("fasm gsgsgsgsgsg.asm gsgsgsgsgsg.com");

	q("nasm");

	ctx.rebuild = ReadFile("gsgsgsgsgsg.com");
	for (auto& i : ctx.l) {
		bool equal = true;
		for (auto j = i.ra - ctx.ra; equal && j < i.ra - ctx.ra + i.sz; ++j) {
			if (j >= ctx.rebuild.size() || ctx.content[j] != ctx.rebuild[j])
				equal = false;
		}
		if (equal)
			i.cmp = Cmp::Same;
		else
			i.cmp = Cmp::Diff;
	}

	q("cmp");
}

std::optional<size_t> GetNearestSkipIndex(UiCtx& ctx, ZyanU64 ra)
{
	const std::pair<size_t, size_t> pos { ra - ctx.ra, 0 };
	auto its = std::lower_bound(ctx.meta.skips.begin(), ctx.meta.skips.end(), pos);
	if (its == ctx.meta.skips.end())
		return std::nullopt;
	return std::distance(ctx.meta.skips.begin(), its);
}

std::optional<size_t> GetSkipIndex(UiCtx& ctx, ZyanU64 ra)
{
	const std::pair<size_t, size_t> pos { ra - ctx.ra, 0 };
	auto its = std::lower_bound(ctx.meta.skips.begin(), ctx.meta.skips.end(), pos);
	if (its == ctx.meta.skips.end())
		return std::nullopt;
	if (pos.first < its->first || pos.first >= its->first + its->second)
		return std::nullopt;
	return std::distance(ctx.meta.skips.begin(), its);
}

std::optional<size_t> GetCurrentSkipIndex(UiCtx& ctx)
{
	const auto* current = ctx.loc.s < ctx.l.size() ? &ctx.l[ctx.loc.s] : nullptr;
	if (!current)
		return std::nullopt;
	return GetSkipIndex(ctx, current->ra);
}

void SkipShrink(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.meta.skips[i];
	skip.second -= 1;
	ctx.dirty = true;
}

void SkipExpand(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.meta.skips[i];
	skip.second += 1;
	ctx.dirty = true;
}

void SkipShiftLeft(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.meta.skips[i];
	skip.first -= 1;
	skip.second += 1;
	ctx.dirty = true;
}

void SkipShiftRight(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.meta.skips[i];
	skip.first += 1;
	skip.second -= 1;
	ctx.dirty = true;
}

void SkipAdd(UiCtx& ctx, size_t i, std::pair<size_t, size_t> s)
{
	ctx.meta.skips.insert(ctx.meta.skips.begin() + i, s);
	ctx.dirty = true;
}

void SkipRemove(UiCtx& ctx, size_t i)
{
	ctx.meta.skips.erase(ctx.meta.skips.begin() + i);
	ctx.dirty = true;
}

std::string SetActions(UiCtx& ctx, const Item* current)
{
	if (!current)
		return {};
	ctx.jump = current->jump;
	std::string status;
	const auto nsi = GetNearestSkipIndex(ctx, current->ra);
	const auto nsii = nsi.value_or(ctx.meta.skips.size());
	const auto si = GetSkipIndex(ctx, current->ra);
	if (!ctx.workFilename.empty()) {
		ctx.actions.push_back({ Action::Write, {} });
		ctx.actions.push_back({ Action::Read, {} });
	}
	if (si) {
		const auto [start, sz] = ctx.meta.skips[*si];
		status = "[" + std::to_string(nsii) + "] Skip from " + std::to_string(start) + " (" + std::to_string(sz) + ")";
		if (start + sz < ctx.content.size() && (*si == ctx.meta.skips.size() - 1 || ctx.meta.skips[*si + 1].first > start + sz)) {
			ctx.actions.push_back({ Action::SkExpand, [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipExpand(ctx, i);
									   else
										   SkipShrink(ctx, i);
								   } });
		}
		if (sz > 1) {
			ctx.actions.push_back({ Action::SkShrink, [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipShrink(ctx, i);
									   else
										   SkipExpand(ctx, i);
								   } });
		}
		if (sz > 1) {
			ctx.actions.push_back({ Action::SkShiftRight, [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipShiftRight(ctx, i);
									   else
										   SkipShiftLeft(ctx, i);
								   } });
		}
		if (start > 0 && (*si == 0 || ctx.meta.skips[*si - 1].first + sz < start)) {
			ctx.actions.push_back({ Action::SkShiftLeft, [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipShiftLeft(ctx, i);
									   else
										   SkipShiftRight(ctx, i);
								   } });
		}
		{
			std::pair<size_t, size_t> oskip = ctx.meta.skips[*si];
			ctx.actions.push_back({ Action::SkRemove, [=, loc = ctx.loc](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipRemove(ctx, nsii);
									   else
										   SkipAdd(ctx, nsii, oskip);
									   ;
								   } });
		}
	} else {
		status = "[" + std::to_string(nsii) + "] Code";
		if (current->jump)
			status += " jump to " + std::to_string(*current->jump);
		{
			std::pair<size_t, size_t> nskip { current->ra - ctx.ra, 1 };
			ctx.actions.push_back({ Action::SkAdd, [=, loc = ctx.loc](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipAdd(ctx, nsii, nskip);
									   else
										   SkipRemove(ctx, nsii);
									   ;
								   } });
		}
	}
	if (ctx.as.index > 0) {
		ctx.actions.push_back({ Action::Undo, {} });
	}
	if (ctx.as.index < ctx.as.actions.size()) {
		ctx.actions.push_back({ Action::Redo, {} });
	}
	return status;
}

bool BaseAction(UiCtx& ctx, Action a, size_t rh)
{
	switch (a) {
	case Action::MoveUp:
		if (ctx.loc.s > 0)
			ctx.loc.s--;
		break;
	case Action::MoveDown:
		if (ctx.loc.s < ctx.l.size() - 1)
			ctx.loc.s++;
		break;
	case Action::PageUp:
		if (ctx.loc.s > rh)
			ctx.loc.s -= rh;
		else
			ctx.loc.s = 0;
		break;
	case Action::PageDown:
		if (ctx.loc.s + rh < ctx.l.size())
			ctx.loc.s += rh;
		else
			ctx.loc.s = ctx.l.size() - 1;
		break;
	case Action::Undo:
		ctx.as.undo(ctx);
		break;
	case Action::Redo:
		ctx.as.redo(ctx);
		break;
	default:
		return false;
	}
	return true;
}
