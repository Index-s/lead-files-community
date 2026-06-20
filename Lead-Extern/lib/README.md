# Lead-Extern/lib — x64 server + client dependency libraries

These are the **64-bit** builds of the third-party libraries the x64 server **and
client** link against. The old 32-bit `win32`/`win64` split was flattened during
the x64 port: `lib/` now holds the x64 libs directly (the 32-bit versions were
removed), and the x64 projects reference this directory in
`AdditionalLibraryDirectories`.

The binaries themselves are **not committed** (large / rebuildable) — see
`.gitignore`. Rebuild them with the steps below. All built with VS 2026 (v145
toolset), x64, Debug, `/MTd` runtime (to match the server's MultiThreadedDebug)
except DevIL which is a DLL (CRT-isolated).

| File | Source | How it was built |
|---|---|---|
| `cryptlib-Debug.lib` | in-repo `Lead-Extern/sources/cryptopp` | `msbuild cryptlib.vcxproj /p:Configuration=Debug /p:Platform=x64` (after the `integer.cpp`/`zdeflate.cpp` patch that drops `stdext::make_*checked_array_iterator`, removed from modern MSVC STL). Output copied to `cryptlib-Debug.lib`. |
| `lzo-2.10MT_d.lib` | lzo 2.10 (oberhumer.com) | `cl /c /MTd /O2 /I include src\*.c` then `lib /out:lzo-2.10MT_d.lib *.obj` in an x64 dev shell. |
| `mysqlclient.lib` | MariaDB Connector/C **3.3.8** (matches the bundled `include/mysql` headers) | `cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DWITH_SSL=SCHANNEL -DWITH_UNIT_TESTS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_POLICY_VERSION_MINIMUM=3.5` then `cmake --build . --target mariadbclient`; `mariadbclient.lib` copied to `mysqlclient.lib`. Schannel backend ⇒ the server links `crypt32.lib`/`bcrypt.lib`. |
| `DevIL.lib` + `DevIL.dll` | DevIL master (github DentonW/DevIL) | `cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DIL_TESTS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug -DJPEG_INCLUDE_DIR=<extern>/include/libjpeg -DJPEG_LIBRARY=<extern>/lib/libjpeg-9fMT_d.lib` then `cmake --build . --target IL` (comment out `add_subdirectory(src-ILUT)` in `DevIL/CMakeLists.txt` — pulls in GLUT, unused). **JPEG backend is wired in** so guild-mark upload accepts `.jpg`: the client decodes the picked image via `ilLoad` and ships compressed BGRA pixel blocks; the server only ever `ilSave`/`ilLoad`s `IL_TGA` mark blocks, so its DevIL needs no external codecs. Without JPEG, `ilLoad` of a `.jpg` returns `IL_INVALID_ENUM` (0x501) ⇒ in-game "The game does not support this picture". `DevIL.dll` must sit next to the exes at runtime. |

## x64 client-only dependency libs

The x64 client (`metin2client_debug.exe`) links these in addition to the shared
ones above. Built/fetched VS 2026 (v145), x64, `/MTd` to match the client.

| File | Source | How obtained |
|---|---|---|
| `d3dx9.lib` (+ `d3dx9d.lib`) | `Microsoft.DXSDK.D3DX` NuGet (Walbourn's repackage of the DXSDK June 2010 D3DX) | `nuget`/REST fetch the nupkg, copy `build/native/release/lib/x64/d3dx9.lib`. The legacy DXSDK installer itself fails headlessly (S1023 redist conflict) — the NuGet avoids it. Runtime: `d3dx9_43.dll` next to the exe. |
| `d3d9.lib` | Windows 10 SDK | copy `…\Windows Kits\10\Lib\<ver>\um\x64\d3d9.lib` here so it wins over the Win32 `lib/d3d9.lib` on the search path (needed for `Direct3DCreate9Ex`). |
| `python27.lib` (+ headers) | Python 2.7.18 amd64 (python.org) | `msiexec /a python-2.7.18.amd64.msi /qn TARGETDIR=…`; copy `libs/python27.lib`. ABI-compatible with the repo's vendored 2.7 headers — do NOT overwrite `Lead-Extern/include/python` (its 2.7.18 copy uses `register` ⇒ C5033). Runtime: `python27.dll`. |
| `mss64.lib` | Miles Sound System 9.3 SDK (`Miles-9.3/win/sdk/MilesSDKWin.rar`) | extract `lib/mss64.lib`. Header `mss.h`+`rrcore.h` replace the Miles-6 header in `Lead-Extern/include/miles`. MilesLib was ported to the 9.3 API. Runtime: `mss64.dll`. |
| `granny2_x64.lib` | `Granny_Common_2_11_8_0_Release/lib/win64` | copy. Client pragma is `_WIN64`-conditional (`granny2_x64.lib`). Runtime: `granny2_x64.dll`. |
| `WebView2Loader.lib` | `Microsoft.Web.WebView2` NuGet | copy `build/native/x64/WebView2Loader.dll.lib` → `WebView2Loader.lib`. Runtime: `WebView2Loader.dll`. |
| `SpeedTreeRT.lib` | `SpeedTreeRT_SRC_1.6.zip` (from `SpeedTreeRT.rar`) | added Debug\|x64 to `SpeedTreeRT.vcxproj` (v145, `/Zc:strictStrings-`, `_HAS_STD_BYTE=0`); removed the unused `LoadTree(KStream*)` overload (client-internal dep not in this drop) and matched `SetNumWindMatrices(unsigned int)` to the vendored public header. |
| `libjpeg-9fMT_d.lib` | IJG libjpeg 9f (`ijg.org/files/jpegsr9f.zip`) | `jconfig.vc`→`jconfig.h`, then `cl /c /MTd` the `j*.c` core (minus `jpegtran.c` + the alternate `jmem*` managers, keep `jmemnobs`) and `lib` into this exact name (the `jpegLibLink.h` pragma builds `libjpeg-9f` + runtime-model + `_d`). |

Runtime DLLs are staged next to the exe in `Lead-Client/`: granny2_x64, mss64,
python27, WebView2Loader, D3DX9_43, DevIL.
