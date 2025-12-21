
add_rules("mode.debug", "mode.release")

target("dosdisasm")
	set_policy("build.warning", true)
	set_warnings("all", "extra")
	set_kind("binary")
	add_files("src/*.cpp")
	add_cxflags("-std=c++23")
	add_links("Zydis", "boost_program_options")
	set_rundir(".")
	if is_mode("debug") then
		add_defines("DEBUG")
	end

