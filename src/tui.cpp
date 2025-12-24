
#include "dosdisasm.hpp"

#include <algorithm>

#include "rogueutil.hpp"

namespace ru = rogueutil;

using V = ru::Vec;
using A = std::pair<ru::Color, ru::Color>;

struct LoadCode : Dumper {
	Listing& listing;
	LoadCode(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, Listing& l);
	virtual void dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, const char* const str, const char* const comment) const override;
};

LoadCode::LoadCode(const std::set<ZyanU64>& el, const std::set<ZyanU64>& jl, Listing& l)
	: Dumper(el, jl)
	, listing(l)
{
}

void LoadCode::dumpStr(const Ctx& ctx, CType ct, ZyanUSize sz, const char* label, const char* const str, const char* const comment) const
{
	listing.push_back({ ctx.runtime_address, sz, label ? label : "", str, comment ? comment : "", ct });
}

void Recompile(TuiCtx& ctx)
{
	if (!ctx.dirty)
		return;
	ctx.dirty = false;
	Process prc;
	std::set<ZyanU64> existingLabels;
	std::set<ZyanU64> jumpLabels;
	{
		AnalyzeLabels anLbl { existingLabels, jumpLabels };
		prc.loop(ctx.content, ctx.skips, anLbl);
	}
	ctx.l.clear();
	LoadCode load { existingLabels, jumpLabels, ctx.l };
	prc.loop(ctx.content, ctx.skips, load);
	FILE* out = fopen("gsgsgsgsgsg.asm", "w");
	fprintf(out, "section .foo vstart=0x100\n");
	for (const auto& i : ctx.l) {
		if (!i.label.empty())
			fprintf(out, "%s: ", i.label.c_str());
		fprintf(out, "  %s\n", i.asmc.c_str());
	}
	fclose(out);
	std::system("nasm gsgsgsgsgsg.asm -o gsgsgsgsgsg.com");
	// std::system("fasm gsgsgsgsgsg.asm gsgsgsgsgsg.com");
	ctx.rebuild = ReadFile("gsgsgsgsgsg.com");
}

ru::Color ColFromCt(CType ct)
{
	switch (ct) {
	case CType::Code:
		return ru::Color::WHITE;
	case CType::Db:
		return ru::Color::RED;
	case CType::Dup:
		return ru::Color::BLUE;
	}
	return ru::Color::BLACK;
}

std::optional<size_t> GetNearestSkipIndex(TuiCtx& ctx, ZyanU64 ra)
{
	const std::pair<size_t, size_t> pos { ra - 0x100, 0 };
	auto its = std::lower_bound(ctx.skips.begin(), ctx.skips.end(), pos);
	if (its == ctx.skips.end())
		return std::nullopt;
	return std::distance(ctx.skips.begin(), its);
}

std::optional<size_t> GetSkipIndex(TuiCtx& ctx, ZyanU64 ra)
{
	const std::pair<size_t, size_t> pos { ra - 0x100, 0 };
	auto its = std::lower_bound(ctx.skips.begin(), ctx.skips.end(), pos);
	if (its == ctx.skips.end())
		return std::nullopt;
	if (pos.first < its->first || pos.first >= its->first + its->second)
		return std::nullopt;
	return std::distance(ctx.skips.begin(), its);
}

std::optional<size_t> GetCurrentSkipIndex(TuiCtx& ctx)
{
	const auto* current = ctx.s < ctx.l.size() ? &ctx.l[ctx.s] : nullptr;
	if (!current)
		return std::nullopt;
	return GetSkipIndex(ctx, current->ra);
}

void TuiMainDrawLine(TuiCtx& ctx, ru::Vec, std::string& spaces, int icode)
{
	const int y = icode - ctx.start;
	const auto& o = ctx.l[icode];
	const bool selected = icode == int(ctx.s);
	const auto tcol = ColFromCt(o.ct);
	const auto col = selected ? A { ru::Color::WHITE, ru::Color::RED } : A { ru::Color::BROWN, ru::Color::NONE };

	ru::tprint(col);
	ru::tprint(V { 1, y + 2 });
	ru::tprint(tcol);

	const int maxSz = 6;
	int lsz = ru::tprint("%3d ", 1 + icode);
	ru::tprint(ru::Color::LIGHTMAGENTA);
	const auto* start = &ctx.content[o.ra - 0x100];
	if (o.sz > maxSz) {
		lsz += ru::tprint("...          ");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz)) {
				lsz += ru::tprint("%02X", start[i]);
			} else {
				lsz += ru::tprint("  ");
			}
		}
		lsz += ru::tprint(" ");
	}
	if (o.sz > maxSz) {
		lsz += ru::tprint("...          ");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz) && o.ra - 0x100 + i < ctx.rebuild.size()) {
				const auto b = ctx.rebuild[o.ra - 0x100 + i];
				const bool same = start[i] == b;
				ru::tprint(same ? ru::Color::LIGHTGREEN : ru::Color::LIGHTRED);
				lsz += ru::tprint("%02X", b);
			} else {
				lsz += ru::tprint("  ");
			}
		}
		lsz += ru::tprint(" ");
	}

	if (o.label.empty())
		lsz += ru::tprint(ru::Color::BROWN, "%04X      ", o.ra);
	else
		lsz += ru::tprint(ru::Color::LIGHTCYAN, "%-8.8s: ", o.label.c_str());

	lsz += ru::tprint(tcol, "%.*s", 64, o.asmc.c_str());

	if (!o.comment.empty())
		lsz += ru::tprint(ru::Color::BROWN, " ; %s", o.comment.c_str());

	ru::tprint("%s", spaces.data() + lsz);

	if (selected)
		ru::resetColor();
}

void SkipShrink(TuiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.second -= 1;
	ctx.dirty = true;
}

void SkipExpand(TuiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.second += 1;
	ctx.dirty = true;
}

void SkipShiftLeft(TuiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.first -= 1;
	skip.second += 1;
	ctx.dirty = true;
}

void SkipShiftRight(TuiCtx& ctx, size_t i)
{
	auto& skip = ctx.skips[i];
	skip.first += 1;
	skip.second -= 1;
	ctx.dirty = true;
}

void SkipAdd(TuiCtx& ctx, size_t i, std::pair<size_t, size_t> s)
{
	ctx.skips.insert(ctx.skips.begin() + i, s);
	ctx.dirty = true;
}

void SkipRemove(TuiCtx& ctx, size_t i)
{
	ctx.skips.erase(ctx.skips.begin() + i);
	ctx.dirty = true;
}

std::string SetActions(TuiCtx& ctx, const Item* current)
{
	if (!current)
		return {};
	std::string status;
	const auto nsi = GetNearestSkipIndex(ctx, current->ra);
	const auto nsii = nsi.value_or(ctx.skips.size());
	const auto si = GetSkipIndex(ctx, current->ra);
	if (si) {
		const auto [start, sz] = ctx.skips[*si];
		status = "[" + std::to_string(nsii) + "] Skip from " + std::to_string(start) + " (" + std::to_string(sz) + ")";
		{
			ctx.actions.push_back({ '+', "+ : expand", [i = *si](bool d, TuiCtx& ctx) {
									   if (d)
										   SkipExpand(ctx, i);
									   else
										   SkipShrink(ctx, i);
								   } });
		}
		if (sz > 1) {
			ctx.actions.push_back({ '-', "- : shrink", [i = *si](bool d, TuiCtx& ctx) {
									   if (d)
										   SkipShrink(ctx, i);
									   else
										   SkipExpand(ctx, i);
								   } });
		}
		if (sz > 1) {
			ctx.actions.push_back({ 's', "s : shift right", [i = *si](bool d, TuiCtx& ctx) {
									   if (d)
										   SkipShiftRight(ctx, i);
									   else
										   SkipShiftLeft(ctx, i);
								   } });
		}
		if (start > 0) {
			ctx.actions.push_back({ 'S', "S : shift left", [i = *si](bool d, TuiCtx& ctx) {
									   if (d)
										   SkipShiftLeft(ctx, i);
									   else
										   SkipShiftRight(ctx, i);
								   } });
		}
		{
			std::pair<size_t, size_t> oskip = ctx.skips[*si];
			ctx.actions.push_back({ 'x', "x : remove skip", [=](bool d, TuiCtx& ctx) {
									   if (d)
										   SkipRemove(ctx, nsii);
									   else
										   SkipAdd(ctx, nsii, oskip);
									   ;
								   } });
		}
	} else {
		status = "[" + std::to_string(nsii) + "] Code";
		{
			std::pair<size_t, size_t> nskip { current->ra - 0x100, 1 };
			ctx.actions.push_back({ 'c', "c : add skip", [=](bool d, TuiCtx& ctx) {
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

void TuiMainDraw(TuiCtx& ctx, ru::Vec d, std::string& spaces)
{
	ctx.actions.clear();
	if (int(spaces.size()) != d.x)
		spaces = std::string(d.x, ' ');
	const auto* current = ctx.s < ctx.l.size() ? &ctx.l[ctx.s] : nullptr;
	const std::string status = SetActions(ctx, current);
	ru::cls();
	ru::tprint(V { 1, 1 }, A { ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
	ru::tprint(V { 1, 1 }, ru::Color::WHITE, "%d %d - %lu of %lu", d.x, d.y, ctx.as.index, ctx.as.actions.size());
	ru::resetColor();
	for (int icode = ctx.start; icode - int(ctx.start) < d.y - 2; ++icode) {
		if (icode < int(ctx.l.size()))
			TuiMainDrawLine(ctx, d, spaces, icode);
	}
	ru::tprint(V { 1, d.y }, A { ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
	ru::tprint(V { 1, d.y }, ru::Color::WHITE);
	ru::tprint("%s", status.c_str());
	for (const auto& a : ctx.actions) {
		ru::tprint(" | %s", std::get<1>(a).c_str());
	}
	ru::resetColor();
	fflush(stdout);
}

void TuiMain(TuiCtx& ctx)
{
	std::sort(ctx.skips.begin(), ctx.skips.end());
	std::string spaces;
	auto cd = ru::dim();
	TuiMainDraw(ctx, cd, spaces);
	for (;;) {
		if (ru::kbhit()) {
			const char k = ru::getkey();
			const auto rh = cd.y - 3;
			if (k == 'q' || k == ru::KeyCode::KEY_ESCAPE) {
				break;
			} else if (k == ru::KeyCode::KEY_UP) {
				if (ctx.s > 0)
					ctx.s--;
			} else if (k == ru::KeyCode::KEY_DOWN) {
				if (ctx.s < ctx.l.size() - 1)
					ctx.s++;
			} else if (k == ru::KeyCode::KEY_LEFT) {
				if (ctx.s > rh)
					ctx.s -= rh;
				else
					ctx.s = 0;
			} else if (k == ru::KeyCode::KEY_RIGHT) {
				if (ctx.s + rh < ctx.l.size())
					ctx.s += rh;
				else
					ctx.s = ctx.l.size() - 1;
			} else {
				for (const auto& [ak, l, a] : ctx.actions) {
					if (k == ak) {
						if (ak == 'u')
							ctx.as.undo(ctx);
						else if (ak == 'U')
							ctx.as.redo(ctx);
						else
							ctx.as.add(a, ctx);
						break;
					}
				}
			}
			// move view
			{
				ctx.start = std::min(ctx.start, ctx.s);
				if (ctx.s > ctx.start + rh)
					ctx.start = ctx.s - rh;
			}
			Recompile(ctx);
			TuiMainDraw(ctx, cd, spaces);
		} else {
			const auto nd = ru::dim();
			if (cd != nd) {
				cd = nd;
				TuiMainDraw(ctx, cd, spaces);
			}
		}
	}
}

void Tui(const Content& content, const Skips& skips)
{
	TuiCtx ctx { {}, content, {}, skips, {}, 0, 0, {}, true };
	Recompile(ctx);
	TuiMain(ctx);
}
