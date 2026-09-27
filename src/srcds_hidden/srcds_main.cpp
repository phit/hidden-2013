//========= Hidden: Source =====================================================//
//
// Purpose: The 64-bit dedicated server launcher (srcds_win64.exe, srcds_linux64).
//			SteamCMD's Source SDK Base 2013 Dedicated Server (app 244310) has the
//			64-bit engine in bin/x64 and bin/linux64 but only 32-bit launchers,
//			which can't load the mod. This does what they do: put the engine's
//			bin folder on the library path, load the dedicated server library
//			and call its DedicatedMain. It goes in the server's folder, next to
//			bin/, and its folder is the server's root.
//
//			Built on its own, without tier0: the libraries it loads bring the
//			server's. See tools/build.ps1 and src/buildhidden.
//
//=============================================================================//

#ifdef _WIN32

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef int ( *DedicatedMain_t )( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow );

static void Fail( const char *pszMessage, const char *pszDetail )
{
	char szText[2048];
	_snprintf_s( szText, sizeof( szText ), _TRUNCATE, "%s\n\n%s", pszMessage, pszDetail );
	MessageBoxA( NULL, szText, "Hidden: Rebuild dedicated server", MB_OK | MB_ICONERROR );
}

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// The server's root is this executable's folder.
	char szRoot[MAX_PATH];
	if ( !GetModuleFileNameA( NULL, szRoot, sizeof( szRoot ) ) )
		return 1;
	char *pszSlash = strrchr( szRoot, '\\' );
	if ( pszSlash )
		*pszSlash = '\0';

	// The engine's libraries load each other from bin\x64.
	char szPath[32768];
	const char *pszOldPath = getenv( "PATH" );
	_snprintf_s( szPath, sizeof( szPath ), _TRUNCATE, "PATH=%s\\bin\\x64;%s", szRoot, pszOldPath ? pszOldPath : "" );
	_putenv( szPath );

	char szDedicated[MAX_PATH];
	_snprintf_s( szDedicated, sizeof( szDedicated ), _TRUNCATE, "%s\\bin\\x64\\dedicated.dll", szRoot );
	HMODULE hDedicated = LoadLibraryExA( szDedicated, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
	if ( !hDedicated )
	{
		Fail( "Couldn't load bin\\x64\\dedicated.dll. Put srcds_win64.exe in the folder of Source SDK Base 2013 "
			  "Dedicated Server (SteamCMD app 244310), next to its bin folder.", szDedicated );
		return 1;
	}

	DedicatedMain_t pDedicatedMain = (DedicatedMain_t)GetProcAddress( hDedicated, "DedicatedMain" );
	if ( !pDedicatedMain )
	{
		Fail( "bin\\x64\\dedicated.dll has no DedicatedMain.", szDedicated );
		return 1;
	}

	int nResult = pDedicatedMain( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
	FreeLibrary( hDedicated );
	return nResult;
}

#else // POSIX

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef int ( *DedicatedMain_t )( int argc, char **argv );

int main( int argc, char **argv )
{
	// The server's root is this executable's folder.
	char szRoot[PATH_MAX];
	ssize_t nLen = readlink( "/proc/self/exe", szRoot, sizeof( szRoot ) - 1 );
	if ( nLen <= 0 )
	{
		perror( "readlink /proc/self/exe" );
		return 1;
	}
	szRoot[nLen] = '\0';
	char *pszSlash = strrchr( szRoot, '/' );
	if ( pszSlash )
		*pszSlash = '\0';

	// The engine's libraries load each other (and libsteam_api.so) from bin/linux64, which has to be
	// on the library path before the process starts, so set it and start again.
	char szLibDir[PATH_MAX + 16];
	snprintf( szLibDir, sizeof( szLibDir ), "%s/bin/linux64", szRoot );
	const char *pszLibPath = getenv( "LD_LIBRARY_PATH" );
	if ( !pszLibPath || strncmp( pszLibPath, szLibDir, strlen( szLibDir ) ) != 0 )
	{
		char szNewPath[PATH_MAX * 4];
		snprintf( szNewPath, sizeof( szNewPath ), "%s%s%s", szLibDir, pszLibPath ? ":" : "", pszLibPath ? pszLibPath : "" );
		setenv( "LD_LIBRARY_PATH", szNewPath, 1 );
		execv( "/proc/self/exe", argv );
		perror( "execv" );
		return 1;
	}

	// Like SteamCMD's srcds_run, run from the server's folder.
	if ( chdir( szRoot ) != 0 )
	{
		perror( szRoot );
		return 1;
	}

	static const char *s_pszLibs[] = { "libtier0_srv.so", "libvstdlib_srv.so", "dedicated_srv.so" };
	void *hDedicated = NULL;
	for ( size_t i = 0; i < sizeof( s_pszLibs ) / sizeof( s_pszLibs[0] ); i++ )
	{
		char szLib[PATH_MAX + 64];
		snprintf( szLib, sizeof( szLib ), "%s/%s", szLibDir, s_pszLibs[i] );
		hDedicated = dlopen( szLib, RTLD_NOW | RTLD_GLOBAL );
		if ( !hDedicated )
		{
			fprintf( stderr, "Failed to open %s (%s)\n"
					 "Put srcds_linux64 in the folder of Source SDK Base 2013 Dedicated Server (SteamCMD app 244310), "
					 "next to its bin folder.\n", szLib, dlerror() );
			return 1;
		}
	}

	DedicatedMain_t pDedicatedMain = (DedicatedMain_t)dlsym( hDedicated, "DedicatedMain" );
	if ( !pDedicatedMain )
	{
		fprintf( stderr, "Failed to find dedicated server entry point (%s)\n", dlerror() );
		return 1;
	}

	int nResult = pDedicatedMain( argc, argv );
	dlclose( hDedicated );
	return nResult;
}

#endif
