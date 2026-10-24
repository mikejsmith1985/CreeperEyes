# Stamps the firmware with a version so the phone page can show (and verify) what's installed.
#   FW_VERSION = 1.<number of commits>[+] ("+" = built with uncommitted changes)
#   FW_GIT     = short commit hash, FW_EPOCH = build time (UTC seconds; the phone shows it in local time)
# After a build, also drops creepereyes-v<version>.bin next to firmware.bin.
import subprocess, time, shutil, os
Import("env")

def git(*a):
    try: return subprocess.check_output(("git",) + a, cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL).decode().strip()
    except Exception: return ""

count = git("rev-list", "--count", "HEAD") or "0"
dirty = "+" if git("status", "--porcelain", "--untracked-files=no") else ""
version = "1.%s%s" % (count, dirty)
sha = git("rev-parse", "--short", "HEAD") or "local"
epoch = int(time.time())
env.Append(CPPDEFINES=[("FW_VERSION", '\\"%s\\"' % version), ("FW_GIT", '\\"%s\\"' % sha), ("FW_EPOCH", epoch)])
print("CreeperEyes firmware version", version, sha)

def copy_named(source, target, env):
    src = str(target[0]); dst = os.path.join(os.path.dirname(src), "creepereyes-v%s.bin" % version.replace("+", "-dev"))
    shutil.copyfile(src, dst); print("Update file:", dst)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_named)
