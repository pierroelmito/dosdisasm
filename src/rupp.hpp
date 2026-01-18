
#pragma once

#include <concepts>
#include <cstdio>
#include <type_traits>

#ifndef HANDLE_INPUT
#define HANDLE_INPUT 1
#endif

namespace rupp {

using RuStr = const char*;

struct Vec {
	int x {};
	int y {};
};

struct CtrlReset { };
inline constexpr CtrlReset Reset;

struct CtrlCls { };
inline constexpr CtrlCls Cls;

inline bool operator==(Vec a, Vec b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Vec a, Vec b) { return !operator==(a, b); }

inline unsigned char rgb(unsigned char r, unsigned char g, unsigned char b)
{
	return 16 + (r * 6 + g) * 6 + b;
}

struct Fg {
	const unsigned char index {};
};

struct Bg {
	const unsigned char index {};
};

inline constexpr Fg FgBlack { 0 };
inline constexpr Fg FgRed { 1 };
inline constexpr Fg FgGreen { 2 };
inline constexpr Fg FgBrown { 3 };
inline constexpr Fg FgBlue { 4 };
inline constexpr Fg FgMagenta { 5 };
inline constexpr Fg FgCyan { 6 };
inline constexpr Fg FgGrey { 7 };
inline constexpr Fg FgDarkgrey { 8 };
inline constexpr Fg FgLightRed { 9 };
inline constexpr Fg FgLightGreen { 10 };
inline constexpr Fg FgYellow { 11 };
inline constexpr Fg FgLightBlue { 12 };
inline constexpr Fg FgLightMagenta { 13 };
inline constexpr Fg FgLightCyan { 14 };
inline constexpr Fg FgWhite { 15 };

inline constexpr Bg BgBlack { 0 };
inline constexpr Bg BgRed { 1 };
inline constexpr Bg BgGreen { 2 };
inline constexpr Bg BgBrown { 3 };
inline constexpr Bg BgBlue { 4 };
inline constexpr Bg BgMagenta { 5 };
inline constexpr Bg BgCyan { 6 };
inline constexpr Bg BgGrey { 7 };
inline constexpr Bg BgDarkgrey { 8 };
inline constexpr Bg BgLightRed { 9 };
inline constexpr Bg BgLightGreen { 10 };
inline constexpr Bg BgYellow { 11 };
inline constexpr Bg BgLightBlue { 12 };
inline constexpr Bg BgLightMagenta { 13 };
inline constexpr Bg BgLightCyan { 14 };
inline constexpr Bg BgWhite { 15 };

#if HANDLE_INPUT

struct KeyCode {
	enum Type {
		Escape = 0,
		Enter = 1,
		Space = 32,
		Insert = 2,
		Home = 3,
		PgUp = 4,
		Delete = 5,
		End = 6,
		PgDown = 7,
		Up = 14,
		Down = 15,
		Left = 16,
		Right = 17,
		F1 = 18,
		F2 = 19,
		F3 = 20,
		F4 = 21,
		F5 = 22,
		F6 = 23,
		F7 = 24,
		F8 = 25,
		F9 = 26,
		F10 = 27,
		F11 = 28,
		F12 = 29,
		NumDel = 30,
		NumPad0 = 31,
		NumPad1 = 127,
		NumPad2 = 128,
		NumPad3 = 129,
		NumPad4 = 130,
		NumPad5 = 131,
		NumPad6 = 132,
		NumPad7 = 133,
		NumPad8 = 134,
		NumPad9 = 135,
	};
};

int getch();
int kbhit();
int getkey();

#endif

namespace impl {
	template <typename... T>
	int f(RuStr fmt, const T&... args)
	{
		return printf(fmt, args...);
	}
	inline int p(RuStr st) { return f("%s", st); }
	inline void cls()
	{
		const char* const ANSI_CLS = "\033[2J\033[3J";
		const char* const ANSI_CURSOR_HOME = "\033[H";
		impl::f("%s%s", ANSI_CLS, ANSI_CURSOR_HOME);
	}
} // namespace impl

Vec dim();

void setConsoleTitle(RuStr title);

inline void setCursor(bool v)
{
	const char* const ANSI_CURSOR_HIDE = "\033[?25l";
	const char* const ANSI_CURSOR_SHOW = "\033[?25h";
	impl::p(v ? ANSI_CURSOR_SHOW : ANSI_CURSOR_HIDE);
}

template <typename T>
struct Apply {
	inline static void Do(const T& v) = delete;
};
template <>
struct Apply<Vec> {
	inline static void Do(Vec pos) { impl::f("\033[%d;%dH", pos.y, pos.x); }
};
template <>
struct Apply<Fg> {
	inline static void Do(Fg c) { impl::f("\033[38;5;%dm", c.index); }
};
template <>
struct Apply<Bg> {
	inline static void Do(Bg c) { impl::f("\033[48;5;%dm", c.index); }
};
template <>
struct Apply<CtrlReset> {
	inline static void Do(CtrlReset)
	{
		const char* const ANSI_ATTRIBUTE_RESET = "\033[0m";
		impl::p(ANSI_ATTRIBUTE_RESET);
	}
};
template <>
struct Apply<CtrlCls> {
	inline static void Do(CtrlCls) { impl::cls(); }
};

template <typename F>
int put(F first)
{
	if constexpr (std::is_same<F, RuStr>()) {
		return impl::p(first);
	} else {
		Apply<F>::Do(first);
		return 0;
	}
}

template <typename F, typename... T>
int put(F first, T... args)
{
	if constexpr (std::is_same<F, RuStr>()) {
		return impl::f(first, args...) + put<T...>(args...);
	} else {
		Apply<F>::Do(first);
		return put<T...>(args...);
	}
}

template <typename F>
int fmt(F first)
{
	if constexpr (std::is_same<F, RuStr>()) {
		return impl::p(first);
	} else {
		Apply<F>::Do(first);
		return 0;
	}
}

template <typename F, typename... T>
int fmt(F first, T... args)
{
	if constexpr (std::is_same<F, RuStr>()) {
		return impl::f(first, args...);
	} else {
		Apply<F>::Do(first);
		return fmt<T...>(args...);
	}
}

template <std::invocable<Vec> DRAW, std::invocable<Vec, int> INPUT>
void tui(DRAW&& draw, INPUT&& input)
{
	setCursor(false);
	auto cd = dim();
	draw(cd);
	while (true) {
		const auto nd = dim();
		if (cd != nd) {
			cd = nd;
			draw(cd);
		}
		if (kbhit()) {
			int k = getkey();
			if (!input(cd, k))
				break;
			draw(cd);
		}
	}
	put(Reset);
	setCursor(true);
}

} // namespace rupp

// ========================================================================

#if HANDLE_INPUT

#include <sys/ioctl.h> /* for getkey() */
#include <sys/time.h> /* for kbhit() */
#include <sys/types.h> /* for kbhit() */
#include <termios.h> /* for getch() and kbhit() */
#include <unistd.h> /* for getch(), kbhit() and getuid() */

namespace rupp {

inline int getch()
{
	struct termios oldt {};
	tcgetattr(STDIN_FILENO, &oldt);

	struct termios newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	int ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return ch;
}

inline int kbhit()
{
	struct termios oldt {};
	tcgetattr(STDIN_FILENO, &oldt);

	struct termios newt = oldt;
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
			return KeyCode::NumPad7;
		case 72:
			return KeyCode::NumPad8;
		case 73:
			return KeyCode::NumPad9;
		case 75:
			return KeyCode::NumPad4;
		case 77:
			return KeyCode::NumPad6;
		case 79:
			return KeyCode::NumPad1;
		case 80:
			return KeyCode::NumPad2;
		case 81:
			return KeyCode::NumPad3;
		case 82:
			return KeyCode::NumPad0;
		case 83:
			return KeyCode::NumDel;
		default:
			return kk - 59 + KeyCode::F1; /* Function keys */
		}
	}
	case 224: {
		int kk;
		switch (kk = getch()) {
		case 71:
			return KeyCode::Home;
		case 72:
			return KeyCode::Up;
		case 73:
			return KeyCode::PgUp;
		case 75:
			return KeyCode::Left;
		case 77:
			return KeyCode::Right;
		case 79:
			return KeyCode::End;
		case 80:
			return KeyCode::Down;
		case 81:
			return KeyCode::PgDown;
		case 82:
			return KeyCode::Insert;
		case 83:
			return KeyCode::Delete;
		default:
			return kk - 123 + KeyCode::F1; /* Function keys */
		}
	}
	case 13:
		return KeyCode::Enter;
	case 155: /* single-character CSI */
	case 27: {
		/* Process ANSI escape sequences */
		if (cnt >= 3 && getch() == '[') {
			switch (k = getch()) {
			case 'A':
				return KeyCode::Up;
			case 'B':
				return KeyCode::Down;
			case 'C':
				return KeyCode::Right;
			case 'D':
				return KeyCode::Left;
			default:
				return KeyCode::Escape;
			}
		} else {
			return KeyCode::Escape;
		}
	}
	default:
		return k;
	}
}

} // namespace rupp

#endif

namespace rupp {

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
	const char* const ANSI_CONSOLE_TITLE_PRE = "\033]0;";
	const char* const ANSI_CONSOLE_TITLE_POST = "\007";
	impl::f("%s%s%s", ANSI_CONSOLE_TITLE_PRE, title, ANSI_CONSOLE_TITLE_POST);
}

} // namespace rupp
