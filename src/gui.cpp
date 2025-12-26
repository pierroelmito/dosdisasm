
#include "ui.hpp"

#include <raylib.h>

void GuiMainDrawLine(UiCtx& /*ctx*/, int /*icode*/)
{
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
	ZyanU64 ra{ 0x100 };
	UiCtx ctx { {}, ra, content, {}, skips, {}, {}, {}, true };
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
