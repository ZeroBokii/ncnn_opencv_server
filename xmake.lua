add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
add_rules("mode.debug", "mode.release")
set_encodings("utf-8")

set_project("ncnn_opencv_server")
set_languages("c++20")

-- 架构选项：amd (默认) 或 arm
option("libarch")
    set_default("amd")
    set_showmenu(true)
    set_description("Library architecture: amd or arm")
    set_values("amd", "arm")
option_end()

-- xmake 包管理
add_requires("nlohmann_json")

-- aarch64 交叉编译工具链
toolchain("aarch64-cross")
    set_kind("cross")
    set_sdkdir("/usr")
    set_bindir("/usr/bin")
    set_toolset("cc", "aarch64-linux-gnu-gcc")
    set_toolset("cxx", "aarch64-linux-gnu-g++")
    set_toolset("ld", "aarch64-linux-gnu-g++")
    set_toolset("ar", "aarch64-linux-gnu-ar")
    set_toolset("strip", "aarch64-linux-gnu-strip")
toolchain_end()

target("ncnn_opencv_server")
    set_kind("binary")
    set_targetdir("$(projectdir)/workspace")
    
    add_files("src/**.cpp")
    
    -- 头文件
    add_includedirs(
        "$(projectdir)/lib/$(libarch)/install_ncnn/include",
        "$(projectdir)/lib/$(libarch)/install_ncnn/include/ncnn",
        "$(projectdir)/lib/$(libarch)/install_opencv/include",
        "$(projectdir)/lib/$(libarch)/install_opencv/include/opencv4",
        "$(projectdir)/lib/$(libarch)/install_inotify/include",
        "$(projectdir)/lib/$(libarch)/install_spdlog/include",
        "$(projectdir)/lib/$(libarch)"
    )
    
    -- 库路径
    add_linkdirs(
        "$(projectdir)/lib/$(libarch)/install_ncnn/lib",
        "$(projectdir)/lib/$(libarch)/install_opencv/lib",
        "$(projectdir)/lib/$(libarch)/install_inotify/lib",
        "$(projectdir)/lib/$(libarch)/install_spdlog/lib"
    )
    
    -- 链接库
    add_links("ncnn", "opencv_core", "opencv_imgproc", "opencv_imgcodecs", 
              "inotify-cpp", "spdlog", "mosquitto", "pthread", "dl")
    
    -- xmake 包
    add_packages("nlohmann_json")
    
    -- 编译选项
    add_cxxflags("-Wno-deprecated-enum-enum-conversion", "-Wno-nonnull", "-fopenmp")
    add_ldflags("-fopenmp")
    
    -- rpath: 嵌入库路径
    add_rpathdirs(
        "$(projectdir)/lib/$(libarch)/install_ncnn/lib",
        "$(projectdir)/lib/$(libarch)/install_opencv/lib",
        "$(projectdir)/lib/$(libarch)/install_inotify/lib",
        "$(projectdir)/lib/$(libarch)/install_spdlog/lib"
    )
    
    -- ARM 交叉编译时使用自定义工具链
    on_load(function (target)
        import("core.project.config")
        local libarch = config.get("libarch") or "amd"
        if libarch == "arm" then
            target:set("toolchains", "aarch64-cross")
        end
    end)
