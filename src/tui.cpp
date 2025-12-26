
#include "ui.hpp"

#include <algorithm>

#include "rogueutil.hpp"

namespace ru = rogueutil;

using V = ru::Vec;
using A = std::pair<ru::Color, ru::Color>;

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

void TuiMainDrawLine(UiCtx& ctx, ru::Vec, std::string& spaces, int icode)
{
	const auto bgSelected = A { ru::Color::WHITE, ru::Color::RED };
	const auto bgNormal = A { ru::Color::BROWN, ru::Color::NONE };
	const auto bgHighlight = A { ru::Color::BROWN, ru::Color::BROWN };

	const int y = icode - ctx.loc.start;
	const auto& o = ctx.l[icode];
	const bool selected = icode == int(ctx.loc.s);
	const bool highlight = ctx.jump && o.ra == ctx.jump;
	const auto tcol = highlight ? ru::Color::BLACK : ColFromCt(o.ct);
	const auto col = highlight ? bgHighlight : (selected ? bgSelected : bgNormal);

	ru::tprint(col);
	ru::tprint(V { 1, y + 2 });
	ru::tprint(tcol);

	const int maxSz = 6;
	int lsz = ru::tprint("%4d ", 1 + icode);
	ru::tprint(ru::Color::LIGHTMAGENTA);
	const auto* start = &ctx.content[o.ra - ctx.ra];
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
		ru::tprint(o.cmp != Cmp::Diff ? ru::Color::LIGHTGREEN : ru::Color::LIGHTRED);
		lsz += ru::tprint("...          ");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz) && o.ra - ctx.ra + i < ctx.rebuild.size()) {
				const auto b = ctx.rebuild[o.ra - ctx.ra + i];
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

	if (highlight || selected)
		ru::resetColor();
}

void TuiMainDraw(UiCtx& ctx, ru::Vec d, std::string& spaces)
{
	ctx.actions.clear();
	const auto* current = ctx.loc.s < ctx.l.size() ? &ctx.l[ctx.loc.s] : nullptr;
	const std::string status = SetActions(ctx, current);
	if (int(spaces.size()) != d.x)
		spaces = std::string(d.x, ' ');
	ru::cls();
	ru::tprint(V { 1, 1 }, A { ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
	ru::tprint(V { 1, 1 }, ru::Color::WHITE, "%d %d - %lu of %lu", d.x, d.y, ctx.as.index, ctx.as.actions.size());
	ru::resetColor();
	for (int icode = ctx.loc.start; icode - int(ctx.loc.start) < d.y - 2; ++icode) {
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

void TuiMain(UiCtx& ctx)
{
	std::sort(ctx.skips.begin(), ctx.skips.end());
	std::string spaces;
	auto cd = ru::dim();
	TuiMainDraw(ctx, cd, spaces);
	for (;;) {
		if (ru::kbhit()) {
			const char k = ru::getkey();
			const auto rh = size_t(cd.y - 3);
			if (k == 'q' || k == ru::KeyCode::KEY_ESCAPE) {
				break;
			} else if (k == ru::KeyCode::KEY_UP) {
				if (ctx.loc.s > 0)
					ctx.loc.s--;
			} else if (k == ru::KeyCode::KEY_DOWN) {
				if (ctx.loc.s < ctx.l.size() - 1)
					ctx.loc.s++;
			} else if (k == ru::KeyCode::KEY_LEFT) {
				if (ctx.loc.s > rh)
					ctx.loc.s -= rh;
				else
					ctx.loc.s = 0;
			} else if (k == ru::KeyCode::KEY_RIGHT) {
				if (ctx.loc.s + rh < ctx.l.size())
					ctx.loc.s += rh;
				else
					ctx.loc.s = ctx.l.size() - 1;
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
				ctx.loc.start = std::min(ctx.loc.start, ctx.loc.s);
				if (ctx.loc.s > ctx.loc.start + rh)
					ctx.loc.start = ctx.loc.s - rh;
			}
			CheckRecompile(ctx);
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
	ZyanU64 ra { 0x100 };
	UiCtx ctx { {}, ra, content, {}, skips, {}, {}, {}, true };
	CheckRecompile(ctx);
	ru::cls();
	ru::setCursor(false);
	TuiMain(ctx);
	ru::cls();
	ru::setCursor(true);
	ru::resetColor();
}
