
add_rules("mode.debug", "mode.release")

local useGui = true
local useTui = true

target("dosdisasm")
	set_policy("build.warning", true)
	set_warnings("all", "extra")
	set_kind("binary")
	add_files("src/*.cpp")
	add_cxflags("-std=c++23")
	add_links("Zydis", "boost_program_options")
	set_rundir(".")
	add_defines("ENABLE_TUI=1")

	if is_mode("debug") then
		add_defines("DEBUG")
	end

	if useTui then
		add_defines("ENABLE_TUI=1")
	else
		add_defines("ENABLE_TUI=0")
	end

	if useGui then
		add_defines("ENABLE_GUI=1")
		add_links("raylib")
	else
		add_defines("ENABLE_GUI=0")
	end

