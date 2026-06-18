# Lead-Extern/lib/win64 — x64 server dependency libraries

These are the **64-bit** builds of the third-party libraries the x64 server links
against (the `lib/` directory holds the 32-bit versions, still used by the
not-yet-ported Win32 client). The server's x64 projects prepend this directory to
`AdditionalLibraryDirectories`, so these take precedence over the 32-bit libs.

The binaries themselves are **not committed** (large / rebuildable) — see
`.gitignore`. Rebuild them with the steps below. All built with VS 2026 (v145
toolset), x64, Debug, `/MTd` runtime (to match the server's MultiThreadedDebug)
except DevIL which is a DLL (CRT-isolated).

| File | Source | How it was built |
|---|---|---|
| `cryptlib-Debug.lib` | in-repo `Lead-Extern/sources/cryptopp` | `msbuild cryptlib.vcxproj /p:Configuration=Debug /p:Platform=x64` (after the `integer.cpp`/`zdeflate.cpp` patch that drops `stdext::make_*checked_array_iterator`, removed from modern MSVC STL). Output copied to `cryptlib-Debug.lib`. |
| `lzo-2.10MT_d.lib` | lzo 2.10 (oberhumer.com) | `cl /c /MTd /O2 /I include src\*.c` then `lib /out:lzo-2.10MT_d.lib *.obj` in an x64 dev shell. |
| `mysqlclient.lib` | MariaDB Connector/C **3.3.8** (matches the bundled `include/mysql` headers) | `cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DWITH_SSL=SCHANNEL -DWITH_UNIT_TESTS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_POLICY_VERSION_MINIMUM=3.5` then `cmake --build . --target mariadbclient`; `mariadbclient.lib` copied to `mysqlclient.lib`. Schannel backend ⇒ the server links `crypt32.lib`/`bcrypt.lib`. |
| `DevIL.lib` + `DevIL.dll` | DevIL 1.8.0 (github DentonW/DevIL) | `cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5` then `cmake --build . --target IL`. Built with DevIL's built-in formats (TGA/BMP for guild marks); external png/jpeg/tiff backends not required. `DevIL.dll` must sit next to the server exes at runtime. |
