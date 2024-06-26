/// @module system

/// Random.
// @section random


#include <lua.h>
#include <lauxlib.h>
#include "compat.h"
#include <fcntl.h>

#ifdef _WIN32
#include "windows.h"
#include "wincrypt.h"
#else
#include <errno.h>
#include <unistd.h>
#include <string.h>
#endif


/***
Generate random bytes.
This uses `CryptGenRandom()` on Windows, and `/dev/urandom` on other platforms. It will return the
requested number of bytes, or an error, never a partial result.
@function random
@tparam[opt=1] int length number of bytes to get
@treturn[1] string string of random bytes
@treturn[2] nil
@treturn[2] string error message
*/
static int lua_get_random_bytes(lua_State* L) {
    int num_bytes = luaL_optinteger(L, 1, 1); // Number of bytes, default to 1 if not provided

    if (num_bytes <= 0) {
        if (num_bytes == 0) {
            lua_pushliteral(L, "");
            return 1;
        }
        lua_pushnil(L);
        lua_pushstring(L, "invalid number of bytes, must not be less than 0");
        return 2;
    }

    unsigned char* buffer = (unsigned char*)lua_newuserdata(L, num_bytes);
    if (buffer == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "failed to allocate memory for random buffer");
        return 2;
    }

    ssize_t n;
    ssize_t total_read = 0;

#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        DWORD error = GetLastError();
        lua_pushnil(L);
        lua_pushfstring(L, "failed to acquire cryptographic context: %lu", error);
        return 2;
    }

    if (!CryptGenRandom(hCryptProv, num_bytes, buffer)) {
        DWORD error = GetLastError();
        lua_pushnil(L);
        lua_pushfstring(L, "failed to get random data: %lu", error);
        CryptReleaseContext(hCryptProv, 0);
        return 2;
    }

    CryptReleaseContext(hCryptProv, 0);
#else

    // for macOS/unixes use /dev/urandom for non-blocking
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "failed opening /dev/urandom");
        return 2;
    }

    while (total_read < num_bytes) {
        n = read(fd, buffer + total_read, num_bytes - total_read);

        if (n < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted, retry

            } else {
                lua_pushnil(L);
                lua_pushfstring(L, "failed reading /dev/urandom: %s", strerror(errno));
                close(fd);
                return 2;
            }
        }

        total_read += n;
    }

    close(fd);
#endif

    lua_pushlstring(L, (const char*)buffer, num_bytes);
    return 1;
}



static luaL_Reg func[] = {
    { "random", lua_get_random_bytes },
    { NULL, NULL }
};



/*-------------------------------------------------------------------------
 * Initializes module
 *-------------------------------------------------------------------------*/
void random_open(lua_State *L) {
    luaL_setfuncs(L, func, 0);
}
