# Sandboxie Plus 文件恢复开关

独立于 WinPrivacyGreen 的小工具：一键关闭 [Sandboxie Plus](https://github.com/sandboxie-plus/Sandboxie/releases) 的全局「Disable File Recovery / 禁用文件恢复」提示，并可恢复修改前的设置。Windows 10/11 x64，原生单文件，不安装服务、不联网。

## 下载和使用

1. 在 [Sandboxie 文件恢复开关发布页](https://github.com/elysianki4225-png/WinPrivacyGreen/releases/tag/sandboxie-recovery-v1.0.0) 下载 `SandboxieRecoverySwitch.exe`。这是单独的工具，不是 Sandboxie 安装包；请先安装并启动过 Sandboxie Plus。
2. **先保存沙盒内的工作**，再从系统托盘的 Sandboxie 图标菜单选择「退出」；仅点窗口右上角可能只是最小化到托盘。如果 Sandboxie 配置了「退出时结束沙盒进程」，退出管理器也可能结束这些进程。
3. 在**沙盒外**双击工具。通常会自动找到 `SandMan.exe` 和实际配置；如果没有找到，点击「选择程序」选择 Sandboxie Plus 安装目录或便携目录里的 `SandMan.exe`，检查界面显示的配置路径。
4. 点击「一键关闭文件恢复」，然后重新启动 Sandboxie Plus。工具不会强制终止进程；若管理器仍在运行或配置不可写，会停止并提示，不会声称成功。配置位于受保护目录时，可能需要右键「以管理员身份运行」。若使用**另一个管理员账户**提权，用户配置目录可能不同，请核对界面显示的路径。
5. 撤销时同样先彻底退出管理器，在工具中点击「恢复之前设置」，然后重启 Sandboxie Plus。

### 重要说明

- 这里关闭的是 Sandboxie **自动/即时文件恢复提示**，对应 Plus 高级视图菜单中的全局选项；它不是禁止文件被拷出沙盒的安全策略。**手动快速恢复、手动拷贝及清空沙盒时的恢复流程仍可能使用。**工具不会修改 `Sandboxie.ini`、逐个沙盒的 `AutoRecover` 或 `RecoverFolder`，也不修改隔离策略。
- 文件仍留在沙盒中；**清空或自动删除沙盒可能连同文件一起删除**。关闭提示前请手动保存重要文件。
- Sandboxie Plus 的简洁视图和经典风格视图不会读取这个全局菜单开关。为确保生效，工具必要时会把管理器切换到**高级视图**；恢复操作会尽量还原原视图。如果后来手动切换了视图，恢复时会保留用户的新选择。关闭后若切回其他视图，自动恢复提示可能重新出现。
- 仅支持 **Sandboxie Plus 的 SandMan.exe**，不支持 Sandboxie Classic 的 `SbieCtrl.exe`。配置定位和行为按 [Sandboxie Plus v1.18.8 源码](https://github.com/sandboxie-plus/Sandboxie/tree/v1.18.8)设计：优先识别便携版 `PlusData`/程序目录，然后是共享配置和当前用户配置。检测到需要迁移的旧版配置时，会要求先启动新版一次，不会猜测配置位置。
- 首次实际修改前，会在配置旁创建完整备份 `Sandboxie-Plus.ini.recovery-switch.bak`；重复点击不会覆盖它。备份可能包含和原配置相同的个人设置，请妥善保管。恢复时只还原此工具涉及的开关及必要的视图设置，保留之后改过的其他选项；成功后删除该备份。所有操作都在本机进行。
- 程序是未签名的独立 exe；Windows SmartScreen 可能提示未知发布者。请从本项目发布页下载，并可使用发布页中的 SHA-256 文件校验。

## 命令行（可选）

在管理器已退出时，可以在终端里使用：

```powershell
.\SandboxieRecoverySwitch.exe --disable --sandman "C:\Program Files\Sandboxie-Plus\SandMan.exe" --quiet
.\SandboxieRecoverySwitch.exe --restore --sandman "C:\Program Files\Sandboxie-Plus\SandMan.exe" --quiet
```

省略 `--sandman` 时会自动检测；省略 `--quiet` 会显示结果对话框。退出码：`0` 成功（包括本来已经关闭），`2` 路径/配置/操作锁错误，`3` 管理器仍在运行或无法检查，`4` 读写/备份错误，`5` 没有备份，`6` 配置格式不支持，`64` 参数错误。

## 自行构建与测试

在 Windows 上从仓库根目录运行：

```powershell
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
# 程序：build/sandboxie-recovery/Release/SandboxieRecoverySwitch.exe
```

Windows 集成及 GUI 测试（使用临时配置和无害的假管理器，不需要安装 Sandboxie）：

```powershell
./sandboxie-recovery/tests/integration.ps1 `
  -Exe build/sandboxie-recovery/Release/SandboxieRecoverySwitch.exe `
  -FakeManager build/sandboxie-recovery/Release/SandboxieRecoveryFakeManager.exe
```

Linux/macOS 可以使用 Zig 0.14+ 交叉编译：

```sh
ZIG=/path/to/zig ./sandboxie-recovery/build.sh
# 程序：build/SandboxieRecoverySwitch.exe
```

INI 编辑核心也可在 Linux 上直接测试：

```sh
c++ -std=c++17 -I sandboxie-recovery/src sandboxie-recovery/src/ini.cpp \
  sandboxie-recovery/tests/ini_tests.cpp -o /tmp/recovery-tests && /tmp/recovery-tests
```

本项目与 Sandboxie 官方无关联。实现依据：上游 [SandMan 菜单及配置](https://github.com/sandboxie-plus/Sandboxie/blob/v1.18.8/SandboxiePlus/SandMan/SandMan.cpp)、[恢复处理](https://github.com/sandboxie-plus/Sandboxie/blob/v1.18.8/SandboxiePlus/SandMan/SandManRecovery.cpp)和 [配置位置](https://github.com/sandboxie-plus/Sandboxie/blob/v1.18.8/SandboxiePlus/MiscHelpers/Common/Settings.cpp)。
