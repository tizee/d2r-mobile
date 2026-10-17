#!/bin/bash
# 自己装机用的开发包（不签名，交给 Sideloadly / AltStore 用自己的 Apple ID 签）：在当前源码树里增量构建，直接出 .ipa。
# 与「壳 + d2rm_pack.py」那条路（给没有 Xcode 的玩家）不同：这里本机有 Xcode 也有游戏文件，模块映像照开发包内嵌进可执行文件，
# 不换替身、不拆动态库、不重复生成覆盖层；DerivedData、Gen/assets、覆盖层缓存都留着，改几个源文件后约一分钟出包。
# 用法：dm/ios/tools/dev_ipa.sh        产物：$DM_ROOT/build/D2RMobile-dev.ipa（日志在 $DM_ROOT/build/dev/）
# 按免费账号的权限出包（不带 iCloud 等签不了的权限、不带资源下载扩展），与壳一致。这个包里有游戏内容，只给自己用。
# Development package for your own device (unsigned; sign it with your own Apple ID in Sideloadly / AltStore): built incrementally in the current source tree, straight to an .ipa.
# Unlike the "shell + d2rm_pack.py" path (for players without Xcode), this machine has Xcode and the game files, so the module images are embedded in the executable as in the dev build:
# no stand-ins, no dylib split, no second overlay generation; DerivedData, Gen/assets and the overlay cache are all kept, so after changing a few source files a package comes out in about a minute.
# Usage: dm/ios/tools/dev_ipa.sh        Output: $DM_ROOT/build/D2RMobile-dev.ipa (logs in $DM_ROOT/build/dev/)
# Built with free-account entitlements (none of the entitlements a free account cannot sign, no asset download extension), same as the shell. This package contains game content and is for your own use only.
set -u
cd "$(dirname "$0")/.." || exit 1   # dm/ios
ROOT=${DM_ROOT:-$HOME/dm-work}; O=$ROOT/build/dev; DD=$O/dd; IPA=$ROOT/build/D2RMobile-dev.ipa
t() { echo "[$(date +%T)] $*"; }; die() { t "🔴 $*"; exit 1; }
mkdir -p "$O" || die "建不了 $O"
# 包名 / 团队号只是占位：不签名，自签工具装机时换成使用者自己的 | Bundle ID / team ID are placeholders: unsigned, the sideloading tool replaces them at install time
printf 'DM_TEAM_ID=0000000000\nDM_BUNDLE_ID=org.d2rmobile.app\nDM_FREE_ACCOUNT=1\n' > "$O/dev.env"
export DM_LOCAL_ENV=$O/dev.env DM_FREE_ACCOUNT=1 DM_ROOT=$ROOT
T0=$SECONDS
t "生成"; DM_GEN_ONLY=1 DM_IOS_DD="$DD" bash run.sh build > "$O/gen.log" 2>&1 || { tail -5 "$O/gen.log"; die "生成失败，日志 $O/gen.log"; }
grep -E "覆盖层：" "$O/gen.log" | tail -1
T1=$SECONDS; t "构建（不签名，增量）"
BN=$(git log -1 --format=%ct 2>/dev/null); [ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ] && BN=1   # 同 run.sh：有未提交改动 = 构建号 1 | same as run.sh: uncommitted changes = build number 1
xcodebuild -project DmIOS.xcodeproj -scheme DmIOS -configuration Release -destination 'generic/platform=iOS' -derivedDataPath "$DD" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY= DM_DEVICE_FAMILY=1,2 DM_BUILD_NUMBER=${BN:-1} build > "$O/build.log" 2>&1 \
  || { grep -E 'error:' "$O/build.log" | head -5; die "构建失败，日志 $O/build.log"; }
NC=$(grep -c '^CompileC' "$O/build.log"); T2=$SECONDS
APP=$(ls -d "$DD"/Build/Products/Release-iphoneos/DmIOS.app 2>/dev/null | head -1); [ -d "$APP" ] || die "没找到构建出的 App"
t "打包（编译了 $NC 个文件）"
rm -rf "$O/Payload" && mkdir -p "$O/Payload" && cp -cR "$APP" "$O/Payload/" || die "拷 App 失败"
A=$O/Payload/DmIOS.app
rm -rf "$A/PlugIns" "$A/Extensions" "$A/_CodeSignature" "$A/embedded.mobileprovision"   # 资源下载扩展与残留签名：自签时不要 | asset download extension and leftover signatures: not wanted for sideloading
for k in BAAppGroupID BAHasManagedAssetPacks BAUsesAppleHosting; do /usr/libexec/PlistBuddy -c "Delete :$k" "$A/Info.plist" 2>/dev/null; done
rm -f "$IPA.tmp" && (cd "$O" && zip -qr -1 "$IPA.tmp" Payload) && mv "$IPA.tmp" "$IPA" || die "写不出 $IPA"
t "好了：${IPA}（$(du -h "$IPA" | cut -f1)；生成 $((T1-T0)) 秒、编译 $((T2-T1)) 秒、打包 $((SECONDS-T2)) 秒，共 $((SECONDS-T0)) 秒）。这个包里有游戏内容，只给自己用"
