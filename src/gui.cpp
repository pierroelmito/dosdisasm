
#include "ui.hpp"

#if ENABLE_GUI

#include <raylib.h>

struct Dim {
	int w {};
	int h {};
	int rows {};
};

struct Assets {
	Font fnt {};
	float sz { 22.0f };
	float fwidth {};
};

template <typename... T>
int GuiPrintf(Assets& asst, Dim, int x, int y, Color c, const char* fmt, T... args)
{
	char buffer[512] {};
	const int r = snprintf(buffer, sizeof(buffer), fmt, args...);
	DrawTextEx(asst.fnt, buffer, { x * asst.fwidth, 2 + y * asst.sz }, asst.sz, 0, c);
	return r;
}

inline Color ColFromCt(CType ct)
{
	switch (ct) {
	case CType::Code:
		return WHITE;
	case CType::Db:
		return RED;
	case CType::Dup:
		return BLUE;
	case CType::Str:
		return GREEN;
	case CType::Ret:
		return YELLOW;
	}
	return BLACK;
}

void GuiMainDrawLine(UiCtx& ctx, Assets& asst, Dim d, int y, int icode)
{
	const auto& o = ctx.l[icode];

	const int xNum = 0;
	const int xOld = xNum + 5;
	const int xNew = xOld + 13;
	const int xLabel = xNew + 13;
	const int xCode = xLabel + 10;

	const bool highlight = ctx.jumpTo && o.ra == ctx.jumpTo;
	const auto tcol = ColFromCt(o.ct);

	if (icode == int(ctx.loc.s)) {
		DrawRectangleRec({ 0, y * asst.sz, float(d.w), asst.sz }, RED);
	} else {
		if (icode % 2 == 0)
			DrawRectangleRec({ 0, y * asst.sz, float(d.w), asst.sz }, BLACK);
	}
	if (highlight) {
		DrawRectangleRec({ 0, y * asst.sz, float(d.w), 1 }, WHITE);
		DrawRectangleRec({ 0, (y + 1) * asst.sz - 1, float(d.w), 1 }, WHITE);
	}

	GuiPrintf(asst, d, xNum, y, YELLOW, "%04d", icode);

	const int maxSz = 6;
	const auto* start = &ctx.content[o.ra - ctx.ra];
	if (o.sz > maxSz) {
		GuiPrintf(asst, d, xOld, y, WHITE, "...");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz)) {
				GuiPrintf(asst, d, xOld + 2 * i, y, WHITE, "%02X", start[i]);
			} else {
				break;
			}
		}
	}
	if (o.sz > maxSz) {
		GuiPrintf(asst, d, xNew, y, GREEN, "...");
	} else {
		for (int i = 0; i < maxSz; ++i) {
			if (i < int(o.sz) && o.ra - ctx.ra + i < ctx.rebuild.size()) {
				const auto b = ctx.rebuild[o.ra - ctx.ra + i];
				const bool same = start[i] == b;
				auto col = same ? GREEN : RED;
				GuiPrintf(asst, d, xNew + 2 * i, y, col, "%02X", b);
			} else {
				break;
			}
		}
	}

	if (o.label.empty()) {
		GuiPrintf(asst, d, xLabel, y, WHITE, "%04X", o.ra);
	} else {
		GuiPrintf(asst, d, xLabel, y, MAGENTA, "%-8.8s: ", o.label.c_str());
	}

	const int sz = GuiPrintf(asst, d, xCode, y, tcol, "%s", o.asmc.c_str());
	if (!o.comment.empty())
		GuiPrintf(asst, d, xCode + sz, y, GREEN, " ; %s", o.comment.c_str());
}

void GuiMainDraw(UiCtx& ctx, Assets& asst, Dim d, int reserved)
{
	ctx.actions.clear();
	const auto* current = ctx.loc.s < ctx.l.size() ? &ctx.l[ctx.loc.s] : nullptr;
	const std::string status = SetActions(ctx, current);
	const int count = int(d.h / asst.sz);

	{
		DrawRectangleRec({ 0, 0, float(d.w), asst.sz }, BLUE);
		int x = GuiPrintf(asst, d, 0, 0, WHITE, "%s", ctx.binFilename.c_str());
		for (const auto& h : ctx.header) {
			x += GuiPrintf(asst, d, x, 0, WHITE, " - %s", h.c_str());
		}
	}

	for (int icode = ctx.loc.start; icode - int(ctx.loc.start) < count - 2 - reserved; ++icode) {
		if (icode < int(ctx.l.size()))
			GuiMainDrawLine(ctx, asst, d, 1 + icode - int(ctx.loc.start), icode);
	}

	{
		DrawRectangleRec({ 0, (d.rows - 1) * asst.sz, float(d.w), asst.sz }, BLUE);
		int x = GuiPrintf(asst, d, 0, count - 1, WHITE, "%s", "status");
		for (const auto& a : ctx.actions) {
			x += GuiPrintf(asst, d, x, count - 1, WHITE, " | %s", actionLabels[std::get<0>(a)]);
		}
	}
}

void Gui(const UiCtxParams& params)
{
	Assets asst {};
	UiCtx ctx(params);
	CheckRecompile(ctx);
	InitWindow(800, 600, "dosdisasm");
	SetTargetFPS(30);
	asst.fnt = LoadFontEx("ProggyClean.ttf", 22, nullptr, 0);
	asst.fwidth = MeasureTextEx(asst.fnt, "w", 22, 0).x;

	const std::map<KeyboardKey, Action> actionMap = {
		{ KEY_UP, Action::MoveUp },
		{ KEY_DOWN, Action::MoveDown },
		{ KEY_LEFT, Action::PageUp },
		{ KEY_RIGHT, Action::PageDown },
		{ KEY_KP_ADD, Action::SkExpand },
		{ KEY_KP_SUBTRACT, Action::SkShrink },
		{ KEY_S, Action::SkShiftRight },
		//{ 'S', Action::SkShiftLeft },
		{ KEY_X, Action::SkRemove },
		{ KEY_C, Action::SkAdd },
		{ KEY_U, Action::Undo },
		//{ 'U', Action::Redo },
	};

	std::sort(ctx.meta.skips.begin(), ctx.meta.skips.end());

	Color clearColor(25, 25, 25, 255);

	while (!WindowShouldClose()) {
		const Dim d {
			GetScreenWidth(),
			GetScreenHeight(),
			int(GetScreenHeight() / asst.sz)
		};
		const int r = 0;
		const auto rh = size_t(d.rows - 3 - r);
		std::optional<Action> action {};
		for (const auto& [key, act] : actionMap) {
			if (IsKeyPressed(key) || IsKeyPressedRepeat(key)) {
				action = act;
				break;
			}
		}
		if (action) {
			if (!BaseAction(ctx, *action, rh)) {
				for (const auto& [ak, fn] : ctx.actions) {
					if (*action == ak) {
						ctx.as.add(fn, ctx);
						break;
					}
				}
			}
			CheckRecompile(ctx);
		}
		{
			ctx.loc.start = std::min(ctx.loc.start, ctx.loc.s);
			if (ctx.loc.s > ctx.loc.start + rh)
				ctx.loc.start = ctx.loc.s - rh;
		}
		BeginDrawing();
		ClearBackground(clearColor);
		GuiMainDraw(ctx, asst, d, r);
		EndDrawing();
	}
	CloseWindow();
}

#endif
