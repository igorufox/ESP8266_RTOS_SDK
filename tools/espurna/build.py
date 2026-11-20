#!/usr/bin/env python3
"""The binary package of ESP8266_RTOS_SDK for ESPurna (as Espressif's esp32-arduino-lib-builder for Arduino-ESP32): the
SDK compiled once here, by the release of this branch, and linked by the firmware - the firmware never compiles it.

    build.py --toolchain DIR --lx106 DIR --kernel DIR --mbedtls DIR --version V --out DIR [--work DIR] [--test]

  --toolchain  Espressif's xtensa-esp-elf (GCC 14.2, bin/ and lib/xtensa_esp8266.so)
  --lx106      the lx106 target libraries of that GCC (igorufox/toolchain-xtensa-lx106: newlib, libgcc, libstdc++)
  --kernel     FreeRTOS-Kernel (igorufox/FreeRTOS-Kernel), --mbedtls  Mbed TLS 3.6 (igorufox/mbedtls)

The SDK is configured with tools/espurna/sdkconfig.defaults, its CMake builds the components and the bootloader, and
the package (--out, also zipped next to it) gets:

  package.json               framework-esp8266-rtos-sdk <version>
  flags.json                 per image ("app", "bootloader"): includes, defines, cflags, cxxflags and link - the SDK's
                             own command lines, every path in them as ${PKG}/... (this package) or ${LX106}/... (the
                             lx106 libraries: not in here, the firmware has them); and what it was built of
  include/{sdk,build,kernel,mbedtls}/...   the headers of every include folder of those command lines
  app/lib/*.a, app/ld/*.ld   the archives and linker scripts of the app (the firmware's objects go before them)
  bootloader/lib/*.a, bootloader/ld/*.ld   the same of the bootloader: the firmware links its own bootloader, its
                             objects first (bootloader_hooks.h: bootloader_after_init() of the firmware replaces the
                             weak one)
  config/                    sdkconfig, sdkconfig.h, sdkconfig.json
  tools/                     esptool (elf2image) and gen_esp32part.py (the partition table of the firmware)

--test links a test app and a test bootloader from the package alone, by flags.json, as a firmware does.
"""

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.dirname(os.path.dirname(HERE))

# the components the firmware links; main (empty) pulls the common ones, these the rest
COMPONENTS = ["main", "esptool_py", "pthread", "nvs_flash", "app_update", "spi_flash"]
HEADERS = (".h", ".hpp", ".hh", ".inc", ".def", ".tcc")


def posix(path):
    return os.path.abspath(path).replace(os.sep, "/").rstrip("/")


def run(args, cwd, env, what):
    print("build.py: %s" % what, flush=True)
    result = subprocess.run(args, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            universal_newlines=True)
    if result.returncode != 0:
        sys.stdout.write(result.stdout[-20000:])
        sys.stdout.write("\n".join(line for line in result.stdout.splitlines()
                                   if line.startswith("FAILED:") or " error:" in line
                                   or "undefined reference" in line) + "\n")
        raise SystemExit("build.py: %s failed" % what)
    return result.stdout


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


class Package:
    """Paths of the build's command lines -> paths of the package; the files they name are copied in"""

    def __init__(self, out, roots, lx106):
        self.out = out
        # longest first: the build folder may lie inside another root
        self.roots = sorted(((posix(path), name) for path, name in roots.items()), key=lambda r: -len(r[0]))
        self.lx106 = posix(lx106)
        self.copied = set()

    def where(self, path):
        path = posix(path)
        if path == self.lx106 or path.startswith(self.lx106 + "/"):
            return "${LX106}" + path[len(self.lx106):], None
        for root, name in self.roots:
            if path == root or path.startswith(root + "/"):
                return "${PKG}/include/%s%s" % (name, path[len(root):]), name + path[len(root):]
        raise SystemExit("build.py: %s is outside the SDK, the build, the kernel, mbedtls and the lx106 libraries" % path)

    def include(self, path):
        token, inside = self.where(path)
        if inside is not None and path not in self.copied:
            self.copied.add(path)
            target = os.path.join(self.out, "include", inside)
            for folder, _, files in os.walk(path):
                for name in files:
                    if name.endswith(HEADERS):
                        rel = os.path.relpath(os.path.join(folder, name), path)
                        os.makedirs(os.path.dirname(os.path.join(target, rel)), exist_ok=True)
                        shutil.copy2(os.path.join(folder, name), os.path.join(target, rel))
        return token


def compile_flags(package, commands_path, c_suffix, cxx_suffix):
    with open(commands_path, encoding="utf-8") as f:
        commands = json.load(f)

    def find(suffix):
        for entry in commands:
            if posix(entry["file"]).endswith(suffix):
                return entry
        return None

    def split(entry):
        tokens = shlex.split(entry["command"].replace("\\", "/"), posix=True)
        includes, defines, flags = [], [], []
        pending = None
        for token in tokens[1:]:
            if pending:
                if pending in ("-I", "-isystem"):
                    path = token if os.path.isabs(token) else os.path.join(entry["directory"], token)
                    (includes if pending == "-I" else flags).append(
                        ("" if pending == "-I" else "-isystem") + package.include(path))
                pending = None
                continue
            if token in ("-o", "-c", "-I", "-isystem", "-MF", "-MT", "-MQ"):
                pending = token
                continue
            if token.startswith("-I"):
                path = token[2:]
                includes.append(package.include(path if os.path.isabs(path)
                                                else os.path.join(entry["directory"], path)))
            elif token.startswith("-isystem"):
                flags.append("-isystem" + package.include(token[len("-isystem"):]))
            elif token.startswith("-D"):
                defines.append(token[2:])
            elif token in ("-MD", "-MMD"):
                continue
            elif token.startswith("-"):
                flags.append(token)
        return includes, defines, flags

    c_entry = find(c_suffix)
    if not c_entry:
        raise SystemExit("build.py: %s is not in %s" % (c_suffix, commands_path))
    includes, defines, cflags = split(c_entry)
    cxx_entry = find(cxx_suffix) if cxx_suffix else None
    cxxflags = split(cxx_entry)[2] if cxx_entry else list(cflags)
    return includes, defines, cflags, cxxflags


def link_flags(package, build_dir, ninja_file, target, kind, skip_archives=()):
    """LINK_FLAGS and LINK_LIBRARIES of the SDK's own link of target: archives -> <kind>/lib, the scripts of every -L
    folder -> <kind>/ld (one -L of the package first), the lx106 libraries as ${LX106}"""
    with open(ninja_file, encoding="utf-8") as f:
        lines = f.read().splitlines()

    values = {}
    inside = False
    for line in lines:
        if line.startswith("build %s:" % target):
            inside = True
            continue
        if inside:
            if not line.startswith("  "):
                break
            key, _, value = line.strip().partition(" = ")
            values[key] = value
    if not values:
        raise SystemExit("build.py: no link of %s in %s" % (target, ninja_file))

    lib_dir = os.path.join(package.out, kind, "lib")
    ld_dir = os.path.join(package.out, kind, "ld")
    os.makedirs(lib_dir, exist_ok=True)
    os.makedirs(ld_dir, exist_ok=True)

    def copy(source, folder):
        target_path = os.path.join(folder, os.path.basename(source))
        if os.path.isfile(target_path):
            with open(target_path, "rb") as a, open(source, "rb") as b:
                if a.read() != b.read():
                    raise SystemExit("build.py: two different %s" % os.path.basename(source))
            return
        shutil.copy2(source, target_path)

    def absolute(path):
        return path if os.path.isabs(path) else os.path.join(build_dir, path)

    def contents_of(folder):
        """a -L folder of the SDK: its linker scripts and its archives (the closed libraries taken by -l)"""
        for name in os.listdir(folder):
            if name.endswith(".ld"):
                copy(os.path.join(folder, name), ld_dir)
            elif name.endswith(".a"):
                copy(os.path.join(folder, name), lib_dir)

    out = ["-L${PKG}/%s/ld" % kind, "-L${PKG}/%s/lib" % kind]
    tokens = (shlex.split(values.get("LINK_FLAGS", "").replace("\\", "/"), posix=True)
              + shlex.split(values.get("LINK_LIBRARIES", "").replace("\\", "/"), posix=True))
    pending = None
    for token in tokens:
        if pending:
            if pending == "-L":
                token = "-L" + token
            elif pending == "-T":
                out += ["-T", os.path.basename(token)]
                if os.path.isfile(absolute(token)):
                    copy(absolute(token), ld_dir)
                pending = None
                continue
            else:
                out += [pending, token]
                pending = None
                continue
            pending = None
        if token in ("-L", "-T", "-u"):
            pending = token
            continue
        if token.startswith("-L"):
            folder = absolute(token[2:])
            mapped, _ = package.where(folder)
            if mapped.startswith("${LX106}"):
                out.append("-L" + mapped)
            else:
                contents_of(folder)
            continue
        if token.startswith("-T"):
            out += ["-T", os.path.basename(token[2:])]
            if os.path.isfile(absolute(token[2:])):
                copy(absolute(token[2:]), ld_dir)
            continue
        if token.startswith("-Wl,--Map") or token.startswith("-Wl,-Map") or token == "-Wl,--cref":
            continue
        if token.endswith(".a"):
            if any(posix(absolute(token)).endswith(skip) for skip in skip_archives):
                continue
            copy(absolute(token), lib_dir)
            out.append("${PKG}/%s/lib/%s" % (kind, os.path.basename(token)))
            continue
        if "/" in token and os.path.exists(absolute(token)):
            raise SystemExit("build.py: a path in the link of %s: %s" % (target, token))
        out.append(token)
    return out


def build(args):
    work = os.path.abspath(args.work)
    project = os.path.join(work, "project")
    build_dir = os.path.join(project, "build")
    out = os.path.abspath(args.out)
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)

    write(os.path.join(project, "CMakeLists.txt"), "\n".join([
        "cmake_minimum_required(VERSION 3.5)",
        "include($ENV{IDF_PATH}/tools/cmake/project.cmake)",
        "project(espurna_sdk)",
        "",
    ]))
    write(os.path.join(project, "main", "CMakeLists.txt"), 'idf_component_register(SRCS "sdk_main.c")\n')
    # only for the SDK's own link of this project: the firmware has its own app_main and never links this libmain.a
    write(os.path.join(project, "main", "sdk_main.c"), "void __attribute__((weak)) app_main(void) {\n}\n")

    env = dict(os.environ)
    env["IDF_PATH"] = posix(SDK)
    env["FREERTOS_KERNEL_DIR"] = posix(args.kernel)
    env["MBEDTLS_DIR"] = posix(args.mbedtls)
    env["XTENSA_GNU_CONFIG"] = os.path.join(os.path.abspath(args.toolchain), "lib", "xtensa_esp8266.so")
    env["ESPURNA_LX106_LIBS"] = posix(args.lx106)
    env["PATH"] = os.pathsep.join([os.path.join(os.path.abspath(args.toolchain), "bin")]
                                  + [os.path.dirname(os.path.abspath(tool)) for tool in (args.cmake, args.ninja)
                                     if os.path.dirname(tool)]
                                  + [env.get("PATH", "")])

    sdkconfig = os.path.join(project, "sdkconfig")
    if os.path.isfile(sdkconfig):
        os.remove(sdkconfig)
    run([args.cmake, "-G", "Ninja", "-S", project, "-B", build_dir, "-Wno-dev",
         "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
         "-DCMAKE_MAKE_PROGRAM=" + posix(shutil.which(args.ninja, path=env["PATH"]) or args.ninja),
         "-DPYTHON_DEPS_CHECKED=1",
         "-DNATIVE_GCC=none",
         "-DPYTHON=" + posix(sys.executable),
         "-DSDKCONFIG_DEFAULTS=" + posix(os.path.join(HERE, "sdkconfig.defaults")),
         "-DFREERTOS_KERNEL_DIR=" + posix(args.kernel),
         "-DMBEDTLS_DIR=" + posix(args.mbedtls),
         "-DCOMPONENTS=" + ";".join(COMPONENTS)],
        project, env, "configure the SDK")
    jobs = str(args.jobs)
    run([args.ninja, "-C", build_dir, "-j", jobs, "-k", "0", "espurna_sdk.elf"], project, env, "build the SDK")
    run([args.ninja, "-C", build_dir, "-j", jobs, "bootloader"], project, env, "build the bootloader")

    package = Package(out, {SDK: "sdk", build_dir: "build", args.kernel: "kernel", args.mbedtls: "mbedtls"}, args.lx106)

    flags = {}
    includes, defines, cflags, cxxflags = compile_flags(
        package, os.path.join(build_dir, "compile_commands.json"), "main/sdk_main.c", "nvs_flash/src/nvs_api.cpp")
    flags["app"] = {
        "includes": includes, "defines": defines, "cflags": cflags, "cxxflags": cxxflags,
        "link": link_flags(package, build_dir, os.path.join(build_dir, "build.ninja"), "espurna_sdk.elf", "app",
                           skip_archives=("/main/libmain.a",)),
    }

    boot_dir = os.path.join(build_dir, "bootloader")
    includes, defines, cflags, _ = compile_flags(
        package, os.path.join(boot_dir, "compile_commands.json"), "main/bootloader_start.c", None)
    flags["bootloader"] = {
        "includes": includes, "defines": defines, "cflags": cflags, "cxxflags": [],
        "link": link_flags(package, boot_dir, os.path.join(boot_dir, "build.ninja"), "bootloader.elf", "bootloader"),
    }

    config = os.path.join(out, "config")
    os.makedirs(config)
    shutil.copy2(sdkconfig, config)
    for name in ("sdkconfig.h", "sdkconfig.json"):
        shutil.copy2(os.path.join(build_dir, "config", name), config)

    tools = os.path.join(out, "tools")
    shutil.copytree(os.path.join(SDK, "components", "esptool_py", "esptool"), os.path.join(tools, "esptool"),
                    ignore=shutil.ignore_patterns(".git*", "test", "docs"))
    shutil.copy2(os.path.join(SDK, "components", "partition_table", "gen_esp32part.py"), tools)

    def version_of(folder):
        try:
            with open(os.path.join(folder, "package.json"), encoding="utf-8") as f:
                return json.load(f)["version"]
        except (OSError, KeyError, ValueError):
            return "?"

    gcc = subprocess.run([os.path.join(os.path.abspath(args.toolchain), "bin", "xtensa-esp-elf-gcc"), "-dumpversion"],
                         stdout=subprocess.PIPE, universal_newlines=True, env=env).stdout.strip()
    flags["built"] = {
        "sdk": os.environ.get("GITHUB_SHA") or subprocess.run(["git", "-C", SDK, "rev-parse", "HEAD"],
                                                               stdout=subprocess.PIPE,
                                                               universal_newlines=True).stdout.strip(),
        "gcc": gcc,
        "lx106": version_of(args.lx106),
        "freertos_kernel": version_of(args.kernel),
        "mbedtls": version_of(args.mbedtls),
        "components": COMPONENTS,
    }
    write(os.path.join(out, "flags.json"), json.dumps(flags, indent=1) + "\n")
    write(os.path.join(out, "package.json"), json.dumps({
        "name": "framework-esp8266-rtos-sdk",
        "version": args.version,
        "description": "ESP8266_RTOS_SDK compiled for ESPurna (tools/espurna/build.py): archives, headers, sdkconfig, "
                       "linker scripts and the flags of the app and of the bootloader; GCC %s, FreeRTOS-Kernel %s, "
                       "Mbed TLS %s" % (gcc, flags["built"]["freertos_kernel"], flags["built"]["mbedtls"]),
        "license": "Apache-2.0",
        "repository": {"type": "git", "url": "https://github.com/igorufox/ESP8266_RTOS_SDK"},
    }, indent=2) + "\n")

    if args.test:
        test(args, out, env)

    archive = out.rstrip("/\\") + ".zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for folder, _, files in os.walk(out):
            for name in files:
                path = os.path.join(folder, name)
                z.write(path, os.path.relpath(path, out).replace(os.sep, "/"))
    print("build.py: %s" % archive)


def expand(tokens, out, lx106):
    return [token.replace("${PKG}", posix(out)).replace("${LX106}", posix(lx106)) for token in tokens]


def test(args, out, env):
    """A firmware's link from the package alone: an app with its own app_main, a bootloader with its own hook"""
    with open(os.path.join(out, "flags.json"), encoding="utf-8") as f:
        flags = json.load(f)
    gcc = os.path.join(os.path.abspath(args.toolchain), "bin", "xtensa-esp-elf-gcc")
    work = os.path.join(os.path.abspath(args.work), "test")
    os.makedirs(work, exist_ok=True)
    sources = {
        "app": 'void app_main(void) {\n}\n',
        "bootloader": '#include "bootloader_hooks.h"\nvolatile int hooked;\nvoid bootloader_after_init(void) {\n'
                      '    hooked = 1;\n}\n',
    }
    for kind, text in sources.items():
        image = flags[kind]
        source = os.path.join(work, kind + ".c")
        write(source, text)
        obj = source + ".o"
        run([gcc] + expand(image["cflags"], out, args.lx106)
            + ["-D" + define for define in image["defines"]]
            + ["-I" + path for path in expand(image["includes"], out, args.lx106)]
            + ["-c", source, "-o", obj], work, env, "test: compile the %s" % kind)
        elf = os.path.join(work, kind + ".elf")
        run([gcc, "-nostdlib", "-Wl,--gc-sections", "-mlongcalls", "-o", elf, obj]
            + expand(image["link"], out, args.lx106), work, env, "test: link the %s" % kind)
        print("build.py: test %s %s, %d bytes" % (kind, elf, os.path.getsize(elf)))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--toolchain", required=True)
    parser.add_argument("--lx106", required=True)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--mbedtls", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--work", default=os.path.join(SDK, "build-espurna"))
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ninja", default="ninja")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--test", action="store_true")
    build(parser.parse_args())


if __name__ == "__main__":
    main()
