-- Visual Studio 2022 x64 :
--   vendor\premake\premake5.exe vs2022
-- puis ouvrir vs2022\tps.sln

local function vendorRoot()
  if os.isdir("vendor/glfw") then
    return path.getabsolute("vendor")
  end
  if os.isdir("../../vendor/glfw") then
    return path.getabsolute("../../vendor")
  end
  error("Dossier vendor introuvable (glfw, glew, glm, premake).")
end

local vendor = vendorRoot()
local glewDll = vendor .. "/glew/lib/glew32.dll"
local glfwDll = vendor .. "/glfw/lib/glfw3.dll"
local assimpDll = vendor .. "/assimp/lib/assimp-vc145-mt.dll"
local lzmaDll = vendor .. "/lzma/lib/lzma.dll"

workspace "tps"
  architecture "x64"
  configurations { "Debug", "Release" }
  location "vs2022"
  cppdialect "C++20"
  multiprocessorcompile "On"

  filter "configurations:Debug"
    runtime "Debug"
    symbols "On"
  filter "configurations:Release"
    runtime "Release"
    optimize "On"
  filter "system:windows"
    defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN", "_CRT_SECURE_NO_WARNINGS" }
    systemversion "latest"
  filter {}

local function cppProject(name, dir)
  project(name)
    kind "ConsoleApp"
    language "C++"
    targetdir "%{wks.location}/bin/%{cfg.buildcfg}"
    objdir("%{wks.location}/obj/" .. name)
    files { dir .. "/src/**.cpp", dir .. "/src/**.hpp" }
    includedirs { dir .. "/src" }
    debugdir(dir)
end

local function withGl(dir, useGlfw, useGlm, useAssimp)
  includedirs { 
    vendor .. "/glew/include",
    vendor .. "/zlib",
    vendor .. "/lzma/include",
    vendor .. "/jpegturbo/include"
  }
  libdirs { 
    vendor .. "/glew/lib",
    vendor .. "/zlib",
    vendor .. "/lzma/lib",
    vendor .. "/jpegturbo/lib"
  }
  links { 
    "glew32", "opengl32",
    "zlib", "lzma", "jpeg-static", "turbojpeg-static"
  }
  if useGlfw then
    includedirs { vendor .. "/glfw/include" }
    libdirs { vendor .. "/glfw/lib" }
    links { "glfw3dll" }
  end
  if useAssimp then
    includedirs { vendor .. "/assimp/include" }
    libdirs { vendor .. "/assimp/lib" }
    links { "assimp-vc145-mt" }
  end
  if useGlm then
    includedirs { vendor .. "/glm/src" }
  end
  defines {
    'SHADER_DIR="' .. path.getabsolute(dir .. "/shaders"):gsub("\\", "/") .. '"',
  }
  filter "system:windows"
    local cmds = { '{COPYFILE} "' .. glewDll .. '" "%{cfg.targetdir}"' }
    table.insert(cmds, '{COPYFILE} "' .. assimpDll .. '" "%{cfg.targetdir}"')
    table.insert(cmds, '{COPYFILE} "' .. lzmaDll .. '" "%{cfg.targetdir}"')
    if useGlfw then
      table.insert(cmds, '{COPYFILE} "' .. glfwDll .. '" "%{cfg.targetdir}"')
    end
    postbuildcommands(cmds)
  filter {}
end

local function add(name, dir, useGlfw, useGlm, useAssimp)
  if not os.isdir(dir) then
    return
  end
  cppProject(name, dir)
  withGl(dir, useGlfw, useGlm, useAssimp)
end

add("tp2_exercice1", "tp2/exercice1", true, true, false)
add("tp2_exercice2", "tp2/exercice2", true, true, false)
add("tp2_exercice3", "tp2/exercice3", true, true, false)
add("tp3_squelette", "tp3/squelette", true, true, true)
