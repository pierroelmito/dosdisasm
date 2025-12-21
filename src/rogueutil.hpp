
/**
 * @section LICENSE
 * Copyright 2020 Sergei Akhmatdinov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied *.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdio>
#include <type_traits>
#include <utility>

namespace rogueutil {

using RuStr = const char*;

struct Vec {
	int x {};
	int y {};
};

inline bool operator==(Vec a, Vec b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Vec a, Vec b) { return !operator==(a, b); }

enum class Color {
	NONE,
	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	GREY,
	DARKGREY,
	LIGHTBLUE,
	LIGHTGREEN,
	LIGHTCYAN,
	LIGHTRED,
	LIGHTMAGENTA,
	YELLOW,
	WHITE
};

/* Constant strings for ANSI colors ans seqiences */
#define DEFSTR inline constexpr const char*
DEFSTR ANSI_CLS = "\033[2J\033[3J";
DEFSTR ANSI_CONSOLE_TITLE_PRE = "\033]0;";
DEFSTR ANSI_CONSOLE_TITLE_POST = "\007";
DEFSTR ANSI_ATTRIBUTE_RESET = "\033[0m";
DEFSTR ANSI_CURSOR_HIDE = "\033[?25l";
DEFSTR ANSI_CURSOR_SHOW = "\033[?25h";
DEFSTR ANSI_CURSOR_HOME = "\033[H";
DEFSTR ANSI_BLACK = "\033[22;30m";
DEFSTR ANSI_RED = "\033[22;31m";
DEFSTR ANSI_GREEN = "\033[22;32m";
DEFSTR ANSI_BROWN = "\033[22;33m";
DEFSTR ANSI_BLUE = "\033[22;34m";
DEFSTR ANSI_MAGENTA = "\033[22;35m";
DEFSTR ANSI_CYAN = "\033[22;36m";
DEFSTR ANSI_GREY = "\033[22;37m";
DEFSTR ANSI_DARKGREY = "\033[01;30m";
DEFSTR ANSI_LIGHTRED = "\033[01;31m";
DEFSTR ANSI_LIGHTGREEN = "\033[01;32m";
DEFSTR ANSI_YELLOW = "\033[01;33m";
DEFSTR ANSI_LIGHTBLUE = "\033[01;34m";
DEFSTR ANSI_LIGHTMAGENTA = "\033[01;35m";
DEFSTR ANSI_LIGHTCYAN = "\033[01;36m";
DEFSTR ANSI_WHITE = "\033[01;37m";
DEFSTR ANSI_BACKGROUND_BLACK = "\033[40m";
DEFSTR ANSI_BACKGROUND_RED = "\033[41m";
DEFSTR ANSI_BACKGROUND_GREEN = "\033[42m";
DEFSTR ANSI_BACKGROUND_YELLOW = "\033[43m";
DEFSTR ANSI_BACKGROUND_BLUE = "\033[44m";
DEFSTR ANSI_BACKGROUND_MAGENTA = "\033[45m";
DEFSTR ANSI_BACKGROUND_CYAN = "\033[46m";
DEFSTR ANSI_BACKGROUND_WHITE = "\033[47m";
#undef DEFSTR
/* Remaining colors not supported as background colors */

struct KeyCode {
	enum Type {
		KEY_ESCAPE = 0,
		KEY_ENTER = 1,
		KEY_SPACE = 32,
		KEY_INSERT = 2,
		KEY_HOME = 3,
		KEY_PGUP = 4,
		KEY_DELETE = 5,
		KEY_END = 6,
		KEY_PGDOWN = 7,
		KEY_UP = 14,
		KEY_DOWN = 15,
		KEY_LEFT = 16,
		KEY_RIGHT = 17,
		KEY_F1 = 18,
		KEY_F2 = 19,
		KEY_F3 = 20,
		KEY_F4 = 21,
		KEY_F5 = 22,
		KEY_F6 = 23,
		KEY_F7 = 24,
		KEY_F8 = 25,
		KEY_F9 = 26,
		KEY_F10 = 27,
		KEY_F11 = 28,
		KEY_F12 = 29,
		KEY_NUMDEL = 30,
		KEY_NUMPAD0 = 31,
		KEY_NUMPAD1 = 127,
		KEY_NUMPAD2 = 128,
		KEY_NUMPAD3 = 129,
		KEY_NUMPAD4 = 130,
		KEY_NUMPAD5 = 131,
		KEY_NUMPAD6 = 132,
		KEY_NUMPAD7 = 133,
		KEY_NUMPAD8 = 134,
		KEY_NUMPAD9 = 135,
	};
};

template <typename... T>
int f(RuStr fmt, const T&... args)
{
	return printf(fmt, args...);
}

inline int p(RuStr st) { return f("%s", st); }

int getch();
int kbhit();
int getkey();

Vec dim();
void setConsoleTitle(RuStr title);
RuStr getANSIFgColor(Color c);
RuStr getANSIBgColor(Color c);

inline void cls() { f("%s%s", ANSI_CLS, ANSI_CURSOR_HOME); }
inline void setCursor(bool v) { p(v ? ANSI_CURSOR_SHOW : ANSI_CURSOR_HIDE); }
inline void resetColor() { p(ANSI_ATTRIBUTE_RESET); }

template <typename T>
struct Apply {
	inline static void Do(const T& v) = delete;
};
template <>
struct Apply<Vec> {
	inline static void Do(Vec pos) { f("\033[%d;%dH", pos.y, pos.x); }
};
template <>
struct Apply<Color> {
	inline static void Do(Color c) { p(getANSIFgColor(c)); }
};
template <>
struct Apply<std::pair<Color, Color>> {
	static void Do(std::pair<Color, Color> attr)
	{
		Apply<Color>::Do(attr.first);
		if (attr.second != Color::NONE)
			p(getANSIBgColor(attr.second));
	}
};

template <typename F>
int tprint(F first)
{
	if constexpr (std::is_same<F, RuStr>()) {
		return p(first);
	} else {
		Apply<F>::Do(first);
		return 0;
	}
}

template <typename F, typename... T>
int tprint(F first, T... args)
{
	if constexpr (std::is_same<F, RuStr>()) {
		return f(first, args...);
	} else {
		Apply<F>::Do(first);
		return tprint<T...>(args...);
	}
}

} // namespace rogueutil

// ========================================================================

#include <sys/ioctl.h> /* for getkey() */
#include <sys/time.h> /* for kbhit() */
#include <sys/types.h> /* for kbhit() */
#include <termios.h> /* for getch() and kbhit() */
#include <unistd.h> /* for getch(), kbhit() and getuid() */

namespace rogueutil {

inline int getch()
{
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	int ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

inline int kbhit()
{
	static struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_iflag = 0; /* input mode */
	newt.c_oflag = 0; /* output mode */
	newt.c_cc[VMIN] = 1; /* minimum time to wait */
	newt.c_cc[VTIME] = 1; /* minimum characters to wait for */
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	int cnt = 0;
	ioctl(0, FIONREAD, &cnt); /* Read count */
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100;
	select(STDIN_FILENO + 1, NULL, NULL, NULL, &tv); /* A small time delay */
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return cnt; /* Return number of characters */
}

inline int getkey()
{
	int cnt = kbhit(); /* for ANSI escapes processing */
	int k = getch();
	switch (k) {
	case 0: {
		int kk;
		switch (kk = getch()) {
		case 71:
			return KeyCode::KEY_NUMPAD7;
		case 72:
			return KeyCode::KEY_NUMPAD8;
		case 73:
			return KeyCode::KEY_NUMPAD9;
		case 75:
			return KeyCode::KEY_NUMPAD4;
		case 77:
			return KeyCode::KEY_NUMPAD6;
		case 79:
			return KeyCode::KEY_NUMPAD1;
		case 80:
			return KeyCode::KEY_NUMPAD2;
		case 81:
			return KeyCode::KEY_NUMPAD3;
		case 82:
			return KeyCode::KEY_NUMPAD0;
		case 83:
			return KeyCode::KEY_NUMDEL;
		default:
			return kk - 59 + KeyCode::KEY_F1; /* Function keys */
		}
	}
	case 224: {
		int kk;
		switch (kk = getch()) {
		case 71:
			return KeyCode::KEY_HOME;
		case 72:
			return KeyCode::KEY_UP;
		case 73:
			return KeyCode::KEY_PGUP;
		case 75:
			return KeyCode::KEY_LEFT;
		case 77:
			return KeyCode::KEY_RIGHT;
		case 79:
			return KeyCode::KEY_END;
		case 80:
			return KeyCode::KEY_DOWN;
		case 81:
			return KeyCode::KEY_PGDOWN;
		case 82:
			return KeyCode::KEY_INSERT;
		case 83:
			return KeyCode::KEY_DELETE;
		default:
			return kk - 123 + KeyCode::KEY_F1; /* Function keys */
		}
	}
	case 13:
		return KeyCode::KEY_ENTER;
	case 155: /* single-character CSI */
	case 27: {
		/* Process ANSI escape sequences */
		if (cnt >= 3 && getch() == '[') {
			switch (k = getch()) {
			case 'A':
				return KeyCode::KEY_UP;
			case 'B':
				return KeyCode::KEY_DOWN;
			case 'C':
				return KeyCode::KEY_RIGHT;
			case 'D':
				return KeyCode::KEY_LEFT;
			default:
				return KeyCode::KEY_ESCAPE;
			}
		} else
			return KeyCode::KEY_ESCAPE;
	}
	default:
		return k;
	}
}

inline RuStr getANSIFgColor(Color c)
{
	switch (c) {
	case Color::BLACK:
		return ANSI_BLACK;
	case Color::BLUE:
		return ANSI_BLUE; /* non-ANSI */
	case Color::GREEN:
		return ANSI_GREEN;
	case Color::CYAN:
		return ANSI_CYAN; /* non-ANSI */
	case Color::RED:
		return ANSI_RED; /* non-ANSI */
	case Color::MAGENTA:
		return ANSI_MAGENTA;
	case Color::BROWN:
		return ANSI_BROWN;
	case Color::GREY:
		return ANSI_GREY;
	case Color::DARKGREY:
		return ANSI_DARKGREY;
	case Color::LIGHTBLUE:
		return ANSI_LIGHTBLUE; /* non-ANSI */
	case Color::LIGHTGREEN:
		return ANSI_LIGHTGREEN;
	case Color::LIGHTCYAN:
		return ANSI_LIGHTCYAN; /* non-ANSI; */
	case Color::LIGHTRED:
		return ANSI_LIGHTRED; /* non-ANSI; */
	case Color::LIGHTMAGENTA:
		return ANSI_LIGHTMAGENTA;
	case Color::YELLOW:
		return ANSI_YELLOW; /* non-ANSI */
	case Color::WHITE:
		return ANSI_WHITE;
	default:
		return "";
	}
}

inline RuStr getANSIBgColor(Color c)
{
	switch (c) {
	case Color::BLACK:
		return ANSI_BACKGROUND_BLACK;
	case Color::BLUE:
		return ANSI_BACKGROUND_BLUE;
	case Color::GREEN:
		return ANSI_BACKGROUND_GREEN;
	case Color::CYAN:
		return ANSI_BACKGROUND_CYAN;
	case Color::RED:
		return ANSI_BACKGROUND_RED;
	case Color::MAGENTA:
		return ANSI_BACKGROUND_MAGENTA;
	case Color::BROWN:
		return ANSI_BACKGROUND_YELLOW;
	case Color::GREY:
		return ANSI_BACKGROUND_WHITE;
	default:
		return "";
	}
}

inline Vec dim()
{
#ifdef TIOCGSIZE
	struct ttysize ts;
	ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
	return ts.ts_lines;
#elif defined(TIOCGWINSZ)
	struct winsize ts;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
	return { ts.ws_col, ts.ws_row };
#else /* TIOCGSIZE */
	return -1;
#endif /* TIOCGSIZE */
	return {};
}

inline void setConsoleTitle(RuStr title)
{
	p(ANSI_CONSOLE_TITLE_PRE);
	p(title);
	p(ANSI_CONSOLE_TITLE_POST);
}

} // namespace rogueutil
