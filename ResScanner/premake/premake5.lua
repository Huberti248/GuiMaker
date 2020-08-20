workspace "ResScanner"

architecture "x86"
location "../"
staticruntime "on"

configurations{
	"Debug",
	"Release",
}

platforms{
	"x86",
}

project "ResScanner"
	location "../"
	language "C++"
	cppdialect "C++17"
	kind "ConsoleApp"

	targetdir "../bin/%{cfg.buildcfg}_%{cfg.platform}"
	objdir "../bin/obj/%{cfg.buildcfg}_%{cfg.platform}"

	files{
		"../main.cpp",
	}

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"
			