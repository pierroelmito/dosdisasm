
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
int GuiPrintf(Assets& asst, Dim d, int x, int y, Color c, const char* fmt, T... args)
{
	char buffer[512] {};
	const int r = snprintf(buffer, sizeof(buffer), fmt, args...);
	DrawTextEx(asst.fnt, buffer, { x * asst.fwidth, 2 + y * asst.sz }, asst.sz, 0, c);
	return r;
}

void GuiMainDrawLine(UiCtx& ctx, Assets& asst, Dim d, int y, int icode)
{
	const auto& o = ctx.l[icode];

	const int xNum = 0;
	const int xOld = xNum + 5;
	const int xNew = xOld + 7;
	const int xLabel = xNew + 7;
	const int xCode = xLabel + 8;

	if (icode == int(ctx.loc.s)) {
		DrawRectangleRec({ 0, y * asst.sz, float(d.w), asst.sz }, RED);
		DrawRectangleRec({ 0, y * asst.sz, float(d.w), 1 }, WHITE);
		DrawRectangleRec({ 0, (y + 1) * asst.sz - 1, float(d.w), 1 }, WHITE);
	}
	GuiPrintf(asst, d, xNum, y, YELLOW, "%04d", icode);
	if (o.label.empty())
		GuiPrintf(asst, d, xLabel, y, WHITE, "%04X", o.ra);
	else
		GuiPrintf(asst, d, xLabel, y, MAGENTA, "%-8.8s: ", o.label.c_str());

	const int sz = GuiPrintf(asst, d, xCode, y, BLUE, "%s", o.asmc.c_str());
	if (!o.comment.empty())
		GuiPrintf(asst, d, xCode + sz, y, GREEN, " ; %s", o.comment.c_str());
}

void GuiMainDraw(UiCtx& ctx, Assets& asst, Dim d)
{
	ctx.actions.clear();
	const auto* current = ctx.loc.s < ctx.l.size() ? &ctx.l[ctx.loc.s] : nullptr;
	const std::string status = SetActions(ctx, current);
	GuiPrintf(asst, d, 0, 0, WHITE, "%d %d - %lu of %lu", d.w, d.h, ctx.as.index, ctx.as.actions.size());
	const int count = int(d.h / asst.sz);
	for (int icode = ctx.loc.start; icode - int(ctx.loc.start) < count - 2; ++icode) {
		if (icode < int(ctx.l.size()))
			GuiMainDrawLine(ctx, asst, d, 1 + icode - int(ctx.loc.start), icode);
	}
	GuiPrintf(asst, d, 0, count - 1, WHITE, "%s", "status");
}

void Gui(const Content& content, const Skips& skips)
{
	Assets asst {};
	ZyanU64 ra { 0x100 };
	UiCtx ctx { {}, ra, content, {}, skips, {}, {}, {}, true };
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

	std::sort(ctx.skips.begin(), ctx.skips.end());

	while (!WindowShouldClose()) {
		const Dim d {
			GetScreenWidth(),
			GetScreenHeight(),
			int(GetScreenHeight() / asst.sz)
		};
		const auto rh = size_t(d.rows - 3);
		std::optional<Action> action {};
		for (const auto& [key, act] : actionMap) {
			if (IsKeyPressed(key))
				action = act;
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
		}
		{
			ctx.loc.start = std::min(ctx.loc.start, ctx.loc.s);
			if (ctx.loc.s > ctx.loc.start + rh)
				ctx.loc.start = ctx.loc.s - rh;
		}
		CheckRecompile(ctx);
		BeginDrawing();
		ClearBackground(DARKGRAY);
		GuiMainDraw(ctx, asst, d);
		EndDrawing();
	}
	CloseWindow();
}

#endif

