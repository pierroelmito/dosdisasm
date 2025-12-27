
#pragma once

#include "dosdisasm.hpp"

enum Action : size_t {
	Quit,
	MoveUp,
	MoveDown,
	PageUp,
	PageDown,
	SkExpand,
	SkShrink,
	SkShiftRight,
	SkShiftLeft,
	SkRemove,
	SkAdd,
	Undo,
	Redo,
	Count,
};

constexpr const char* const actionLabels[Action::Count] = {
	"",
	"",
	"",
	"",
	"",
	"expand",
	"shrink",
	"shift right",
	"shift left",
	"remove skip",
	"add skip",
	"undo",
	"redo",
};

template <typename... T>
struct ActionStack {
	using Action = std::function<void(bool, T...)>;
	std::vector<Action> actions;
	size_t index {};
	void add(Action a, T... params)
	{
		actions.resize(index);
		actions.emplace_back(a)(true, params...);
		index++;
	}
	void undo(T... params)
	{
		if (index > 0)
			actions[--index](false, params...);
	}
	void redo(T... params)
	{
		if (index < actions.size())
			actions[index++](true, params...);
	}
};

struct Loc {
	size_t start {};
	size_t s {};
};

struct UiCtx {
	using As = ActionStack<UiCtx&>;
	As as {};
	const ZyanU64 ra {};
	const Content& content;
	Content rebuild;
	Skips skips;
	Listing l;
	Loc loc {};
	std::vector<std::tuple<Action, As::Action>> actions {};
	bool dirty { true };
	std::optional<ZyanU64> jump {};
};

void CheckRecompile(UiCtx& ctx);

std::optional<size_t> GetNearestSkipIndex(UiCtx& ctx, ZyanU64 ra);
std::optional<size_t> GetSkipIndex(UiCtx& ctx, ZyanU64 ra);
std::optional<size_t> GetCurrentSkipIndex(UiCtx& ctx);

std::string SetActions(UiCtx& ctx, const Item* current);
bool BaseAction(UiCtx& ctx, Action a, size_t rh);
