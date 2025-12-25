
#include "ui.hpp"

#include <raylib.h>

void GuiMainDrawLine(UiCtx& ctx, int icode)
{
	/*
	const int y = icode - ctx.start;
	const auto& o = ctx.l[icode];
	const bool selected = icode == int(ctx.s);
	//const auto tcol = ColFromCt(o.ct);
	//const auto col = selected ? A { ru::Color::WHITE, ru::Color::RED } : A { ru::Color::BROWN, ru::Color::NONE };

	const int maxSz = 6;
	int lsz = ru::tprint("%3d ", 1 + icode);
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
	*/
}

void GuiMainDraw(UiCtx& ctx)
{
	for (int icode = ctx.loc.start; icode - int(ctx.loc.start) < 10; ++icode) {
		if (icode < int(ctx.l.size()))
			GuiMainDrawLine(ctx, icode);
	}
}

void Gui(const Content& content, const Skips& skips)
{
	UiCtx ctx { {}, content, {}, skips, {}, {}, {}, true };
	CheckRecompile(ctx);
	InitWindow(800, 600, "dosdisasm");
	SetTargetFPS(30);
	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(PINK);
		EndDrawing();
	}
	CloseWindow();
}
