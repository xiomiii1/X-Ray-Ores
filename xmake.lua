add_rules("mode.release")
set_policy("package.requires_lock", true)

package("preloader")
    set_homepage("https://github.com/LiteLDev/preloader-android")
    add_urls("https://github.com/LiteLDev/preloader-android.git")
    add_versions("main", "main")
    add_deps("cmake")
    on_install("android", function (package)
        import("package.tools.cmake").install(package)
    end)
package_end()

add_requires("preloader")

target("XRay")
    set_kind("shared")
    set_languages("c++20")
    set_strip("all")
    add_files("src/main.cpp")
    add_packages("preloader")
    if is_plat("android") then
        add_cxflags("-fPIC", "-Oz", "-ffunction-sections", "-fdata-sections", "-flto", "-fvisibility=hidden", "-fno-rtti", "-w")
        add_shflags("-Wl,--gc-sections", "-flto", "-Wl,--hash-style=gnu", "-Wl,-z,max-page-size=16384")
        add_links("android", "log")
    end
end
