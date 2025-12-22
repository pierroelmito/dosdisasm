
#include "dosdisasm.hpp"

#include "rogueutil.hpp"

namespace ru = rogueutil;

void Recompile()
{
	// std::filesystem::create_directory(".tmp");
	// std::system("nasm .tmp/yolo.asm -o .tmp/yolo.com");
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
}

void TuiMain(TuiCtx& ctx)
{
	std::string spaces;
	auto draw = [&](ru::Vec d) {
		using V = ru::Vec;
		using A = std::pair<ru::Color, ru::Color>;
		if (int(spaces.size()) != d.x)
			spaces = std::string(d.x, ' ');
		ru::cls();
		ru::tprint(V { 1, 1 }, A { ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
		ru::tprint(V { 1, d.y }, A { ru::Color::WHITE, ru::Color::BLUE }, "%s", spaces.c_str());
		ru::tprint(V { 1, 1 }, ru::Color::WHITE, "%d %d", d.x, d.y);
		ru::resetColor();
		for (int icode = ctx.start; icode - ctx.start < d.y - 2; ++icode) {
			if (icode < int(ctx.l.size())) {
				const auto y = icode - ctx.start;
				const auto& o = ctx.l[icode];
				const bool selected = icode == int(ctx.s);
				const auto tcol = ColFromCt(o.ct);
				const auto col = selected ? A { ru::Color::WHITE, ru::Color::RED } : A { ru::Color::BROWN, ru::Color::NONE };
				ru::tprint(col);
				ru::tprint(V { 1, y + 2 });
				ru::tprint(tcol);
				int lsz = ru::tprint("%3d ", y);
				if (o.label.empty())
					lsz += ru::tprint(ru::Color::BROWN, "%04X      ", o.ra);
				else
					lsz += ru::tprint(ru::Color::GREEN, "%-8.8s: ", o.label.c_str());
				lsz += ru::tprint(tcol, "%.*s", 64, o.asmc.c_str());
				ru::tprint("%s", spaces.data() + lsz);
				if (selected)
					ru::resetColor();
			}
		}
		fflush(stdout);
	};

	auto cd = ru::dim();
	draw(cd);
	for (;;) {
		if (ru::kbhit()) {
			char k = ru::getkey();
			if (k == 'q' || k == ru::KeyCode::KEY_ESCAPE) {
				break;
			} else if (k == 'e') {
			} else if (k == ru::KeyCode::KEY_UP) {
				if (ctx.s > 0)
					ctx.s--;
				ctx.start = std::min(ctx.start, ctx.s);
			} else if (k == ru::KeyCode::KEY_DOWN) {
				if (ctx.s < ctx.l.size() - 1)
					ctx.s++;
				if (ctx.s > ctx.start + cd.y - 3)
					ctx.start = ctx.s - cd.y + 3;
			}
			draw(cd);
		} else {
			const auto nd = ru::dim();
			if (cd != nd) {
				cd = nd;
				draw(cd);
			}
		}
	}
}

void Tui(const Content& content, Listing&& listing)
{
	TuiCtx ctx { content, listing, 0 };
	TuiMain(ctx);
}
