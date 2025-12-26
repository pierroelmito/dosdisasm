
#include "ui.hpp"

struct LoadCode : Dumper {
	Listing& listing;
	LoadCode(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, Listing& l);
	virtual void dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, std::optional<ZyanU64> jump, const char* const str, const char* const comment) const override;
};

LoadCode::LoadCode(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, Listing& l)
	: Dumper(el, jl)
	, listing(l)
{
}

void LoadCode::dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, std::optional<ZyanU64> jump, const char* const str, const char* const comment) const
{
	listing.push_back({ ctx.runtime_address, jump, sz, label ? label : "", str, comment ? comment : "", ct });
}

void CheckRecompile(UiCtx& ctx)
{
	if (!ctx.dirty)
		return;
	ctx.dirty = false;
	Process prc;
	std::set<ZyanU64> existingLabels;
	std::set<ZyanU64> jumpLabels;
	{
		AnalyzeLabels anLbl { existingLabels, jumpLabels };
		prc.loop(ctx.ra, ctx.content, ctx.skips, anLbl);
	}
	ctx.l.clear();
	LoadCode load { existingLabels, jumpLabels, ctx.l };
	prc.loop(ctx.ra, ctx.content, ctx.skips, load);
	FILE* out = fopen("gsgsgsgsgsg.asm", "w");
	fprintf(out, "section .foo vstart=0x%04lX\n", ctx.ra);
	for (const auto& i : ctx.l) {
		if (!i.label.empty())
			fprintf(out, "%s: ", i.label.c_str());
		fprintf(out, "  %s\n", i.asmc.c_str());
	}
	fclose(out);
	std::system("nasm gsgsgsgsgsg.asm -o gsgsgsgsgsg.com");
	// std::system("fasm gsgsgsgsgsg.asm gsgsgsgsgsg.com");
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
}

std::optional<size_t> GetNearestSkipIndex(UiCtx& ctx, ZyanU64 ra)
{
	const std::pair<size_t, size_t> pos { ra - ctx.ra, 0 };
	auto its = std::lower_bound(ctx.skips.begin(), ctx.skips.end(), pos);
	if (its == ctx.skips.end())
		return std::nullopt;
	return std::distance(ctx.skips.begin(), its);
}

std::optional<size_t> GetSkipIndex(UiCtx& ctx, ZyanU64 ra)
{
	const std::pair<size_t, size_t> pos { ra - ctx.ra, 0 };
	auto its = std::lower_bound(ctx.skips.begin(), ctx.skips.end(), pos);
	if (its == ctx.skips.end())
		return std::nullopt;
	if (pos.first < its->first || pos.first >= its->first + its->second)
		return std::nullopt;
	return std::distance(ctx.skips.begin(), its);
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
	auto& skip = ctx.skips[i];
	skip.second -= 1;
	ctx.dirty = true;
}

void SkipExpand(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.second += 1;
	ctx.dirty = true;
}

void SkipShiftLeft(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.first -= 1;
	skip.second += 1;
	ctx.dirty = true;
}

void SkipShiftRight(UiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.first += 1;
	skip.second -= 1;
	ctx.dirty = true;
}

void SkipAdd(UiCtx& ctx, size_t i, std::pair<size_t, size_t> s)
{
	ctx.skips.insert(ctx.skips.begin() + i, s);
	ctx.dirty = true;
}

void SkipRemove(UiCtx& ctx, size_t i)
{
	ctx.skips.erase(ctx.skips.begin() + i);
	ctx.dirty = true;
}

std::string SetActions(UiCtx& ctx, const Item* current)
{
	if (!current)
		return {};
	ctx.jump = current->jump;
	std::string status;
	const auto nsi = GetNearestSkipIndex(ctx, current->ra);
	const auto nsii = nsi.value_or(ctx.skips.size());
	const auto si = GetSkipIndex(ctx, current->ra);
	if (si) {
		const auto [start, sz] = ctx.skips[*si];
		status = "[" + std::to_string(nsii) + "] Skip from " + std::to_string(start) + " (" + std::to_string(sz) + ")";
		{
			ctx.actions.push_back({ '+', "+ : expand", [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipExpand(ctx, i);
									   else
										   SkipShrink(ctx, i);
								   } });
		}
		if (sz > 1) {
			ctx.actions.push_back({ '-', "- : shrink", [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipShrink(ctx, i);
									   else
										   SkipExpand(ctx, i);
								   } });
		}
		if (sz > 1) {
			ctx.actions.push_back({ 's', "s : shift right", [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipShiftRight(ctx, i);
									   else
										   SkipShiftLeft(ctx, i);
								   } });
		}
		if (start > 0) {
			ctx.actions.push_back({ 'S', "S : shift left", [loc = ctx.loc, i = *si](bool d, UiCtx& ctx) {
									   ctx.loc = loc;
									   if (d)
										   SkipShiftLeft(ctx, i);
									   else
										   SkipShiftRight(ctx, i);
								   } });
		}
		{
			std::pair<size_t, size_t> oskip = ctx.skips[*si];
			ctx.actions.push_back({ 'x', "x : remove skip", [=, loc = ctx.loc](bool d, UiCtx& ctx) {
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
			ctx.actions.push_back({ 'c', "c : add skip", [=, loc = ctx.loc](bool d, UiCtx& ctx) {
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
		ctx.actions.push_back({ 'u', "u : undo", {} });
	}
	if (ctx.as.index < ctx.as.actions.size()) {
		ctx.actions.push_back({ 'U', "U : redo", {} });
	}
	return status;
}
