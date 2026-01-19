
#include "ui.hpp"

#if ENABLE_TUI

#include <algorithm>

#include "rupp.hpp"

namespace ru = rupp;

using V = ru::Vec;

inline ru::Fg ColFromCt(CType ct)
{
	switch (ct) {
	case CType::Code:
		return ru::FgWhite;
	case CType::Db:
		return ru::FgRed;
	case CType::Dup:
		return ru::Fg { 39 };
	case CType::Str:
		return ru::FgGreen;
	case CType::Ret:
		return ru::FgYellow;
	}
	return ru::FgBlack;
}

void TuiMainDrawLine(UiCtx& ctx, ru::Vec, std::string& spaces, int icode)
{
	const auto bgSelected = ru::Bg { 25 };
	const auto bgNormal = ru::Bg { 235 };
	const auto bgHighlight = ru::Bg { 58 };

	const int y = icode - ctx.loc.start;
	const auto& o = ctx.l[icode];
	const bool selected = icode == int(ctx.loc.s);
	const bool highlight = ctx.jump && o.ra == ctx.jump;
	const auto tcol = ColFromCt(o.ct);
	const auto col = highlight ? bgHighlight : (selected ? bgSelected : bgNormal);

	ru::put(tcol, V { 1, y + 2 });

	const int maxSz = 6;
	int lsz = ru::fmt(col, "%4d ", 1 + icode);
	ru::put(ru::FgLightMagenta);
	const auto* start = &ctx.content[o.ra - ctx.ra];
	if (o.sz > maxSz) {
		lsz += ru::put("...          ");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz)) {
				lsz += ru::fmt("%02X", start[i]);
			} else {
				lsz += ru::put("  ");
			}
		}
		lsz += ru::put(" ");
	}
	if (o.sz > maxSz) {
		ru::put(o.cmp != Cmp::Diff ? ru::FgLightGreen : ru::FgLightRed);
		lsz += ru::put("...          ");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz) && o.ra - ctx.ra + i < ctx.rebuild.size()) {
				const auto b = ctx.rebuild[o.ra - ctx.ra + i];
				const bool same = start[i] == b;
				ru::put(same ? ru::FgLightGreen : ru::FgLightRed);
				lsz += ru::fmt("%02X", b);
			} else {
				lsz += ru::put("  ");
			}
		}
		lsz += ru::put(" ");
	}

	if (o.label.empty())
		lsz += ru::fmt(ru::FgBrown, "%04X      ", o.ra);
	else
		lsz += ru::fmt(ru::FgLightCyan, "%-8.8s: ", o.label.c_str());

	lsz += ru::fmt(tcol, "%.*s", 64, o.asmc.c_str());

	if (!o.comment.empty())
		lsz += ru::fmt(ru::FgBrown, " ; %s", o.comment.c_str());

	ru::fmt("%s", spaces.data() + lsz);

	if (highlight || selected)
		ru::put(ru::Reset);
}

void TuiMainDraw(UiCtx& ctx, ru::Vec d, std::string& spaces, const std::array<std::string, Action::Count>& actionKeys, int r)
{
	ctx.actions.clear();
	const auto* current = ctx.loc.s < ctx.l.size() ? &ctx.l[ctx.loc.s] : nullptr;
	const std::string status = SetActions(ctx, current);
	if (int(spaces.size()) != d.x)
		spaces = std::string(d.x, ' ');
	ru::put(ru::Cls);

	// header
	{
		ru::fmt(V { 1, 1 }, ru::BgBlue, "%s", spaces.c_str());
		ru::fmt(V { 1, 1 }, ru::FgWhite, "%s", ctx.filename.c_str());
		ru::fmt(" - %lu", ctx.nasmErrors.size());
		for (const auto& h : ctx.header) {
			ru::fmt(" - %s", h.c_str());
		}
	}

	// code
	{
		ru::put(ru::Reset);
		for (int icode = ctx.loc.start; icode - int(ctx.loc.start) < d.y - 2 - r; ++icode) {
			if (icode < int(ctx.l.size()))
				TuiMainDrawLine(ctx, d, spaces, icode);
		}
	}

	// footer
	{
		ru::fmt(V { 1, d.y }, ru::BgBlue, "%s", spaces.c_str());
		ru::put(V { 1, d.y }, ru::FgWhite);
		ru::fmt("%s", status.c_str());
		for (const auto& a : ctx.actions) {
			const auto action = std::get<0>(a);
			if (!actionKeys[action].empty()) {
				ru::put(" | ");
				ru::fmt(ru::FgYellow, "%s", actionKeys[action].c_str());
				ru::fmt(ru::FgWhite, " : %s", actionLabels[action]);
			}
		}
	}

	ru::put(ru::Reset);
	fflush(stdout);
}

void TuiMain(UiCtx& ctx)
{
	std::sort(ctx.skips.begin(), ctx.skips.end());

	const std::map<char, Action> actionMap = {
		{ ru::KeyCode::Up, Action::MoveUp },
		{ ru::KeyCode::Down, Action::MoveDown },
		{ ru::KeyCode::Left, Action::PageUp },
		{ ru::KeyCode::Right, Action::PageDown },
		{ '+', Action::SkExpand },
		{ '-', Action::SkShrink },
		{ 's', Action::SkShiftRight },
		{ 'S', Action::SkShiftLeft },
		{ 'x', Action::SkRemove },
		{ 'c', Action::SkAdd },
		{ 'u', Action::Undo },
		{ 'U', Action::Redo },
	};

	std::array<std::string, Action::Count> actionKeys;
	for (const auto& [key, action] : actionMap) {
		if (printable(key))
			actionKeys[action] = key;
	}

	std::string spaces;
	auto cd = ru::dim();
	int r = int(ctx.nasmErrors.size());
	TuiMainDraw(ctx, cd, spaces, actionKeys, r);
	for (;;) {
		if (ru::kbhit()) {
			const char k = ru::getkey();
			const auto rh = size_t(cd.y - 3 - r);
			std::optional<Action> action {};
			for (const auto& [key, act] : actionMap) {
				if (k == key) {
					action = act;
					break;
				}
			}
			if (k == 'q') {
				break;
			} else if (action) {
				if (!BaseAction(ctx, *action, rh)) {
					for (const auto& [ak, fn] : ctx.actions) {
						if (*action == ak) {
							ctx.as.add(fn, ctx);
							break;
						}
					}
				}
			}
			// move view
			{
				ctx.loc.start = std::min(ctx.loc.start, ctx.loc.s);
				if (ctx.loc.s > ctx.loc.start + rh)
					ctx.loc.start = ctx.loc.s - rh;
			}
			CheckRecompile(ctx);
			r = int(ctx.nasmErrors.size());
			TuiMainDraw(ctx, cd, spaces, actionKeys, r);
		} else {
			const auto nd = ru::dim();
			if (cd != nd) {
				cd = nd;
				TuiMainDraw(ctx, cd, spaces, actionKeys, r);
			}
		}
	}
}

void Tui(const UiCtxParams& params)
{
	UiCtx ctx(params);
	CheckRecompile(ctx);
	ru::put(ru::Cls);
	ru::setCursor(false);
	TuiMain(ctx);
	ru::put(ru::Cls);
	ru::setCursor(true);
	ru::put(ru::Reset);
}

#endif
